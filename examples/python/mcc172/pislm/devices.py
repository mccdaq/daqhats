#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
Acquisition-device backends for the PiSLM.

Two backends behind one small interface, so the monitor can run an MCC 172
DAQ HAT (2 IEPE channels, daqhats library) and a Data Translation DT9837A
(4 IEPE channels, uldaq library) side by side -- 6 channels total.

Global channel numbering: devices are opened in the order listed in
config.ini and their channels are numbered sequentially, e.g.

    mcc172  local 0,1   -> global 0,1
    dt9837a local 0..3  -> global 2..5

IMPORTANT -- clocks are NOT synchronized between devices. Each device runs
its own ADC clock, so blocks from different devices drift relative to each
other. Per-channel levels and metrics are unaffected; cross-channel phase
analysis is only valid within a single device.

Backend interface (duck-typed):
    name, num_channels, actual_rate, running
    configure(rate, iepe_by_local_chan, sensitivity_mv_by_local_chan)
    start()                 -> begin the continuous scan
    read_new()              -> (interleaved_samples, overrun_flag); non-blocking
    stop()
    close()
    set_sensitivity(local_chan, mv_per_unit)   (only while stopped)
    set_iepe(local_chan, on)                   (only while stopped)
    info()                  -> dict for the "info" command
    blink(count)            -> flash the device LED if supported

Sensitivity is expressed in mV per mechanical unit everywhere (the MCC 172
convention); the uldaq backend converts to V/unit internally. 1000 mV/unit
means "no scaling" (data in volts) for both backends.
"""
from __future__ import print_function

import numpy as np

#: Shared empty block returned when a device has produced no new samples.
_EMPTY = np.empty(0, dtype=np.float64)


class Mcc172Backend:
    """MCC 172 DAQ HAT via the daqhats library (2 IEPE channels)."""

    name = 'mcc172'
    ao_full_scale_v = None   # no analog output on this device

    def __init__(self, channels=None, address=None):
        from daqhats import hat_list, mcc172, HatIDs, HatError
        self._HatError = HatError
        hats = hat_list(filter_by_id=HatIDs.MCC_172)
        if not hats:
            raise RuntimeError('No MCC 172 HAT device found')
        descriptor = hats[0] if address is None else next(
            (h for h in hats if h.address == address), None)
        if descriptor is None:
            raise RuntimeError('No MCC 172 at address {}'.format(address))
        self._hat = mcc172(descriptor.address)
        self._product = descriptor.product_name
        self._address = descriptor.address
        self.channels = sorted(set(channels)) if channels else [0, 1]
        if any(c not in (0, 1) for c in self.channels):
            raise ValueError('mcc172 channels must be a subset of 0, 1')
        self.num_channels = len(self.channels)
        self.actual_rate = 51200.0
        self.running = False
        # ADC full-scale, for clipping/overload detection -- fixed by the
        # hardware, not affected by the sensitivity scaling applied on read.
        self.full_scale_v = abs(mcc172.info().AI_MAX_VOLTAGE)

    # -- configuration (call while stopped) --
    def configure(self, rate, iepe, sensitivity_mv):
        from daqhats import SourceType
        from time import sleep
        for chan in self.channels:
            self._hat.iepe_config_write(chan, 1 if iepe.get(chan) else 0)
            self._hat.a_in_sensitivity_write(
                chan, sensitivity_mv.get(chan, 1000.0))
        self._hat.a_in_clock_config_write(SourceType.LOCAL, rate)
        synced = False
        while not synced:
            _src, self.actual_rate, synced = self._hat.a_in_clock_config_read()
            if not synced:
                sleep(0.005)

    def set_sensitivity(self, chan, mv_per_unit):
        self._hat.a_in_sensitivity_write(chan, mv_per_unit)

    def set_iepe(self, chan, on):
        self._hat.iepe_config_write(chan, 1 if on else 0)

    # -- triggered start --
    def arm_trigger(self):
        """Configure the TRIG input for a rising edge (call while stopped).
        Rising is the common denominator: the DT9837A's external digital
        trigger only supports rising edges."""
        from daqhats import SourceType, TriggerModes
        self._hat.trigger_config(SourceType.LOCAL, TriggerModes.RISING_EDGE)

    def has_triggered(self):
        """True once the armed scan has seen its trigger edge."""
        try:
            return bool(self._hat.a_in_scan_status().triggered)
        except self._HatError:
            return False

    # -- streaming --
    def start(self, triggered=False):
        from daqhats import OptionFlags
        mask = 0
        for chan in self.channels:
            mask |= 1 << chan
        options = OptionFlags.CONTINUOUS
        if triggered:
            options |= OptionFlags.EXTTRIGGER
        self._hat.a_in_scan_start(mask, 0, options)
        self.running = True

    def read_new(self):
        """Return (interleaved float64 numpy array, overrun).

        Uses the daqhats NumPy read so samples never pass through a Python
        list -- 8 bytes/sample instead of ~32, and no per-sample boxing.
        """
        result = self._hat.a_in_scan_read_numpy(-1, 0)
        overrun = result.hardware_overrun or result.buffer_overrun
        return result.data, overrun

    def stop(self):
        try:
            self._hat.a_in_scan_stop()
        except self._HatError:
            pass
        try:
            self._hat.a_in_scan_cleanup()
        except self._HatError:
            pass
        self.running = False

    def close(self):
        for chan in self.channels:
            try:
                self._hat.iepe_config_write(chan, 0)
            except self._HatError:
                pass

    # -- misc --
    def info(self):
        fw = self._hat.firmware_version()
        return {'type': 'mcc172', 'product_name': self._product,
                'address': self._address,
                'firmware_version': fw.version,
                'serial': self._hat.serial(),
                'calibration_date': self._hat.calibration_date(),
                # ADC input range, volts, at the screw terminals/10-32
                # connectors -- fixed by the hardware (+-5V AC coupled),
                # not affected by the sensitivity scaling applied on read.
                'full_scale_v': self.full_scale_v}

    def blink(self, count):
        self._hat.blink_led(count)

    # -- analog output: the MCC 172 is IEPE input only, no DAC. --
    def has_output(self):
        return False

    # MCC-172-specific extras used by some commands.
    def calibration_read(self, chan):
        return self._hat.calibration_coefficient_read(chan)

    def calibration_write(self, chan, slope, offset):
        self._hat.calibration_coefficient_write(chan, slope, offset)

    def trigger_config(self, source, mode):
        self._hat.trigger_config(source, mode)

    def test_signals_write(self, mode, clock, sync):
        self._hat.test_signals_write(mode, clock, sync)


class Dt9837aBackend:
    """Data Translation DT9837A via the uldaq library (4 IEPE channels).

    The DT9837A is a USB dynamic-signal-analyzer module: 4 simultaneous
    24-bit IEPE inputs, up to 52.734 kHz per channel. uldaq handles IEPE
    excitation, AC coupling, and per-channel sensor sensitivity (V/unit,
    converted from the monitor's mV/unit convention).
    """

    name = 'dt9837a'

    #: seconds of circular buffer allocated for the background scan
    BUFFER_SECONDS = 4.0

    #: uldaq Range enum name -> full-scale volts. Only these two are ever
    #: registered for the DT9837A (see AiUsb9837x.cpp).
    _RANGE_VOLTS = {'BIP10VOLTS': 10.0, 'BIP1VOLTS': 1.0}

    #: Analog output range -- only BIP10VOLTS is ever registered for the
    #: DT9837A's AO subsystem (see AoUsb9837x.cpp; the DT9837C uses
    #: BIP3VOLTS instead, but that variant isn't handled here).
    _AO_RANGE_VOLTS = {'BIP10VOLTS': 10.0}

    def __init__(self, channels=None, unique_id=None):
        import uldaq
        self._ul = uldaq
        inventory = uldaq.get_daq_device_inventory(uldaq.InterfaceType.ANY)
        matches = [d for d in inventory
                   if 'DT9837' in (d.product_name or '')]
        if unique_id is not None:
            matches = [d for d in matches if d.unique_id == unique_id]
        if not matches:
            raise RuntimeError('No DT9837A device found')
        self._descriptor = matches[0]
        self._device = uldaq.DaqDevice(self._descriptor)
        self._device.connect()
        self._ai = self._device.get_ai_device()
        self._ai_config = self._ai.get_config()
        info = self._ai.get_info()
        max_chans = info.get_num_chans()
        # uldaq scans a contiguous low..high range, so channels must be 0..N.
        self.channels = sorted(set(channels)) if channels else list(
            range(min(4, max_chans)))
        if self.channels != list(range(len(self.channels))):
            raise ValueError('dt9837a channels must be contiguous from 0 '
                             '(e.g. 0,1,2)')
        if len(self.channels) > max_chans:
            raise ValueError('dt9837a has only {} channels'.format(max_chans))
        self.num_channels = len(self.channels)
        self._range = info.get_ranges(self._input_mode())[0]
        # ADC full-scale, for clipping/overload detection -- fixed by the
        # selected input range, not affected by the sensitivity scaling
        # applied on read. Only BIP10VOLTS/BIP1VOLTS are ever registered for
        # this device (see AiUsb9837x.cpp); default to the wider one if an
        # unexpected range name shows up rather than raising.
        self.full_scale_v = self._RANGE_VOLTS.get(self._range.name, 10.0)
        self._buffer = None
        self._view = None
        self._last_total = 0
        self.actual_rate = 51200.0
        self.running = False

        # -- analog output: one channel, DT9837A only (see Usb9837x.cpp:
        # `new AoUsb9837x(*this, 1)` -- the AO subsystem is hard-wired to a
        # single channel regardless of how many AI channels are in use). --
        self._ao = None
        self.ao_full_scale_v = None
        self.ao_min_rate = None
        self.ao_max_rate = None
        try:
            ao = self._device.get_ao_device()
            if ao is not None:
                ao_info = ao.get_info()
                ao_range = ao_info.get_ranges()[0]
                self.ao_full_scale_v = self._AO_RANGE_VOLTS.get(
                    ao_range.name, 10.0)
                self.ao_min_rate = ao_info.get_min_scan_rate()
                self.ao_max_rate = ao_info.get_max_scan_rate()
                self._ao_range = ao_range
                self._ao = ao
        except Exception:   # noqa: BLE001 - AO is optional; AI must not fail
            self._ao = None
        self._ao_buffer = None
        self._ao_running = False
        self._ao_last_played = 0

    def has_output(self):
        return self._ao is not None

    def _input_mode(self):
        # The DT9837A's ADC only supports single-ended inputs; DIFFERENTIAL
        # has no ranges registered in the driver and get_ranges() returns [].
        return self._ul.AiInputMode.SINGLE_ENDED

    # -- configuration (call while stopped) --
    def configure(self, rate, iepe, sensitivity_mv):
        ul = self._ul
        for chan in self.channels:
            mode = (ul.IepeMode.ENABLED if iepe.get(chan)
                    else ul.IepeMode.DISABLED)
            self._ai_config.set_chan_iepe_mode(chan, mode)
            self._ai_config.set_chan_coupling_mode(chan, ul.CouplingMode.AC)
            # uldaq sensitivity is V/unit; the monitor speaks mV/unit.
            self._ai_config.set_chan_sensor_sensitivity(
                chan, sensitivity_mv.get(chan, 1000.0) / 1000.0)
        self._requested_rate = rate
        # The exact achieved rate is only reported when the scan starts;
        # until then the requested rate is the best estimate (the DT9837A
        # supports nearly arbitrary rates up to 52.734 kHz).
        self.actual_rate = rate

    def set_sensitivity(self, chan, mv_per_unit):
        self._ai_config.set_chan_sensor_sensitivity(chan, mv_per_unit / 1000.0)

    def set_iepe(self, chan, on):
        ul = self._ul
        self._ai_config.set_chan_iepe_mode(
            chan, ul.IepeMode.ENABLED if on else ul.IepeMode.DISABLED)

    # -- triggered start --
    def arm_trigger(self):
        """Arm the external digital trigger (rising edge -- the only edge the
        DT9837A supports). Call while stopped, before start()."""
        ul = self._ul
        self._ai.set_trigger(ul.TriggerType.POS_EDGE, 0, 0.0, 0.0, 0)

    def has_triggered(self):
        """True once samples have started flowing (the DT9837A has no
        explicit 'triggered' status; first data implies the edge arrived)."""
        try:
            _status, transfer = self._ai.get_scan_status()
            return transfer.current_total_count > 0
        except self._ul.ULException:
            return False

    # -- streaming --
    def start(self, triggered=False):
        ul = self._ul
        samples_per_chan = max(1000, int(self._requested_rate *
                                         self.BUFFER_SECONDS))
        self._buffer = ul.create_float_buffer(self.num_channels,
                                              samples_per_chan)
        self._view = None          # numpy view, built on first read
        self._last_total = 0
        options = ul.ScanOption.CONTINUOUS
        if triggered:
            options |= ul.ScanOption.EXTTRIGGER
        self.actual_rate = self._ai.a_in_scan(
            0, self.num_channels - 1, self._input_mode(), self._range,
            samples_per_chan, self._requested_rate,
            options, ul.AInScanFlag.DEFAULT, self._buffer)
        self.running = True

    def read_new(self):
        """Return (interleaved float64 numpy array, overrun).

        The uldaq scan buffer is a ctypes double array; ``np.frombuffer``
        wraps it without copying, so only the new span is copied out.
        """
        import numpy as np
        ul = self._ul
        status, transfer = self._ai.get_scan_status()
        total = transfer.current_total_count      # cumulative, interleaved
        new = total - self._last_total
        buf_len = len(self._buffer)
        overrun = False
        if new > buf_len:
            # The writer lapped us; drop to the newest full buffer.
            self._last_total = total - buf_len
            new = buf_len
            overrun = True
        start = self._last_total % buf_len
        end = (start + new) % buf_len
        if self._view is None:
            self._view = np.frombuffer(self._buffer, dtype=np.float64)
        if new == 0:
            data = _EMPTY
        elif start < end:
            data = self._view[start:end].copy()
        else:
            data = np.concatenate((self._view[start:], self._view[:end]))
        self._last_total = total
        # A non-RUNNING status is an error only once data has flowed; an
        # armed scan waiting for its trigger edge must not be flagged.
        if status != ul.ScanStatus.RUNNING and self.running and total > 0:
            overrun = True
        return data, overrun

    def stop(self):
        try:
            self._ai.scan_stop()
        except self._ul.ULException:
            pass
        self.running = False

    def close(self):
        try:
            self.stop()
        except Exception:   # noqa: BLE001 - releasing best-effort
            pass
        try:
            self.stop_output()
        except Exception:   # noqa: BLE001 - releasing best-effort
            pass
        try:
            self._device.disconnect()
            self._device.release()
        except self._ul.ULException:
            pass

    # -- analog output (single channel; independent of the AI scan) --
    def start_output(self, samples_volts, rate):
        """Play a finite buffer once (not recycled/looped -- 'repeats' for
        MLS is handled by the caller tiling the sequence before this call).
        `samples_volts` is a 1-D array already scaled to real volts (the
        caller multiplies the normalized excitation.py signal by
        ao_full_scale_v). Returns the achieved output rate."""
        if self._ao is None:
            raise RuntimeError('no analog output on this device')
        import numpy as np
        ul = self._ul
        n = len(samples_volts)
        self._ao_buffer = ul.create_float_buffer(1, n)
        view = np.frombuffer(self._ao_buffer, dtype=np.float64)
        view[:n] = samples_volts
        actual_rate = self._ao.a_out_scan(
            0, 0, self._ao_range, n, rate, ul.ScanOption.DEFAULTIO,
            ul.AOutScanFlag.DEFAULT, self._ao_buffer)
        self._ao_running = True
        self._ao_last_played = 0
        return actual_rate

    def output_progress(self):
        """(samples_played, done) for the in-flight output scan.

        Once the scan has stopped (naturally or via stop_output()), keeps
        returning the last known count instead of resetting to 0 -- a
        caller that queries progress *after* a stop (e.g. to report
        samples_played in the stop response, or a completion watcher
        racing a concurrent stop_output()) must still see the true final
        count, not an artifact of query timing.
        """
        if self._ao is None:
            return 0, True
        if not self._ao_running:
            return self._ao_last_played, True
        try:
            status, transfer = self._ao.get_scan_status()
        except self._ul.ULException:
            return self._ao_last_played, True
        done = status != self._ul.ScanStatus.RUNNING
        self._ao_last_played = transfer.current_total_count
        if done:
            self._ao_running = False
        return self._ao_last_played, done

    def stop_output(self, ramp_ms=10.0):
        """Ramp to 0 V over ~ramp_ms (avoids a click/thump into an
        amplifier), then stop the scan and leave the DAC at 0 V."""
        if self._ao is None:
            return
        if self._ao_running:
            try:
                self._ramp_output_to_zero(ramp_ms)
            except self._ul.ULException:
                pass
            try:
                self._ao.scan_stop()
            except self._ul.ULException:
                pass
            # Capture the true final count now that the scan has been
            # stopped, so a subsequent output_progress() (e.g. the
            # completion watcher racing this call) reports the real
            # samples-played instead of a stale pre-stop snapshot.
            try:
                _status, transfer = self._ao.get_scan_status()
                self._ao_last_played = transfer.current_total_count
            except self._ul.ULException:
                pass
            self._ao_running = False
        else:
            self.zero_output()

    def zero_output(self):
        if self._ao is None:
            return
        try:
            self._ao.a_out(0, self._ao_range, self._ul.AOutFlag.DEFAULT, 0.0)
        except self._ul.ULException:
            pass

    def _ramp_output_to_zero(self, ramp_ms):
        """Step the single-value output down from the last sample toward
        0 V. During a scan the single-value write and the scan share the
        same DAC, so this both audibly fades and leaves a clean state for
        scan_stop()."""
        import numpy as np
        from time import sleep
        ul = self._ul
        last = 0.0
        if self._ao_buffer is not None:
            view = np.frombuffer(self._ao_buffer, dtype=np.float64)
            if view.size:
                last = float(view[-1])
        steps = max(1, int(ramp_ms))
        for i in range(steps, -1, -1):
            self._ao.a_out(0, self._ao_range, ul.AOutFlag.DEFAULT,
                           last * i / steps)
            sleep(0.001)

    # -- misc --
    def info(self):
        return {'type': 'dt9837a',
                'product_name': self._descriptor.product_name,
                'unique_id': self._descriptor.unique_id,
                'interface': str(self._descriptor.dev_interface),
                # ADC input range, volts -- auto-detected from the device
                # (BIP10VOLTS or BIP1VOLTS; see _RANGE_VOLTS above), not
                # affected by the sensitivity scaling applied on read.
                'full_scale_v': self.full_scale_v,
                'input_range': self._range.name}

    def blink(self, count):
        self._device.flash_led(count)


def open_backends(device_configs):
    """Open the configured backends in order, returning (backends, errors).

    ``device_configs`` is a list of dicts: {'type': 'mcc172'|'dt9837a',
    'channels': [...]}. Unavailable devices are reported, not fatal, so a Pi
    with only one of the two devices attached still starts with what it has.
    """
    backends = []
    errors = {}
    for cfg in device_configs:
        name = cfg.get('type')
        try:
            if name == 'mcc172':
                backends.append(Mcc172Backend(channels=cfg.get('channels')))
            elif name == 'dt9837a':
                backends.append(Dt9837aBackend(channels=cfg.get('channels')))
            else:
                errors[name] = 'unknown device type'
        except Exception as err:    # noqa: BLE001 - report and continue
            errors[name] = str(err)
    return backends, errors


class ChannelMap:
    """Maps global channel numbers to (device_index, local_channel)."""

    def __init__(self, backends):
        self.entries = []            # global index -> (dev_idx, local_chan)
        for dev_idx, backend in enumerate(backends):
            for local in backend.channels:
                self.entries.append((dev_idx, local))

    def __len__(self):
        return len(self.entries)

    def resolve(self, global_chan):
        if 0 <= global_chan < len(self.entries):
            return self.entries[global_chan]
        raise ValueError('channel must be 0..{}'.format(len(self.entries) - 1))

    def globals_for_device(self, dev_idx):
        return [g for g, (d, _l) in enumerate(self.entries) if d == dev_idx]

    def table(self, backends):
        return [{'global': g, 'device': d, 'device_type': backends[d].name,
                 'local': l} for g, (d, l) in enumerate(self.entries)]
