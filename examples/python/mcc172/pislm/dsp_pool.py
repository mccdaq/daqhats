#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
Multi-core DSP pool for the PiSLM.

The level/band DSP is the monitor's only heavy computation, and it is pure
per-channel filtering, so it parallelizes cleanly by channel group. Threads
do not help -- ``scipy.signal.sosfilt`` holds the GIL, so a thread pool is
measurably *slower* than serial -- therefore this uses worker **processes**.

Data path (no pickling of sample blocks):

    main process                          worker process
    ------------                          --------------
    slice the device block   -> shared memory slot
    queue (slot, n_frames,   -> Queue     -> read slot as a numpy view
           gap_frames)                       filter / time-weight
    broadcast frames         <- Queue     <- return framed bytes + free slot

Each worker owns the filter state (weighting SOS + zi, ExpLevel
integrators, band bank) for its channels, so no state crosses the process
boundary after start-up. Only the small output frames travel back.

Workers are assigned channel groups from a single device (blocks arrive
per device), balanced so every worker gets a similar number of channels.

Backpressure: if a worker falls behind (no free shared-memory slot),
DspPool.submit() drops the block rather than blocking the acquisition
loop. The dropped frame count travels as gap_frames on the worker's next
successful task, so it can skip() its integrators across the gap (see
_WorkerState.process) instead of silently splicing pre-gap and post-gap
samples together through stale filter state -- which would otherwise
look like a real, spurious transient (most visible in narrow, high-Q
band filters) and would defeat the point of start_index gap detection.
"""
from __future__ import print_function

import multiprocessing as mp
import os

import numpy as np

#: Shared-memory slots per worker (double buffering + slack).
SLOTS_PER_WORKER = 4


def plan_workers(device_channels, max_workers=None):
    """Split channels into balanced per-device worker groups.

    Args:
        device_channels: list, indexed by device, of that device's GLOBAL
            channel numbers in block-column order.
        max_workers: cap on worker count (None -> cpu_count() - 1). Blocks
            arrive per device, so a worker serves exactly one device and the
            effective minimum is one worker per device with channels.

    Returns:
        list of dicts: {'device', 'columns' (block column indices),
        'channels' (global numbers)} -- one per worker.
    """
    total = sum(len(c) for c in device_channels)
    if total == 0:
        return []
    if max_workers is None:
        max_workers = max(1, (os.cpu_count() or 2) - 1)
    max_workers = max(1, int(max_workers))

    # Never split a channel across workers; at most one worker per channel.
    n_workers = min(max_workers, total)
    # Distribute workers across devices proportionally to their channel count.
    quota = []
    for chans in device_channels:
        share = max(1, round(n_workers * len(chans) / total)) if chans else 0
        quota.append(min(share, len(chans)))
    # Trim/grow to land on n_workers without exceeding per-device channels.
    while sum(quota) > n_workers:
        i = max(range(len(quota)), key=lambda k: quota[k])
        if quota[i] <= 1:
            break
        quota[i] -= 1
    while sum(quota) < n_workers:
        candidates = [k for k in range(len(quota))
                      if quota[k] < len(device_channels[k])]
        if not candidates:
            break
        i = max(candidates,
                key=lambda k: len(device_channels[k]) / (quota[k] or 1))
        quota[i] += 1

    plan = []
    for dev_idx, chans in enumerate(device_channels):
        groups = quota[dev_idx]
        if not chans or groups <= 0:
            continue
        # Contiguous, as-even-as-possible split of this device's channels.
        base, extra = divmod(len(chans), groups)
        pos = 0
        for g in range(groups):
            size = base + (1 if g < extra else 0)
            if size == 0:
                continue
            cols = list(range(pos, pos + size))
            plan.append({'device': dev_idx, 'columns': cols,
                         'channels': [chans[c] for c in cols]})
            pos += size
    return plan


class _WorkerState:
    """Per-worker DSP state; built inside the worker process."""

    def __init__(self, cfg):
        import slm
        self.channels = cfg['channels']
        self.rate = cfg['rate']
        self.level_enabled = cfg['level_enabled']
        self.band_output = cfg['band_output']       # None|'level'|'waveform'
        self.refs = cfg['refs']                     # global chan -> reference

        tau = slm.tau_for(cfg['time_weighting'])
        self.sos = slm.design_weighting_sos(cfg['freq_weighting'], self.rate)
        n_sec = self.sos.shape[0] if self.sos is not None else 0
        self.zi = {c: np.zeros((n_sec, 2)) for c in self.channels}
        self.level = {c: slm.ExpLevel(self.rate, tau, cfg['level_rate'],
                                      ref=self.refs[c])
                      for c in self.channels} if self.level_enabled else {}

        self.bank = None
        self.band_level = {}
        self.band_offset = {}
        if self.band_output:
            from band_filter import BandFilterBank
            b = cfg['bands']
            self.bank = BandFilterBank(
                self.rate, self.channels, f_min=b['f_min'], f_max=b['f_max'],
                fraction=b['fraction'], order=b['order'], margin=b['margin'])
            if self.band_output == 'level':
                for band in self.bank.bands:
                    self.band_offset[band['index']] = slm.weighting_offset_db(
                        cfg['freq_weighting'], band['center'])
                    for c in self.channels:
                        self.band_level[(band['index'], c)] = slm.ExpLevel(
                            band['decimated_rate'], tau, cfg['level_rate'],
                            ref=self.refs[c])

    def band_metadata(self):
        return self.bank.metadata() if self.bank is not None else None

    def process(self, data, gap_frames=0):
        """Run the DSP on one block; return a list of (kind, args, payload).

        gap_frames is the count of INPUT samples dropped (by DSP-pool
        backpressure, see DspPool.submit) just before this block -- i.e.
        this worker never saw them at all. Rather than silently splicing
        this block onto stale pre-gap filter state (which would look like a
        real signal transition and can show up as a spurious spike,
        especially in the narrow/high-Q band filters), each affected
        integrator is skip()-ed across the gap first: that resets its state
        for a clean restart and returns how many OUTPUT samples the gap is
        worth in that grid, emitted here as a ('..._gap', key, n) entry so
        the caller can advance start_index counters to correctly reflect
        the gap instead of reporting false continuity.
        """
        from scipy import signal
        out = []
        if gap_frames:
            if self.level_enabled:
                for c in self.channels:
                    n = self.level[c].skip(gap_frames)
                    if n:
                        out.append(('level_gap', (c,), n))
            if self.bank is not None:
                for band_index, chan, n in self.bank.skip(gap_frames):
                    if self.band_output == 'waveform':
                        out.append(('band_gap', (band_index, chan), n))
                    else:
                        integrator = self.band_level.get((band_index, chan))
                        if integrator is not None:
                            n2 = integrator.skip(n)
                            if n2:
                                out.append(
                                    ('band_level_gap', (band_index, chan), n2))
        if self.level_enabled:
            for i, c in enumerate(self.channels):
                x = data[:, i]
                if self.sos is not None:
                    x, self.zi[c] = signal.sosfilt(self.sos, x, zi=self.zi[c])
                levels = self.level[c].process(x)
                if levels.size:
                    out.append(('level', (c,),
                                levels.astype('<f8', copy=False).tobytes()))
        if self.bank is not None:
            for band_index, chan, samples in self.bank.process_2d(data):
                if self.band_output == 'waveform':
                    out.append(('band', (band_index, chan),
                                samples.astype('<f8', copy=False).tobytes()))
                    continue
                integrator = self.band_level.get((band_index, chan))
                if integrator is None:
                    continue
                levels = integrator.process(samples)
                if levels.size:
                    levels = levels + self.band_offset.get(band_index, 0.0)
                    out.append(('band_level', (band_index, chan),
                                levels.astype('<f8', copy=False).tobytes()))
        return out


def _worker_main(cfg, shm_name, slot_bytes, n_slots, n_cols, task_q, result_q):
    """Worker entry point: attach the shared block buffer and process."""
    from multiprocessing import shared_memory
    shm = shared_memory.SharedMemory(name=shm_name)
    try:
        state = _WorkerState(cfg)
        result_q.put(('ready', cfg['worker_id'], state.band_metadata()))
        flat = np.ndarray((n_slots * slot_bytes // 8,), dtype=np.float64,
                          buffer=shm.buf)
        per_slot = slot_bytes // 8
        while True:
            task = task_q.get()
            if task is None:
                break
            slot, n_frames, gap_frames = task
            start = slot * per_slot
            view = flat[start:start + n_frames * n_cols].reshape(
                n_frames, n_cols)
            try:
                frames = state.process(view, gap_frames)
            except Exception as err:            # noqa: BLE001
                result_q.put(('error', cfg['worker_id'], repr(err)))
                frames = []
            result_q.put(('frames', cfg['worker_id'], slot, frames))
    finally:
        try:
            shm.close()
        except Exception:                       # noqa: BLE001
            pass


class DspPool:
    """Owns the worker processes and the shared block buffers."""

    def __init__(self, plan, common_cfg, max_block_frames):
        from multiprocessing import shared_memory
        ctx = mp.get_context('spawn' if os.name == 'nt' else 'fork')
        self.plan = plan
        self.workers = []
        self.result_q = ctx.Queue()
        self.band_metadata = {}          # worker_id -> band table
        self.errors = []
        self._shms = []

        for wid, spec in enumerate(plan):
            n_cols = len(spec['columns'])
            slot_bytes = max_block_frames * n_cols * 8
            shm = shared_memory.SharedMemory(
                create=True, size=slot_bytes * SLOTS_PER_WORKER)
            self._shms.append(shm)
            cfg = dict(common_cfg)
            cfg.update({'worker_id': wid, 'channels': spec['channels'],
                        'rate': spec['rate'], 'refs': spec['refs']})
            task_q = ctx.Queue()
            proc = ctx.Process(
                target=_worker_main,
                args=(cfg, shm.name, slot_bytes, SLOTS_PER_WORKER, n_cols,
                      task_q, self.result_q),
                daemon=True)
            proc.start()
            flat = np.ndarray((SLOTS_PER_WORKER * slot_bytes // 8,),
                              dtype=np.float64, buffer=shm.buf)
            self.workers.append({
                'spec': spec, 'proc': proc, 'task_q': task_q, 'shm': shm,
                'flat': flat, 'per_slot': slot_bytes // 8, 'n_cols': n_cols,
                'free': list(range(SLOTS_PER_WORKER)), 'pending': 0,
                'dropped': 0, 'dropped_frames': 0})

        # Wait for every worker to finish building its filter state.
        ready = 0
        while ready < len(self.workers):
            msg = self.result_q.get()
            if msg[0] == 'ready':
                self.band_metadata[msg[1]] = msg[2]
                ready += 1

    def submit(self, dev_idx, block_2d):
        """Hand a device block to the workers that serve that device.

        Drops the block (or truncates it to fit the slot) for a worker with
        no free slot (that worker is lagging) rather than blocking the
        acquisition loop. Either way, the dropped/truncated frame count is
        carried as gap_frames on the next task so the worker can skip() its
        integrators across the gap instead of silently splicing across it
        (see _WorkerState.process) -- and dropped_frames still accumulates
        even while pending, so a worker that never catches up before the
        scan ends doesn't lose the count.
        """
        total_frames = block_2d.shape[0]
        for worker in self.workers:
            spec = worker['spec']
            if spec['device'] != dev_idx:
                continue
            if not worker['free']:
                worker['dropped'] += 1
                worker['dropped_frames'] += total_frames
                continue
            n_frames = total_frames
            cap = worker['per_slot'] // worker['n_cols']
            if n_frames > cap:
                # The tail beyond the slot's capacity is lost too.
                worker['dropped_frames'] += n_frames - cap
                n_frames = cap
            slot = worker['free'].pop()
            start = slot * worker['per_slot']
            size = n_frames * worker['n_cols']
            # Copy only this worker's columns into its shared slot.
            worker['flat'][start:start + size] = \
                block_2d[:n_frames, spec['columns']].reshape(-1)
            worker['task_q'].put((slot, n_frames, worker['dropped_frames']))
            worker['dropped_frames'] = 0
            worker['pending'] += 1

    def drain(self):
        """Collect completed frames without blocking; frees slots."""
        out = []
        while True:
            try:
                msg = self.result_q.get_nowait()
            except Exception:                   # noqa: BLE001 - queue.Empty
                break
            if msg[0] == 'error':
                self.errors.append((msg[1], msg[2]))
                continue
            if msg[0] != 'frames':
                continue
            _tag, wid, slot, frames = msg
            worker = self.workers[wid]
            worker['free'].append(slot)
            worker['pending'] -= 1
            out.extend(frames)
        return out

    def stats(self):
        return {'workers': len(self.workers),
                'dropped_blocks': sum(w['dropped'] for w in self.workers),
                'channels': [w['spec']['channels'] for w in self.workers]}

    def close(self):
        for worker in self.workers:
            try:
                worker['task_q'].put(None)
            except Exception:                   # noqa: BLE001
                pass
        for worker in self.workers:
            worker['proc'].join(timeout=2.0)
            if worker['proc'].is_alive():
                worker['proc'].terminate()
        for worker in self.workers:
            worker['flat'] = None
        for shm in self._shms:
            try:
                shm.close()
                shm.unlink()
            except Exception:                   # noqa: BLE001
                pass
        self._shms = []
        self.workers = []
