#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
PiSLM -- multi-device sound level meter -- Raspberry Pi side.

Runs on a headless Raspberry Pi and behaves like a networked sound level
meter across up to two IEPE acquisition devices:

    * MCC 172 DAQ HAT (daqhats library)  -- 2 IEPE channels
    * Data Translation DT9837A (uldaq)   -- 4 IEPE channels

for a total of 6 channels, numbered globally in device order (mcc172 first
by default: global 0-1 = MCC 172, global 2-5 = DT9837A). All streaming and
commands use global channel numbers.

NOTE -- the two devices run separate, unsynchronized ADC clocks. Levels and
metrics per channel are unaffected, but cross-channel phase comparisons are
only valid within one device.

It streams Fast/Slow/Impulse time-weighted levels continuously, keeps raw
samples in per-device ring buffers, and computes Leq / Lmax / Lmin / Lpeak /
LN over a window on the "get_metrics" command. Designed to be launched at
boot by systemd (see pislm.service).

Two separate TCP ports (see PROTOCOL.md for the full specification):

  Control port (default 5000) -- newline-delimited UTF-8 JSON, both ways.
      Handshake on connect, then commands / responses / events.

  Streaming port (default 5001) -- typed, length-prefixed binary frames:
          [1-byte type][4-byte little-endian uint32 length][payload]
      Every payload below ends its fixed header with an 8-byte little-endian
      uint64 start_index: the index, on that stream's own sample grid (reset
      to 0 at each start()), of the first sample in the frame. It advances
      monotonically even across a network-dropped frame, so a client can
      size a gap exactly (pislm/4; see PROTOCOL.md sec. 1 and 9).
      type 0x01 DATA : [4-byte device index][8-byte start_index] then
                       interleaved little-endian float64 samples for that
                       device's channels (channel-fastest within the device).
      type 0x02 MSG  : UTF-8 JSON (handshake on connect, then events).
      type 0x03 BAND : fractional-octave band waveform.
                       [4-byte band index][4-byte GLOBAL channel]
                       [8-byte start_index] + float64.
      type 0x04 LEVEL: broadband time-weighted level in dB.
                       [4-byte GLOBAL channel][8-byte start_index] + float64.
      type 0x05 BAND_LEVEL : per-band time-weighted level in dB.
                       [4-byte band index][4-byte GLOBAL channel]
                       [8-byte start_index] + float64.
      type 0x06 RAW_DUMP : one chunk of an on-demand "get_raw" dump of the
                       RAM ring buffer. [4-byte dump id][4-byte device index]
                       [4-byte chunk index][4-byte is_last][8-byte
                       start_index -- same grid as DATA for that device] +
                       interleaved float64 (channel-fastest within the
                       device). Chunks are delivered reliably (blocking, not
                       dropped).

Storage note: raw samples live only in RAM (packed float64 ring buffers,
[storage] buffer_seconds each per device). Nothing is written to the SD
card; the laptop records via stream_raw live streaming and/or pulls the
buffered window with get_raw after an event.
"""
from __future__ import print_function

import configparser
import json
import os
import signal
import socket
import struct
import sys
import threading
from datetime import datetime, timezone
from time import monotonic, sleep
from time import time as wall_time

try:
    import queue
except ImportError:  # pragma: no cover - Python 2 fallback
    import Queue as queue

from devices import open_backends, ChannelMap, Mcc172Backend

PROTOCOL_VERSION = 'pislm/4'

# Downstream frame types.
TYPE_DATA = 0x01        # raw interleaved waveform (per device)
TYPE_MSG = 0x02         # JSON handshake / events (and responses on ctrl port)
TYPE_BAND = 0x03        # decimated fractional-octave band waveform
TYPE_LEVEL = 0x04       # broadband time-weighted level (dB)
TYPE_BAND_LEVEL = 0x05  # per-band time-weighted level (dB)
TYPE_RAW_DUMP = 0x06    # on-demand chunked dump of the buffered raw samples
FRAME_HEADER = struct.Struct('<BI')          # type byte + payload length
# Every payload header below ends with a u64 start_index: the index (on
# that stream's own sample grid, reset to 0 at each start()) of the first
# sample in this frame. It advances monotonically even when a frame is
# later dropped by network backpressure, so a client can detect and size
# a gap exactly (see PROTOCOL.md sec. 9).
DATA_HEADER = struct.Struct('<IQ')            # device index, start_index
BAND_HEADER = struct.Struct('<IIQ')           # band index, channel, start_index
LEVEL_HEADER = struct.Struct('<IQ')           # channel, start_index
BAND_LEVEL_HEADER = struct.Struct('<IIQ')     # band index, channel, start_index
RAW_DUMP_HEADER = struct.Struct('<IIIIQ')     # dump id, device, chunk, is_last, start_index

# Interleaved samples per RAW_DUMP chunk (x8 bytes = 512 KiB per frame).
RAW_DUMP_CHUNK = 65536


class CommandError(Exception):
    """Raised for invalid/rejected commands; reported back to the client."""


# --------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------
def _channel_list(text):
    return [int(c) for c in text.split(',') if c.strip() != '']


def update_ini(path, values, note=None):
    """Set ``{(section, key): value}`` in an ini file, preserving layout.

    configparser would drop every comment on write, and config.ini is
    largely documentation, so this rewrites the affected lines in place:
    existing keys keep their position (an optional ``note`` replaces their
    inline comment), and keys that do not exist yet are appended to their
    section. Sections that do not exist are created at the end.

    Returns the list of "section.key" entries that were written.
    """
    with open(path, 'r') as handle:
        lines = handle.read().splitlines()

    pending = dict(values)
    written = []
    section = None
    out = []
    # Track where each section ends so new keys land inside it.
    section_last_line = {}
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            section = stripped[1:-1]
        elif section is not None and stripped and \
                not stripped.startswith((';', '#')):
            section_last_line[section] = idx

    section = None
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            section = stripped[1:-1]
            out.append(line)
            continue
        key = None
        if '=' in line and not stripped.startswith((';', '#')):
            key = line.split('=', 1)[0].strip()
        if key is not None and (section, key) in pending:
            value = pending.pop((section, key))
            text = '{} = {}'.format(key, value)
            if note:
                text += '   ; {}'.format(note)
            out.append(text)
            written.append('{}.{}'.format(section, key))
        else:
            out.append(line)
        if section is not None and idx == section_last_line.get(section):
            # End of this section's body: append its missing keys here.
            for (sec, k) in [kv for kv in pending if kv[0] == section]:
                value = pending.pop((sec, k))
                text = '{} = {}'.format(k, value)
                if note:
                    text += '   ; {}'.format(note)
                out.append(text)
                written.append('{}.{}'.format(sec, k))

    # Anything left belongs to a section the file does not have yet.
    for (sec, k), value in list(pending.items()):
        if not any(ln.strip() == '[{}]'.format(sec) for ln in out):
            out.extend(['', '[{}]'.format(sec)])
        text = '{} = {}'.format(k, value)
        if note:
            text += '   ; {}'.format(note)
        out.append(text)
        written.append('{}.{}'.format(sec, k))

    # Write via a temporary file so a crash cannot truncate the config.
    tmp = path + '.tmp'
    with open(tmp, 'w') as handle:
        handle.write('\n'.join(out) + '\n')
    os.replace(tmp, path)
    return written


def load_config(path):
    """Load config.ini into a plain settings dict (the initial state)."""
    parser = configparser.ConfigParser(inline_comment_prefixes=(';', '#'))
    if not parser.read(path):
        raise RuntimeError('Could not read configuration file: {}'.format(path))

    device_names = [n.strip() for n in parser.get(
        'devices', 'enabled', fallback='mcc172').split(',') if n.strip()]
    devices = []
    for name in device_names:
        section = name
        channels = None
        if parser.has_option(section, 'channels'):
            channels = _channel_list(parser.get(section, 'channels'))
        devices.append({'type': name, 'channels': channels,
                        'iepe_enable': parser.getboolean(
                            section, 'iepe_enable', fallback=True),
                        'sensitivity': {
                            ch: parser.getfloat(
                                section, 'sensitivity_ch{}'.format(ch),
                                fallback=1000.0)
                            for ch in range(8)}})

    return {
        'devices': devices,
        'sample_rate': parser.getfloat('acquisition', 'sample_rate',
                                       fallback=51200.0),
        'stream_raw': parser.getboolean('acquisition', 'stream_raw',
                                        fallback=False),
        'host': parser.get('network', 'host'),
        'control_port': parser.getint('network', 'control_port'),
        'stream_port': parser.getint('network', 'stream_port'),
        'max_queue_blocks': parser.getint('network', 'max_queue_blocks'),
        'autostart': parser.getboolean('control', 'autostart', fallback=True),
        'bands': {
            'enabled': parser.getboolean('bands', 'enabled', fallback=False),
            'output': parser.get('bands', 'output', fallback='level'),
            'f_min': parser.getfloat('bands', 'f_min', fallback=20.0),
            'f_max': parser.getfloat('bands', 'f_max', fallback=20000.0),
            'fraction': parser.getint('bands', 'fraction', fallback=3),
            'order': parser.getint('bands', 'order', fallback=3),
            'margin': parser.getfloat('bands', 'decimation_margin',
                                      fallback=1.0),
        },
        'weighting': {
            'frequency': parser.get('weighting', 'frequency', fallback='A'),
            'time': parser.get('weighting', 'time', fallback='Fast'),
        },
        'level': {
            'enabled': parser.getboolean('level', 'enabled', fallback=True),
            'output_rate': parser.getfloat('level', 'output_rate',
                                           fallback=10.0),
        },
        'storage': {
            'buffer_seconds': parser.getfloat('storage', 'buffer_seconds',
                                              fallback=60.0),
        },
        'dsp': {
            'workers': parser.getint('dsp', 'workers', fallback=-1),
        },
        'resample': {
            'enabled': parser.getboolean('resample', 'enabled',
                                         fallback=False),
            'output_rate': parser.getfloat('resample', 'output_rate',
                                           fallback=48000.0),
            'taps': parser.getint('resample', 'taps', fallback=32),
            'phases': parser.getint('resample', 'phases', fallback=4096),
        },
        'trigger': {
            'enabled': parser.getboolean('trigger', 'sync_start',
                                         fallback=False),
            'source': parser.get('trigger', 'source', fallback='gpio'),
            'gpio_pin': parser.getint('trigger', 'gpio_pin', fallback=17),
            'pulse_ms': parser.getfloat('trigger', 'pulse_ms', fallback=10.0),
        },
        'ups': {
            # Read-only: this is written by the separate
            # pislm-shutdown-button service (shutdown_button.py), which
            # owns the actual I2C polling and low-battery shutdown -- see
            # its module docstring. pislm.py only surfaces the latest
            # snapshot for status/get_config, and never touches the bus.
            'status_file': parser.get(
                'ups', 'status_file', fallback='/run/pislm-ups-status.json'),
            'stale_after_seconds': parser.getfloat(
                'ups', 'stale_after_seconds', fallback=60.0),
        },
    }


# --------------------------------------------------------------------------
# Framing helpers
# --------------------------------------------------------------------------
def build_frame(frame_type, payload):
    return FRAME_HEADER.pack(frame_type, len(payload)) + payload


def data_frame(device_index, start_index, payload_bytes):
    """A DATA frame: [device index][start_index] then interleaved float64."""
    return build_frame(TYPE_DATA,
                       DATA_HEADER.pack(device_index, start_index) +
                       payload_bytes)


def msg_frame(obj):
    return build_frame(TYPE_MSG, json.dumps(obj).encode('utf-8'))


def band_frame(band_index, channel, start_index, sample_bytes):
    """A BAND frame: [band_index][global channel][start_index], decimated
    float64."""
    return build_frame(
        TYPE_BAND,
        BAND_HEADER.pack(band_index, channel, start_index) + sample_bytes)


def level_frame(channel, start_index, sample_bytes):
    """A LEVEL frame: [global channel][start_index], float64 level(dB)."""
    return build_frame(
        TYPE_LEVEL, LEVEL_HEADER.pack(channel, start_index) + sample_bytes)


def band_level_frame(band_index, channel, start_index, sample_bytes):
    """A BAND_LEVEL frame: [band_index][global channel][start_index],
    float64 dB."""
    return build_frame(
        TYPE_BAND_LEVEL,
        BAND_LEVEL_HEADER.pack(band_index, channel, start_index) +
        sample_bytes)


def raw_dump_frame(dump_id, device_index, chunk_index, is_last, start_index,
                   sample_bytes):
    """A RAW_DUMP frame: one chunk of a get_raw buffer dump. start_index is
    on the same grid as DATA for that device, so a dump lines up exactly
    with the live stream it was pulled from."""
    return build_frame(
        TYPE_RAW_DUMP,
        RAW_DUMP_HEADER.pack(dump_id, device_index, chunk_index, is_last,
                             start_index) +
        sample_bytes)


# --------------------------------------------------------------------------
# Raw sample ring buffer -- per device; keeps the most recent N seconds so
# the client can request Leq / Lmax / Lmin / LN over a window on demand.
# --------------------------------------------------------------------------
class RawRingBuffer:
    """Rolling store of interleaved raw samples, trimmed to a max duration.

    Blocks are stored as float64 numpy arrays (8 bytes per sample), so the
    RAM cost is the theoretical minimum -- e.g. 2 ch x 51.2 kHz for 300 s is
    ~246 MB -- and blocks are never mutated after append, so readers can
    snapshot the block list under the lock and concatenate outside it.
    """

    def __init__(self, max_seconds, num_channels, sample_rate):
        self._num_channels = num_channels
        self._max_interleaved = int(max_seconds * sample_rate) * num_channels
        self._blocks = []            # list of float64 numpy arrays
        self._count = 0              # total interleaved samples held
        self._lock = threading.Lock()

    def append(self, interleaved):
        """Store one interleaved block (a float64 numpy array)."""
        with self._lock:
            self._blocks.append(interleaved)
            self._count += interleaved.size
            while (self._blocks and
                   self._count - self._blocks[0].size >=
                   self._max_interleaved):
                self._count -= self._blocks.pop(0).size

    def get_recent(self, seconds, sample_rate):
        """Return the most recent ``seconds`` as one interleaved float64
        numpy array (trimmed to whole frames)."""
        import numpy as np
        want = int(seconds * sample_rate) * self._num_channels
        with self._lock:
            blocks = list(self._blocks)
        picked = []
        total = 0
        for block in reversed(blocks):
            picked.append(block)
            total += block.size
            if total >= want:
                break
        picked.reverse()
        if not picked:
            return np.empty(0, dtype=np.float64)
        data = (picked[0] if len(picked) == 1
                else np.concatenate(picked))
        if want and data.size > want:
            data = data[-want:]
        extra = data.size % self._num_channels
        return data[extra:] if extra else data


# --------------------------------------------------------------------------
# Client registry -- two client kinds on two ports:
#   'control' : newline-delimited JSON, both directions (commands/responses).
#   'stream'  : typed length-prefixed frames (handshake + DATA + events);
#               anything the client sends on this port is ignored.
# --------------------------------------------------------------------------
class ClientRegistry:
    """Manages connected clients: fan out data/events, deliver replies.

    Each client gets TWO outbound queues, merged in delivery order by one
    sender thread (a socket can only have one writer at a time, so the two
    queues cannot each run their own send loop):

    - ``best_effort`` (bounded to ``max_queue_blocks``): DATA and BAND. This
      is the bulk of the bandwidth, so it drops the oldest queued frame on
      overflow rather than stalling acquisition for a slow client.
    - ``reliable`` (bounded, but generously -- its traffic is a tiny
      fraction of DATA's): LEVEL, BAND_LEVEL, and MSG (events/handshake).
      These are the sound-level-meter's primary output and its state
      messages, so they are practically never dropped, without ever
      blocking the producer thread (see PROTOCOL.md sec. 4).
    """

    #: LEVEL/BAND_LEVEL/MSG traffic is tiny (~1 KB/s) next to DATA/BAND, so
    #: this can be large enough to absorb minutes of backlog before a real
    #: eviction would ever happen.
    RELIABLE_QUEUE_SIZE = 8192

    _RELIABLE_KINDS = frozenset(('LEVEL', 'BAND_LEVEL', 'MSG'))

    def __init__(self, controller, max_queue_blocks):
        self._controller = controller
        self._max_queue_blocks = max_queue_blocks
        # conn -> (kind, best_effort queue.Queue, reliable queue.Queue,
        #          doorbell queue.Queue)
        self._clients = {}
        self._lock = threading.Lock()
        self._frames_dropped = 0          # total, all frame kinds
        self._frames_dropped_by_type = {}  # frame kind name -> count

    @staticmethod
    def _encode(kind, obj):
        """Encode a JSON message the way the given client kind expects."""
        if kind == 'stream':
            return msg_frame(obj)
        return (json.dumps(obj) + '\n').encode('utf-8')

    def add(self, conn, addr, kind):
        best_effort = queue.Queue(maxsize=self._max_queue_blocks)
        reliable = queue.Queue(maxsize=self.RELIABLE_QUEUE_SIZE)
        # Wake-up signal only (unbounded, tiny tokens): lets _sender block
        # until *either* queue has something, with no polling delay -- a
        # control client's best_effort queue is never fed (only stream
        # clients get DATA/BAND), so a poll-with-timeout design would have
        # to wait out a full timeout on every single reply/event.
        doorbell = queue.Queue()
        with self._lock:
            self._clients[conn] = (kind, best_effort, reliable, doorbell)
        threading.Thread(target=self._sender,
                         args=(conn, addr, best_effort, reliable, doorbell),
                         daemon=True).start()
        threading.Thread(target=self._reader,
                         args=(conn, addr, reliable, doorbell, kind),
                         daemon=True).start()
        # Greet the new client with the current configuration.
        self._enqueue(reliable, self._encode(kind, self._controller.handshake()),
                     doorbell=doorbell)
        print('[net] {} client connected: {}'.format(kind, addr), flush=True)

    def broadcast_stream_frame(self, frame, kind_name):
        """Send a prebuilt frame to stream clients.

        ``kind_name`` is one of 'DATA', 'BAND', 'LEVEL', 'BAND_LEVEL' and
        selects which queue/drop policy applies (see the class docstring).
        """
        reliable = kind_name in self._RELIABLE_KINDS
        with self._lock:
            targets = [(be, rel, bell) for (kind, be, rel, bell)
                      in self._clients.values() if kind == 'stream']
        q_index = 1 if reliable else 0
        dropped = 0
        for t in targets:
            if self._enqueue(t[q_index], frame, doorbell=t[2],
                             drop_oldest=True):
                dropped += 1
        if dropped:
            with self._lock:
                self._frames_dropped += dropped
                self._frames_dropped_by_type[kind_name] = (
                    self._frames_dropped_by_type.get(kind_name, 0) + dropped)

    def stream_frames_dropped(self):
        with self._lock:
            return self._frames_dropped

    def stream_frames_dropped_by_type(self):
        with self._lock:
            return dict(self._frames_dropped_by_type)

    def broadcast_message(self, obj):
        """Send a MSG/event to every client (both kinds), encoded per
        client kind, via the reliable queue -- an event lost to backpressure
        would leave a client's state permanently out of sync."""
        with self._lock:
            targets = [(kind, rel, bell) for (kind, _be, rel, bell)
                      in self._clients.values()]
        dropped = 0
        for kind, reliable, bell in targets:
            if self._enqueue(reliable, self._encode(kind, obj),
                             doorbell=bell, drop_oldest=True):
                dropped += 1
        if dropped:
            with self._lock:
                self._frames_dropped += dropped
                self._frames_dropped_by_type['MSG'] = (
                    self._frames_dropped_by_type.get('MSG', 0) + dropped)

    def stream_client_count(self):
        with self._lock:
            return sum(1 for (kind, _be, _rel, _bell) in self._clients.values()
                       if kind == 'stream')

    def send_stream_reliable(self, frame, timeout=30.0):
        """Send a frame to stream clients WITHOUT dropping it when the queue
        is full: block until there is room (or the per-client timeout runs
        out). Used for get_raw dumps, where every chunk matters. Shares the
        best_effort queue with DATA/BAND (documented: live frames continue
        and interleave with a dump) -- this call runs on its own thread
        (see _cmd_get_raw), so blocking here never stalls acquisition."""
        with self._lock:
            targets = [(be, bell) for (kind, be, _rel, bell)
                      in self._clients.values() if kind == 'stream']
        for send_queue, bell in targets:
            try:
                send_queue.put(frame, timeout=timeout)
                bell.put_nowait(None)
            except queue.Full:
                pass    # client stalled for the whole timeout; it loses this chunk

    @staticmethod
    def _enqueue(send_queue, frame, doorbell=None, drop_oldest=False):
        """Return True if an already-queued frame had to be evicted (i.e. a
        frame was lost to a slow consumer), False otherwise. Rings
        ``doorbell`` (if given) whenever the frame actually got queued, so
        _sender's wait wakes up immediately instead of polling."""
        try:
            send_queue.put_nowait(frame)
            if doorbell is not None:
                doorbell.put_nowait(None)
            return False
        except queue.Full:
            if not drop_oldest:
                return False
            try:
                send_queue.get_nowait()
            except queue.Empty:
                pass
            try:
                send_queue.put_nowait(frame)
                if doorbell is not None:
                    doorbell.put_nowait(None)
            except queue.Full:
                pass
            return True

    def _remove(self, conn):
        with self._lock:
            self._clients.pop(conn, None)

    def _sender(self, conn, addr, best_effort, reliable, doorbell):
        """Merge both queues onto the one socket, in send order: drain
        whatever is ready right now (reliable first -- its volume is tiny,
        so this never meaningfully delays best_effort), then block on the
        doorbell -- rung by every successful enqueue on either queue -- so
        this wakes up immediately on new data with no polling delay."""
        try:
            while True:
                try:
                    frame = reliable.get_nowait()
                except queue.Empty:
                    try:
                        frame = best_effort.get_nowait()
                    except queue.Empty:
                        doorbell.get()   # blocks until something is enqueued
                        continue
                if frame is None:
                    break
                conn.sendall(frame)
        except (OSError, socket.error):
            pass
        finally:
            self._remove(conn)
            try:
                conn.close()
            except OSError:
                pass
            print('[net] client disconnected: {}'.format(addr), flush=True)

    def _reader(self, conn, addr, reply_queue, doorbell, kind):
        """For control clients, read newline-delimited JSON commands and reply
        on the reliable queue. For stream clients, just watch for
        disconnect. Replies go on the reliable queue (same as events), so
        responses and events stay in one strict FIFO per connection."""
        buf = bytearray()
        try:
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                if kind != 'control':
                    continue   # ignore any upstream bytes on the stream port
                buf.extend(chunk)
                while b'\n' in buf:
                    line, _, rest = buf.partition(b'\n')
                    buf = bytearray(rest)
                    self._handle_line(line, reply_queue, doorbell)
        except (OSError, socket.error):
            pass
        finally:
            self._remove(conn)
            # unblock the sender
            self._enqueue(reply_queue, None, doorbell=doorbell)

    def _handle_line(self, line, reply_queue, doorbell):
        text = line.decode('utf-8', errors='replace').strip()
        if not text:
            return
        cmd_id = None
        try:
            request = json.loads(text)
            if not isinstance(request, dict):
                raise CommandError('command must be a JSON object')
            cmd_id = request.get('id')
            result = self._controller.dispatch(request)
            reply = {'type': 'response', 'id': cmd_id, 'ok': True,
                     'cmd': request.get('cmd'), 'result': result}
        except CommandError as err:
            reply = {'type': 'response', 'id': cmd_id, 'ok': False,
                     'error': str(err)}
        except (ValueError, KeyError, TypeError) as err:
            reply = {'type': 'response', 'id': cmd_id, 'ok': False,
                     'error': '{}: {}'.format(type(err).__name__, err)}
        except Exception as err:    # noqa: BLE001 - device library errors
            reply = {'type': 'response', 'id': cmd_id, 'ok': False,
                     'error': '{}: {}'.format(type(err).__name__, err)}
        self._enqueue(reply_queue, self._encode('control', reply),
                     doorbell=doorbell)


# --------------------------------------------------------------------------
# Controller: owns the acquisition devices and all configurable state
# --------------------------------------------------------------------------
class Controller:
    """Serializes access to the devices and executes client commands."""

    def __init__(self, backends, settings, config_path=None):
        self._backends = backends
        self.config_path = config_path
        self._chan_map = ChannelMap(backends)
        self._lock = threading.RLock()          # guards devices + state
        self._control_lock = threading.Lock()   # serializes start/stop
        self._registry = None

        # Global mutable state, keyed by GLOBAL channel where per-channel.
        self.sample_rate = settings['sample_rate']
        self.stream_raw = settings.get('stream_raw', False)
        self.iepe = {}
        self.sensitivity = {}
        for dev_cfg_idx, dev_cfg in enumerate(settings['devices']):
            if dev_cfg_idx >= len(backends):
                break
            backend = backends[dev_cfg_idx]
            for g in self._chan_map.globals_for_device(dev_cfg_idx):
                _d, local = self._chan_map.resolve(g)
                self.iepe[g] = 1 if dev_cfg.get('iepe_enable', True) else 0
                self.sensitivity[g] = dev_cfg.get('sensitivity', {}).get(
                    local, 1000.0)

        # Synchronized start: arm every device on a shared rising edge.
        # source 'gpio' = the Pi pulses trigger_gpio_pin itself; 'external'
        # = the user supplies the edge and the scans wait for it.
        self.trigger_cfg = dict(settings.get('trigger', {'enabled': False}))
        self._gpio = None           # GpioTrigger, opened lazily
        self._trigger_pending = set()   # dev_idx awaiting first data

        self.band_config = dict(settings.get('bands', {'enabled': False}))
        weighting = settings.get('weighting', {})
        self.freq_weighting = weighting.get('frequency', 'A')
        self.time_weighting = weighting.get('time', 'Fast')
        level = settings.get('level', {})
        self.level_enabled = level.get('enabled', True)
        self.level_rate = level.get('output_rate', 10.0)
        storage = settings.get('storage', {})
        self.buffer_seconds = storage.get('buffer_seconds', 60.0)
        self.ups_cfg = dict(settings.get('ups', {}))
        # DSP worker processes: -1 = auto (cpu_count-1), 0 = inline.
        self.dsp_workers = settings.get('dsp', {}).get('workers', -1)
        self._pool = None

        # Cross-device clock alignment. Rate tracking is always on (it is
        # nearly free and its numbers are reported); resampling to a common
        # grid is opt-in because it costs CPU.
        self.resample_cfg = dict(settings.get('resample', {'enabled': False}))
        self._trackers = {}         # dev_idx -> ClockTracker
        self._resamplers = {}       # dev_idx -> Resampler (when enabled)
        self._retune_at = {}        # dev_idx -> next retune time

        # Runtime state, rebuilt on start(): all keyed by device index or
        # global channel as noted.
        self._raw_buffers = {}      # dev_idx -> RawRingBuffer
        self._band_banks = {}       # dev_idx -> BandFilterBank
        self._wsos = {}             # dev_idx -> weighting SOS (rate-specific)
        self._wzi = {}              # global chan -> filter state
        self._level = {}            # global chan -> ExpLevel
        self._band_level = {}       # (dev_idx, band_index, global) -> ExpLevel
        self._band_offset = {}      # (dev_idx, band_index) -> dB offset

        self._running = False
        self._stop_event = threading.Event()
        self._scan_thread = None
        self._dump_id = 0           # sequence for get_raw dumps

        # pislm/4 per-stream sample counters -- the index (on that stream's
        # own grid) of the NEXT sample to be emitted, reset to 0 at start().
        # These advance even when a frame is later dropped by network
        # backpressure, so start_index always reflects true elapsed samples
        # produced, letting a client size a gap exactly.
        self._data_count = {}        # dev_idx -> count (also the RAW_DUMP grid)
        self._level_count = {}       # global chan -> count
        self._band_count = {}        # (band_index, global chan) -> count
        self._band_level_count = {}  # (band_index, global chan) -> count
        self._epoch = None           # set at start(): ties index 0 to wall time
        self._overload_count = {}    # global chan -> cumulative clipped samples
        self._overload_last_emit = {}  # global chan -> monotonic time of last event

        # Analog output (excitation signal for reverberation measurement).
        # At most one device offers it (the DT9837A; the MCC 172 is input
        # only) -- find it once, up front.
        self._output_dev_idx = next(
            (i for i, b in enumerate(backends) if b.has_output()), None)
        self._output_cfg = None      # last set_output result (incl. samples)
        self._output_running = False
        self._output_start_index = None
        self._output_samples_played = 0
        self._output_lock = threading.Lock()

        # Rate tracking is always on, so the clock figures are available
        # from the first handshake; start() rebuilds these for the rates the
        # devices actually settle on.
        self._build_clock_sync(verbose=False)

        # Apply initial IEPE + sensitivity so a fresh boot is calibrated.
        self._apply_static_config()

        # The output must be at zero until output_start (§ safety).
        if self._output_dev_idx is not None:
            self._backends[self._output_dev_idx].zero_output()

    def attach_registry(self, registry):
        self._registry = registry

    # -- helpers -----------------------------------------------------------
    @property
    def running(self):
        with self._lock:
            return self._running

    @property
    def channels(self):
        return list(range(len(self._chan_map)))

    def _apply_static_config(self):
        with self._lock:
            for dev_idx, backend in enumerate(self._backends):
                iepe_local = {}
                sens_local = {}
                for g in self._chan_map.globals_for_device(dev_idx):
                    _d, local = self._chan_map.resolve(g)
                    iepe_local[local] = self.iepe.get(g, 1)
                    sens_local[local] = self.sensitivity.get(g, 1000.0)
                backend.configure(self.sample_rate, iepe_local, sens_local)

    def _require_stopped(self):
        if self._running:
            raise CommandError('a scan is active; send "stop" first')

    def _resolve(self, global_chan):
        try:
            return self._chan_map.resolve(int(global_chan))
        except ValueError as err:
            raise CommandError(str(err))

    def _units(self):
        return {str(g): ('Pa' if self.sensitivity.get(g, 1000.0) != 1000
                         else 'V') for g in self.channels}

    def _rate(self, dev_idx):
        """Rate of the data downstream of resampling.

        With resampling on, every device's stream has been converted to the
        common output rate, so ring buffers, DSP, metrics and get_raw all
        use that instead of the device's own ADC rate.
        """
        if self._resamplers:
            return float(self.resample_cfg.get('output_rate', 48000.0))
        return self._backends[dev_idx].actual_rate

    @staticmethod
    def _next_index(table, key, n):
        """Advance a pislm/4 per-stream sample counter by n samples and
        return the index the *next* n samples start at (i.e. the value the
        counter had before this call). One counter per (stream, key) --
        device for DATA, global channel for LEVEL, (band, channel) for
        BAND/BAND_LEVEL -- shared by the inline and DSP-pool code paths so
        both advance it identically. Called once per produced block,
        independent of whether the resulting frame is later dropped by
        network backpressure, so the index always reflects true elapsed
        samples."""
        start = table.get(key, 0)
        table[key] = start + n
        return start

    def _make_epoch(self):
        """Wall-clock/monotonic reference for sample index 0 of this scan."""
        now_wall = wall_time()
        return {
            'index': 0,
            'unix': now_wall,
            'utc': datetime.fromtimestamp(
                now_wall, tz=timezone.utc).strftime('%Y-%m-%dT%H:%M:%S.%fZ'),
            'monotonic': monotonic(),
            'source': 'system_clock',
            'note': 'NTP synchronization is not guaranteed',
        }

    def _ref_for(self, global_chan):
        return (20e-6 if self.sensitivity.get(global_chan, 1000.0) != 1000
                else 1.0)

    def _mcc(self):
        """First MCC 172 backend, for HAT-specific commands."""
        for backend in self._backends:
            if isinstance(backend, Mcc172Backend):
                return backend
        raise CommandError('no mcc172 device present')

    def config_snapshot(self):
        with self._lock:
            return {
                'running': self._running,
                'channels': self.channels,
                'num_channels': len(self._chan_map),
                'channel_map': self._chan_map.table(self._backends),
                'devices': [{'index': i, 'type': b.name,
                             'channels': self._chan_map.globals_for_device(i),
                             'actual_rate': b.actual_rate,
                             'full_scale_v': b.full_scale_v}
                            for i, b in enumerate(self._backends)],
                'sample_rate': self.sample_rate,
                'iepe': {str(g): self.iepe.get(g, 0) for g in self.channels},
                'sensitivity_mv_per_unit': {
                    str(g): self.sensitivity.get(g, 1000.0)
                    for g in self.channels},
                'units': self._units(),
                'stream_raw': self.stream_raw,
                'bands': {'enabled': bool(self.band_config.get('enabled')),
                          'output': self.band_config.get('output', 'level'),
                          'fraction': self.band_config.get('fraction', 3),
                          'order': self.band_config.get('order', 3),
                          'f_min': self.band_config.get('f_min', 20.0),
                          'f_max': self.band_config.get('f_max', 20000.0)},
                'weighting': {'frequency': self.freq_weighting,
                              'time': self.time_weighting},
                'level': {'enabled': self.level_enabled,
                          'output_rate': self.level_rate},
                'storage': {'buffer_seconds': self.buffer_seconds},
                'resample': {
                    'enabled': bool(self._resamplers) or bool(
                        self.resample_cfg.get('enabled')),
                    'active': bool(self._resamplers),
                    'output_rate': float(self.resample_cfg.get(
                        'output_rate', 48000.0)),
                    'taps': int(self.resample_cfg.get('taps', 32)),
                    'phases': int(self.resample_cfg.get('phases', 4096)),
                },
                'clock': {str(i): t.state()
                          for i, t in sorted(self._trackers.items())},
                'dsp': dict({'workers_configured': self.dsp_workers},
                            **(self._pool.stats() if self._pool
                               else {'workers': 0, 'mode': 'inline'})),
                'trigger': {
                    'enabled': bool(self.trigger_cfg.get('enabled')),
                    'source': self.trigger_cfg.get('source', 'gpio'),
                    'gpio_pin': self.trigger_cfg.get('gpio_pin', 17),
                    'pulse_ms': self.trigger_cfg.get('pulse_ms', 10.0),
                    'mode': 'RISING_EDGE',
                },
                'clock_sync_note': ('shared-trigger start aligns scan start; '
                                    'ADC clocks still drift (~ppm) between '
                                    'devices'),
                'epoch': self._epoch,
                'overload': {str(g): self._overload_count.get(g, 0)
                            for g in self.channels},
                'output': self._output_snapshot(),
                'ups': self._ups_snapshot(),
                'network': {
                    'stream_clients': (self._registry.stream_client_count()
                                       if self._registry else 0),
                    'stream_frames_dropped': (
                        self._registry.stream_frames_dropped()
                        if self._registry else 0),
                    'stream_frames_dropped_by_type': (
                        self._registry.stream_frames_dropped_by_type()
                        if self._registry else {}),
                },
            }

    def handshake(self):
        snap = self.config_snapshot()
        snap.update({'type': 'handshake',
                     'protocol': PROTOCOL_VERSION,
                     'dtype': 'float64', 'byte_order': 'little',
                     'interleave': 'channel-fastest-per-device'})
        table = self._band_table()
        if table:
            snap['band_table'] = table
        return snap

    def _band_table(self):
        """Per-device band tables, from the DSP pool or the inline banks.

        With workers, a device's channels may be split across several
        workers; their band layouts are identical (same rate and settings),
        so the entries are merged back into one table per device.
        """
        if self._pool is not None:
            merged = {}
            for worker, meta in zip(self._pool.workers,
                                    (self._pool.band_metadata.get(i)
                                     for i in range(
                                         len(self._pool.workers)))):
                if not meta:
                    continue
                dev = worker['spec']['device']
                entry = merged.setdefault(
                    dev, dict(meta, device=dev, channels=[]))
                entry['channels'].extend(meta.get('channels', []))
            for entry in merged.values():
                entry['channels'] = sorted(entry['channels'])
            return [merged[d] for d in sorted(merged)]
        if self._band_banks:
            return [dict(self._band_banks[i].metadata(), device=i)
                    for i in sorted(self._band_banks)]
        return []

    # -- streaming lifecycle ----------------------------------------------
    def start(self):
        with self._control_lock:
            with self._lock:
                if self._running:
                    raise CommandError('already running')
                for dev_idx, backend in enumerate(self._backends):
                    iepe_local = {}
                    sens_local = {}
                    for g in self._chan_map.globals_for_device(dev_idx):
                        _d, local = self._chan_map.resolve(g)
                        iepe_local[local] = self.iepe.get(g, 1)
                        sens_local[local] = self.sensitivity.get(g, 1000.0)
                    backend.configure(self.sample_rate, iepe_local, sens_local)

                triggered = bool(self.trigger_cfg.get('enabled'))
                use_gpio = (triggered and
                            self.trigger_cfg.get('source', 'gpio') == 'gpio')
                if use_gpio:
                    # Fail fast (before arming scans) if GPIO is unusable.
                    self._ensure_gpio()

                self._build_processing()

                started = []
                try:
                    for backend in self._backends:
                        if triggered:
                            backend.arm_trigger()
                        backend.start(triggered=triggered)
                        started.append(backend)
                except Exception:
                    for backend in started:
                        backend.stop()
                    raise
                self._running = True
                self._stop_event.clear()
                self._trigger_pending = (set(range(len(self._backends)))
                                         if triggered else set())

            self._scan_thread = threading.Thread(
                target=self._acquire, daemon=True)
            self._scan_thread.start()

            # Broadcast 'started' BEFORE any trigger pulse so it always
            # precedes the first data frames on the stream port.
            snapshot = self.handshake()
            snapshot['type'] = 'event'
            snapshot['event'] = 'started'
            if triggered:
                snapshot['trigger'] = {'armed': True, 'fired': False}
            if self._registry:
                self._registry.broadcast_message(snapshot)

            trigger_result = None
            if triggered:
                if use_gpio:
                    # Give the armed scans a moment to reach the wait state,
                    # then fire one rising edge for all devices.
                    sleep(0.25)
                    self._gpio.pulse(
                        self.trigger_cfg.get('pulse_ms', 10.0) / 1000.0)
                    trigger_result = self._wait_for_trigger(timeout=2.0)
                else:
                    trigger_result = {'armed': True, 'fired': False,
                                      'note': 'waiting for external edge'}

            result = self.config_snapshot()
            if trigger_result is not None:
                result['trigger_start'] = trigger_result
            return result

    def _ensure_gpio(self):
        """Open (or re-open) the trigger GPIO line for the configured pin."""
        from gpio_trigger import GpioTrigger
        pin = int(self.trigger_cfg.get('gpio_pin', 17))
        if self._gpio is not None and self._gpio.pin != pin:
            self._gpio.close()
            self._gpio = None
        if self._gpio is None:
            try:
                self._gpio = GpioTrigger(pin)
            except (RuntimeError, ValueError) as err:
                raise CommandError('trigger GPIO unavailable: {}'.format(err))
        return self._gpio

    def _wait_for_trigger(self, timeout):
        """Poll the devices until they all report the trigger edge."""
        deadline = 0.0
        step = 0.05
        status = {}
        while deadline <= timeout:
            with self._lock:
                status = {i: b.has_triggered()
                          for i, b in enumerate(self._backends)}
            if all(status.values()):
                break
            sleep(step)
            deadline += step
        return {'armed': True, 'fired': all(status.values()),
                'devices': {str(i): bool(v) for i, v in status.items()}}

    def _build_processing(self):
        """(Re)build ring buffers and the DSP: either a multi-process pool
        (default) or the inline single-process path."""
        self._close_pool()
        self._raw_buffers = {}
        self._band_banks = {}
        self._wsos = {}
        self._wzi = {}
        self._level = {}
        self._band_level = {}
        self._band_offset = {}

        # A new scan is a new time axis: every per-stream sample counter and
        # overload tally starts over at 0.
        self._data_count = {}
        self._level_count = {}
        self._band_count = {}
        self._band_level_count = {}
        self._overload_count = {g: 0 for g in self.channels}
        self._overload_last_emit = {}
        self._epoch = self._make_epoch()

        self._build_clock_sync()
        for dev_idx, backend in enumerate(self._backends):
            self._raw_buffers[dev_idx] = RawRingBuffer(
                self.buffer_seconds, backend.num_channels, self._rate(dev_idx))

        if self.dsp_workers != 0 and self._build_pool():
            return          # the pool owns all filter state

        self._build_inline_processing()

    def _build_clock_sync(self, verbose=True):
        """Create the per-device clock trackers and (if enabled) resamplers."""
        self._trackers = {}
        self._resamplers = {}
        self._retune_at = {}
        try:
            from clock_sync import ClockTracker, Resampler
        except ImportError as err:
            print('[clock] rate tracking disabled ({})'.format(err),
                  flush=True)
            return
        for dev_idx, backend in enumerate(self._backends):
            self._trackers[dev_idx] = ClockTracker(backend.actual_rate)
        if not self.resample_cfg.get('enabled'):
            return
        out_rate = float(self.resample_cfg.get('output_rate', 48000.0))
        for dev_idx, backend in enumerate(self._backends):
            self._resamplers[dev_idx] = Resampler(
                backend.actual_rate, out_rate, backend.num_channels,
                n_taps=int(self.resample_cfg.get('taps', 32)),
                n_phases=int(self.resample_cfg.get('phases', 4096)))
            self._retune_at[dev_idx] = 0.0
        if verbose:
            print('[clock] resampling every device to {:g} Hz'.format(
                out_rate), flush=True)

    def _band_output_mode(self):
        """'level', 'waveform', or None when band output is off."""
        if not self.band_config.get('enabled'):
            return None
        return self.band_config.get('output', 'level')

    def _build_pool(self):
        """Start the DSP worker processes. Returns False to fall back to
        inline processing (missing dependency, or nothing to compute)."""
        band_output = self._band_output_mode()
        if not (self.level_enabled or band_output):
            return False
        try:
            import dsp_pool
        except ImportError as err:
            print('[dsp] pool unavailable ({}); running inline'.format(err),
                  flush=True)
            return False

        device_channels = [self._chan_map.globals_for_device(i)
                           for i in range(len(self._backends))]
        max_workers = (None if self.dsp_workers < 0 else self.dsp_workers)
        plan = dsp_pool.plan_workers(device_channels, max_workers)
        if not plan:
            return False

        cfg = self.band_config
        common = {
            'level_enabled': self.level_enabled,
            'level_rate': self.level_rate,
            'freq_weighting': self.freq_weighting,
            'time_weighting': self.time_weighting,
            'band_output': band_output,
            'bands': {'f_min': cfg.get('f_min', 20.0),
                      'f_max': cfg.get('f_max', 20000.0),
                      'fraction': cfg.get('fraction', 3),
                      'order': cfg.get('order', 3),
                      'margin': cfg.get('margin', 1.0)},
        }
        for spec in plan:
            backend = self._backends[spec['device']]
            spec['rate'] = self._rate(spec['device'])
            spec['refs'] = {g: self._ref_for(g) for g in spec['channels']}

        # One second of samples per slot is ample for any read block.
        max_frames = int(max(self._rate(i)
                             for i in range(len(self._backends))))
        try:
            self._pool = dsp_pool.DspPool(plan, common, max_frames)
        except Exception as err:        # noqa: BLE001 - fall back safely
            print('[dsp] pool start failed ({}); running inline'.format(err),
                  flush=True)
            self._pool = None
            return False
        print('[dsp] {} worker processes: {}'.format(
            len(plan), [s['channels'] for s in plan]), flush=True)
        return True

    def _close_pool(self):
        if self._pool is not None:
            try:
                self._pool.close()
            except Exception:           # noqa: BLE001
                pass
            self._pool = None

    def _build_inline_processing(self):
        """Single-process DSP state (used when workers = 0 or unavailable)."""
        if self.band_config.get('enabled'):
            try:
                from band_filter import BandFilterBank
                for dev_idx, backend in enumerate(self._backends):
                    cfg = self.band_config
                    self._band_banks[dev_idx] = BandFilterBank(
                        self._rate(dev_idx),
                        self._chan_map.globals_for_device(dev_idx),
                        f_min=cfg.get('f_min', 20.0),
                        f_max=cfg.get('f_max', 20000.0),
                        fraction=cfg.get('fraction', 3),
                        order=cfg.get('order', 3),
                        margin=cfg.get('margin', 1.0))
            except ImportError as err:
                print('[bands] disabled (missing dependency: {})'.format(err),
                      flush=True)

        need_level = self.level_enabled
        need_band_level = (self._band_banks and
                           self.band_config.get('output', 'level') == 'level')
        if not (need_level or need_band_level):
            return
        try:
            import numpy as np
            import slm
        except ImportError as err:
            print('[slm] level output disabled (missing dependency: {})'
                  .format(err), flush=True)
            self.level_enabled = False
            return

        tau = slm.tau_for(self.time_weighting)
        if need_level:
            for dev_idx, backend in enumerate(self._backends):
                self._wsos[dev_idx] = slm.design_weighting_sos(
                    self.freq_weighting, self._rate(dev_idx))
                n_sec = (self._wsos[dev_idx].shape[0]
                         if self._wsos[dev_idx] is not None else 0)
                for g in self._chan_map.globals_for_device(dev_idx):
                    self._wzi[g] = np.zeros((n_sec, 2))
                    self._level[g] = slm.ExpLevel(
                        self._rate(dev_idx), tau, self.level_rate,
                        ref=self._ref_for(g))
        if need_band_level:
            for dev_idx, bank in self._band_banks.items():
                for band in bank.bands:
                    self._band_offset[(dev_idx, band['index'])] = \
                        slm.weighting_offset_db(
                            self.freq_weighting, band['center'])
                    for g in self._chan_map.globals_for_device(dev_idx):
                        self._band_level[(dev_idx, band['index'], g)] = \
                            slm.ExpLevel(band['decimated_rate'], tau,
                                         self.level_rate,
                                         ref=self._ref_for(g))

    def stop(self):
        with self._control_lock:
            with self._lock:
                if not self._running:
                    raise CommandError('not running')
            self._stop_event.set()
            thread = self._scan_thread
        if thread is not None:
            thread.join(timeout=5.0)
        return {'running': self.running}

    def _acquire(self):
        """Poll every device for new samples; store, process, broadcast."""
        try:
            while not self._stop_event.is_set():
                got_any = False
                now = monotonic()
                for dev_idx, backend in enumerate(self._backends):
                    with self._lock:
                        data, overrun = backend.read_new()
                    if overrun:
                        print('[scan] overrun on device {} ({})'.format(
                            dev_idx, backend.name), flush=True)
                        if self._registry:
                            self._registry.broadcast_message(
                                {'type': 'event', 'event': 'overrun',
                                 'device': dev_idx, 'kind': 'buffer'})
                        return
                    if data.size == 0:
                        continue
                    got_any = True
                    if dev_idx in self._trigger_pending:
                        # First samples after an armed start: edge arrived.
                        self._trigger_pending.discard(dev_idx)
                        if self._registry:
                            self._registry.broadcast_message(
                                {'type': 'event', 'event': 'triggered',
                                 'device': dev_idx})
                    # Clipping is checked on the raw ADC block, before any
                    # resampling could smear a hard clip's edge.
                    if self._registry:
                        self._check_overload(dev_idx, backend, data, now)
                    # Track the device's true rate against the Pi clock, and
                    # convert to the common grid if resampling is on. The
                    # Pi's own clock error cancels in the ratio between two
                    # devices, so this is what actually removes the drift.
                    tracker = self._trackers.get(dev_idx)
                    if tracker is not None:
                        tracker.update(data.size // backend.num_channels)
                    resampler = self._resamplers.get(dev_idx)
                    if resampler is not None:
                        if (tracker is not None and tracker.settled() and
                                now >= self._retune_at.get(dev_idx, 0.0)):
                            resampler.set_input_rate(tracker.measured_rate)
                            self._retune_at[dev_idx] = now + 10.0
                        data = resampler.process(data)
                        if data.size == 0:
                            continue

                    # The backend hands us a float64 array; the ring buffer,
                    # the DATA frame, and the DSP all share that one buffer.
                    self._raw_buffers[dev_idx].append(data)
                    # This counter is the DATA/RAW_DUMP grid for this device;
                    # advance it unconditionally (ring buffer always gets the
                    # data) so get_raw's start_index lines up with DATA even
                    # when stream_raw is off.
                    data_start = self._next_index(
                        self._data_count, dev_idx,
                        data.size // backend.num_channels)
                    if not self._registry:
                        continue
                    if self.stream_raw:
                        self._registry.broadcast_stream_frame(
                            data_frame(dev_idx, data_start, data.tobytes()),
                            'DATA')
                    if self._pool is not None:
                        # (frames, channels) view -- no copy.
                        self._pool.submit(
                            dev_idx,
                            data.reshape(-1, backend.num_channels))
                    else:
                        self._process_inline(dev_idx, backend, data)
                if self._pool is not None:
                    self._emit_pool_frames()
                if not got_any:
                    sleep(0.002)
        finally:
            with self._lock:
                for backend in self._backends:
                    try:
                        backend.stop()
                    except Exception:   # noqa: BLE001
                        pass
                self._running = False
            # An in-flight excitation output loses its start_index meaning
            # the instant the scan it was correlated against stops; the
            # already-running _poll_output watcher notices stop_output()'s
            # effect within one poll and broadcasts output_finished itself.
            if self._output_dev_idx is not None:
                with self._output_lock:
                    output_was_running = self._output_running
                if output_was_running:
                    try:
                        self._backends[self._output_dev_idx].stop_output()
                    except Exception:   # noqa: BLE001
                        pass
            # Flush anything the workers finished during shutdown, then
            # release the processes and their shared-memory slots.
            if self._pool is not None:
                try:
                    self._emit_pool_frames()
                except Exception:       # noqa: BLE001
                    pass
                self._close_pool()
            print('[scan] stopped', flush=True)
            if self._registry:
                self._registry.broadcast_message(
                    {'type': 'event', 'event': 'stopped'})

    #: fraction of ADC full-scale counted as clipped (matched against the
    #: raw voltage reconstructed from the calibrated sample, not the
    #: calibrated value itself -- clipping happens at the ADC, and judging
    #: it post-calibration would make the threshold move with sensitivity).
    _OVERLOAD_THRESHOLD = 0.99

    def _check_overload(self, dev_idx, backend, raw_block, now):
        """Flag ADC clipping per channel, throttled to at most one event per
        channel per level-output period; tallies persist even when
        throttled so get_config/handshake can report the full count."""
        full_scale = getattr(backend, 'full_scale_v', None)
        if not full_scale:
            return
        import numpy as np
        block = raw_block.reshape(-1, backend.num_channels)
        threshold = self._OVERLOAD_THRESHOLD * full_scale
        period = (1.0 / self.level_rate) if self.level_rate > 0 else 1.0
        globals_ = self._chan_map.globals_for_device(dev_idx)
        dev_start = self._data_count.get(dev_idx, 0)
        for ci, g in enumerate(globals_):
            sens = self.sensitivity.get(g, 1000.0) / 1000.0  # mV/unit -> V/unit
            col = block[:, ci]
            raw_volts = col * sens if sens else col
            clipped = np.abs(raw_volts) >= threshold
            n_clip = int(clipped.sum())
            if n_clip == 0:
                continue
            self._overload_count[g] = self._overload_count.get(g, 0) + n_clip
            last = self._overload_last_emit.get(g, 0.0)
            if now - last < period:
                continue
            self._overload_last_emit[g] = now
            peak = float(np.max(np.abs(raw_volts)))
            self._registry.broadcast_message({
                'type': 'event', 'event': 'overload',
                'device': dev_idx, 'channel': g,
                'start_index': dev_start, 'samples': n_clip,
                'peak': peak, 'units': 'V', 'full_scale': full_scale,
            })

    # -- analog output: excitation signal for reverberation measurement ----
    # DT9837A only (single channel; see devices.py). Independent of the AI
    # scan's running state, except that start_index (correlating the
    # output's first sample to the AI DATA grid, §2.1) is only meaningful
    # while the scan is running.
    #
    # Accuracy note: start_index is a *software* timestamp correlation --
    # the AI DATA-grid counter is read as close as possible to the instant
    # a_out_scan() is issued -- not a hardware-verified alignment. It is a
    # good starting estimate, dominated by USB scheduling latency (small
    # but not zero); a client doing MLS deconvolution (which needs exact
    # circular alignment) should still refine the alignment by
    # cross-correlating against the known sequence rather than trusting
    # this index to the sample. True hardware synchronization would need
    # the same GPIO trigger used for synchronized AI start (§ "Synchronized
    # start" in PROTOCOL.md), but that only works when arming a scan from a
    # full stop -- it cannot mark an arbitrary future edge in an AI scan
    # that is already running, which is the whole point of this feature
    # ("does not require the scan to be stopped").
    def _output_snapshot(self):
        if self._output_dev_idx is None:
            return {'available': False}
        backend = self._backends[self._output_dev_idx]
        cfg = self._output_cfg
        return {
            'available': True,
            'device': self._output_dev_idx,
            'channels': [0],
            'output_rate': (cfg['output_rate'] if cfg
                            else backend.ao_max_rate),
            'full_scale_volts': backend.ao_full_scale_v,
            'running': self._output_running,
            'signal': cfg['signal'] if cfg else None,
        }

    def _ups_snapshot(self):
        """Best-effort read of the UPS status file written by the separate
        pislm-shutdown-button service (see its module docstring) -- pislm
        never touches the I2C bus itself, just surfaces the latest
        snapshot so a client can check battery status without a second
        connection. Missing/stale/malformed is reported, never raised:
        a UPS (or its monitor service) being absent must not affect
        anything else pislm does."""
        path = self.ups_cfg.get('status_file', '/run/pislm-ups-status.json')
        try:
            with open(path) as f:
                data = json.load(f)
        except (OSError, ValueError):
            return {'available': False}
        age = wall_time() - data.get('timestamp', 0)
        stale_after = self.ups_cfg.get('stale_after_seconds', 60.0)
        return {
            'available': True,
            'stale': age > stale_after,
            'age_seconds': round(age, 1),
            'percent': data.get('percent'),
            'bus_voltage_v': data.get('bus_voltage_v'),
            'current_ma': data.get('current_ma'),
            'power_w': data.get('power_w'),
            'low_battery_hold_seconds': data.get('low_battery_hold_seconds'),
        }

    def _require_output_configured(self):
        if self._output_dev_idx is None:
            raise CommandError('no analog output device available')
        return self._backends[self._output_dev_idx]

    def _cmd_set_output(self, req):
        backend = self._require_output_configured()
        with self._output_lock:
            if self._output_running:
                raise CommandError(
                    'output is running; send "output_stop" first')
        import numpy as np
        import excitation

        signal = str(req.get('signal', 'sweep'))
        if signal not in ('white', 'pink', 'sweep', 'mls'):
            raise CommandError(
                'signal must be one of white, pink, sweep, mls')
        channel = int(req.get('channel', 0))
        if channel != 0:
            raise CommandError(
                'the DT9837A analog output has only channel 0')
        level_dbfs = float(req.get('level_dbfs', -20.0))
        if level_dbfs > 0:
            raise CommandError('level_dbfs must be <= 0')
        seconds = float(req.get('seconds', 3.0))
        f_min = float(req.get('f_min', 50.0))
        f_max = float(req.get('f_max', 5000.0))
        mls_order = int(req.get('mls_order', 16))
        tail_seconds = float(req.get('tail_seconds', 1.0))
        if tail_seconds < 0:
            raise CommandError('tail_seconds must be >= 0')
        repeats = max(1, int(req.get('repeats', 1)))

        rate = self.resample_cfg.get('output_rate', 48000.0) if \
            self.resample_cfg.get('enabled') else 48000.0
        rate = max(backend.ao_min_rate, min(rate, backend.ao_max_rate))

        try:
            if signal == 'mls':
                sig = excitation.generate_mls(mls_order, level_dbfs, repeats)
            elif signal == 'sweep':
                sig = excitation.generate_sweep(
                    seconds, f_min, f_max, rate, level_dbfs)
                sig = np.tile(sig, repeats) if repeats > 1 else sig
            else:
                sig = excitation.generate_noise(
                    signal, seconds, rate, f_min, f_max, level_dbfs)
                sig = np.tile(sig, repeats) if repeats > 1 else sig
        except ValueError as err:
            raise CommandError(str(err))

        if tail_seconds > 0:
            tail = np.zeros(int(round(tail_seconds * rate)))
            sig = np.concatenate([sig, tail])

        volts = sig * backend.ao_full_scale_v
        total_samples = int(volts.size)
        total_seconds = total_samples / rate

        self._output_cfg = {
            'signal': signal, 'seconds': seconds, 'level_dbfs': level_dbfs,
            'f_min': f_min, 'f_max': f_max, 'mls_order': mls_order,
            'channel': channel, 'tail_seconds': tail_seconds,
            'repeats': repeats, 'output_rate': rate,
            'total_samples': total_samples, 'total_seconds': total_seconds,
            'device': self._output_dev_idx, 'volts': volts,
        }
        result = dict(self._output_cfg)
        del result['volts']
        return result

    def _cmd_output_start(self, req):
        backend = self._require_output_configured()
        with self._output_lock:
            if self._output_running:
                raise CommandError('output already running')
            if self._output_cfg is None:
                raise CommandError('call set_output first')
            cfg = self._output_cfg
            start_index = (self._data_count.get(self._output_dev_idx)
                           if self._running else None)
            actual_rate = backend.start_output(cfg['volts'], cfg['output_rate'])
            self._output_running = True
            self._output_start_index = start_index
            self._output_samples_played = 0
        threading.Thread(target=self._poll_output,
                         args=(backend, cfg['total_samples']),
                         daemon=True).start()
        snapshot = {'type': 'event', 'event': 'output_started',
                   'signal': cfg['signal'], 'start_index': start_index,
                   'total_seconds': cfg['total_seconds'],
                   'device': self._output_dev_idx, 'channel': cfg['channel']}
        if self._registry:
            self._registry.broadcast_message(snapshot)
        return {'running': True, 'signal': cfg['signal'],
                'total_seconds': cfg['total_seconds'],
                'start_index': start_index, 'device': self._output_dev_idx}

    def _poll_output(self, backend, expected_total):
        """Background completion watcher for one output_start (§ above) --
        runs independent of the AI scan's own thread/lifecycle. Owns
        broadcasting 'output_finished' for both natural completion and a
        manual output_stop, so it is never broadcast twice."""
        played = 0
        while True:
            played, done = backend.output_progress()
            with self._output_lock:
                self._output_samples_played = played
            if done:
                break
            sleep(0.02)
        with self._output_lock:
            self._output_running = False
            start_index = self._output_start_index
        end_index = (start_index + played) if start_index is not None else None
        if self._registry:
            self._registry.broadcast_message({
                'type': 'event', 'event': 'output_finished',
                'samples_played': played, 'end_index': end_index,
                'completed': played >= expected_total,
            })

    def _cmd_output_stop(self, req):
        backend = self._require_output_configured()
        with self._output_lock:
            if not self._output_running:
                raise CommandError('output is not running')
        played, _done = backend.output_progress()
        backend.stop_output()   # ramps to 0 V, then stops the scan
        return {'running': False, 'samples_played': played}

    def _cmd_output_status(self, req):
        if self._output_dev_idx is None:
            return {'running': False, 'signal': None,
                    'elapsed_seconds': 0.0, 'remaining_seconds': 0.0,
                    'start_index': None, 'device': None, 'channel': None}
        with self._output_lock:
            running = self._output_running
            played = self._output_samples_played
            start_index = self._output_start_index
            cfg = self._output_cfg
        if cfg is None:
            return {'running': False, 'signal': None,
                    'elapsed_seconds': 0.0, 'remaining_seconds': 0.0,
                    'start_index': None, 'device': self._output_dev_idx,
                    'channel': None}
        elapsed = played / cfg['output_rate'] if running else 0.0
        remaining = max(0.0, cfg['total_seconds'] - elapsed) if running else 0.0
        return {'running': running, 'signal': cfg['signal'],
                'elapsed_seconds': elapsed, 'remaining_seconds': remaining,
                'start_index': start_index if running else None,
                'device': self._output_dev_idx, 'channel': cfg['channel']}

    _POOL_FRAME_BUILDERS = {
        'level': level_frame,
        'band': band_frame,
        'band_level': band_level_frame,
    }
    # kind -> (counter table attr, key-from-args function). 'level' args are
    # (channel,); 'band'/'band_level' args are (band_index, channel) -- the
    # same shape the inline path keys its counters with (see _emit_bands
    # etc.), so pool and inline runs advance identical counters.
    _POOL_COUNTER_TABLE = {
        'level': ('_level_count', lambda args: args[0]),
        'band': ('_band_count', lambda args: args),
        'band_level': ('_band_level_count', lambda args: args),
    }
    _POOL_KIND_NAMES = {
        'level': 'LEVEL', 'band': 'BAND', 'band_level': 'BAND_LEVEL',
    }
    # A worker emits a '..._gap' entry (base kind, key, n_skipped_samples)
    # instead of a data frame when it skip()-ed a dropped-block gap (see
    # _WorkerState.process) -- no frame to send, just the counter to
    # advance, so start_index on the next real frame correctly reflects the
    # gap instead of reporting false continuity across it.
    _POOL_GAP_KINDS = {
        'level_gap': 'level', 'band_gap': 'band', 'band_level_gap': 'band_level',
    }

    def _emit_pool_frames(self):
        """Broadcast whatever the DSP workers have finished."""
        for kind, args, payload in self._pool.drain():
            gap_base = self._POOL_GAP_KINDS.get(kind)
            if gap_base is not None:
                table_name, key_fn = self._POOL_COUNTER_TABLE[gap_base]
                table = getattr(self, table_name)
                self._next_index(table, key_fn(args), payload)
                continue
            builder = self._POOL_FRAME_BUILDERS.get(kind)
            if builder is None:
                continue
            table_name, key_fn = self._POOL_COUNTER_TABLE[kind]
            table = getattr(self, table_name)
            start = self._next_index(table, key_fn(args), len(payload) // 8)
            self._registry.broadcast_stream_frame(
                builder(*(args + (start, payload))), self._POOL_KIND_NAMES[kind])
        if self._pool.errors:
            for wid, err in self._pool.errors:
                print('[dsp] worker {} error: {}'.format(wid, err), flush=True)
            self._pool.errors = []

    def _process_inline(self, dev_idx, backend, block):
        """Single-process DSP path (workers = 0 or pool unavailable)."""
        if self.level_enabled and self._level:
            self._emit_levels(dev_idx, backend, block)
        bank = self._band_banks.get(dev_idx)
        if bank is not None:
            if self.band_config.get('output', 'level') == 'waveform':
                self._emit_bands(bank, block)
            else:
                self._emit_band_levels(dev_idx, bank, block)

    def _emit_bands(self, bank, raw_data):
        """BAND (waveform) frames; bank yields GLOBAL channel labels."""
        for band_index, g_chan, samples in bank.process(raw_data):
            start = self._next_index(
                self._band_count, (band_index, g_chan), samples.size)
            self._registry.broadcast_stream_frame(
                band_frame(band_index, g_chan, start,
                          samples.astype('<f8').tobytes()), 'BAND')

    def _emit_levels(self, dev_idx, backend, raw_data):
        """Broadband weighted level (dB) LEVEL frames for one device block."""
        import numpy as np
        from scipy import signal
        data = np.asarray(raw_data, dtype=np.float64).reshape(
            -1, backend.num_channels)
        sos = self._wsos.get(dev_idx)
        for ci, g_chan in enumerate(
                self._chan_map.globals_for_device(dev_idx)):
            x = data[:, ci]
            if sos is not None:
                x, self._wzi[g_chan] = signal.sosfilt(
                    sos, x, zi=self._wzi[g_chan])
            levels = self._level[g_chan].process(x)
            if levels.size:
                start = self._next_index(
                    self._level_count, g_chan, levels.size)
                self._registry.broadcast_stream_frame(
                    level_frame(g_chan, start, levels.astype('<f8').tobytes()),
                    'LEVEL')

    def _emit_band_levels(self, dev_idx, bank, raw_data):
        """Per-band weighted level (dB) BAND_LEVEL frames for one block."""
        for band_index, g_chan, samples in bank.process(raw_data):
            integrator = self._band_level.get((dev_idx, band_index, g_chan))
            if integrator is None:
                continue
            levels = integrator.process(samples)
            if levels.size:
                levels = levels + self._band_offset.get(
                    (dev_idx, band_index), 0.0)
                start = self._next_index(
                    self._band_level_count, (band_index, g_chan), levels.size)
                self._registry.broadcast_stream_frame(
                    band_level_frame(band_index, g_chan, start,
                                     levels.astype('<f8').tobytes()),
                    'BAND_LEVEL')

    # -- command dispatch --------------------------------------------------
    def dispatch(self, request):
        cmd = request.get('cmd')
        if not cmd:
            raise CommandError('missing "cmd"')
        handler = self._HANDLERS.get(cmd)
        if handler is None:
            raise CommandError('unknown command: {}'.format(cmd))
        # start/stop manage their own locking (they join the scan thread,
        # which itself needs the device lock). get_raw/get_metrics only read
        # the ring buffers (own lock) and run-frozen config, and can take
        # seconds on large windows -- run them without the device lock so
        # they never stall acquisition. Everything else runs under it.
        # calibrate bounces the scan (stop/start), so it must not hold the
        # device lock either.
        if cmd in ('start', 'stop', 'get_raw', 'get_metrics', 'calibrate'):
            return handler(self, request)
        with self._lock:
            return handler(self, request)

    # ---- query handlers (allowed anytime) ----
    def _cmd_ping(self, _req):
        return {'pong': True}

    def _cmd_get_config(self, _req):
        return self.config_snapshot()

    def _cmd_status(self, _req):
        armed = bool(self.trigger_cfg.get('enabled')) and self._running
        return {'running': self._running,
                'devices': [{'index': i, 'type': b.name,
                             'running': b.running,
                             'actual_rate': b.actual_rate,
                             'effective_rate': self._rate(i),
                             'clock': (self._trackers[i].state()
                                       if i in self._trackers else None),
                             'triggered': (b.has_triggered()
                                           if armed else None)}
                            for i, b in enumerate(self._backends)]}

    def _cmd_info(self, _req):
        return {'devices': [dict(b.info(), index=i,
                                 channels=self._chan_map.globals_for_device(i))
                            for i, b in enumerate(self._backends)],
                'channel_map': self._chan_map.table(self._backends),
                'num_channels': len(self._chan_map)}

    def _cmd_get_sensitivity(self, req):
        g = int(req['channel'])
        self._resolve(g)
        return {'channel': g,
                'sensitivity': self.sensitivity.get(g, 1000.0)}

    def _cmd_get_iepe(self, req):
        g = int(req['channel'])
        self._resolve(g)
        return {'channel': g, 'mode': self.iepe.get(g, 0)}

    def _cmd_get_clock(self, _req):
        return {'devices': [{'index': i, 'type': b.name,
                             'actual_rate': b.actual_rate}
                            for i, b in enumerate(self._backends)],
                'requested_rate': self.sample_rate,
                'synchronized': False}

    def _cmd_calibration_read(self, req):
        g = int(req['channel'])
        dev_idx, local = self._resolve(g)
        backend = self._backends[dev_idx]
        if not isinstance(backend, Mcc172Backend):
            raise CommandError('calibration_read is mcc172-only')
        slope, offset = backend.calibration_read(local)
        return {'channel': g, 'slope': slope, 'offset': offset}

    def _cmd_blink_led(self, req):
        count = int(req.get('count', 1))
        blinked = []
        want = req.get('device')
        for i, backend in enumerate(self._backends):
            if want is not None and int(want) != i:
                continue
            try:
                backend.blink(count)
                blinked.append(i)
            except Exception:   # noqa: BLE001 - not all devices support it
                pass
        return {'count': count, 'devices': blinked}

    # ---- streaming ----
    def _cmd_start(self, _req):
        return self.start()

    def _cmd_stop(self, _req):
        return self.stop()

    # ---- configuration handlers (require the scan to be stopped) ----
    def _cmd_set_sensitivity(self, req):
        self._require_stopped()
        g = int(req['channel'])
        dev_idx, local = self._resolve(g)
        value = float(req['value'])
        self._backends[dev_idx].set_sensitivity(local, value)
        self.sensitivity[g] = value
        return {'channel': g, 'sensitivity': value,
                'units': self._units().get(str(g))}

    def _cmd_set_resample(self, req):
        """Configure resampling of every device onto one common rate.

        Fields: enabled (bool); output_rate (Hz); taps; phases. Applies
        from the next start.
        """
        self._require_stopped()
        cfg = self.resample_cfg
        if 'enabled' in req:
            cfg['enabled'] = bool(req['enabled'])
        if 'output_rate' in req:
            rate = float(req['output_rate'])
            if not 1000.0 <= rate <= 200000.0:
                raise CommandError('output_rate must be 1000..200000 Hz')
            cfg['output_rate'] = rate
        if 'taps' in req:
            taps = int(req['taps'])
            if not 8 <= taps <= 256 or taps % 2:
                raise CommandError('taps must be even, 8..256')
            cfg['taps'] = taps
        if 'phases' in req:
            phases = int(req['phases'])
            if not 64 <= phases <= 65536:
                raise CommandError('phases must be 64..65536')
            cfg['phases'] = phases
        return {'enabled': bool(cfg.get('enabled')),
                'output_rate': float(cfg.get('output_rate', 48000.0)),
                'taps': int(cfg.get('taps', 32)),
                'phases': int(cfg.get('phases', 4096)),
                'note': 'applies from the next start; device ADC rates are '
                        'unchanged, the streams are converted to this grid'}

    def _cmd_calibrate(self, req):
        """Calibrate a channel against an acoustic calibrator.

        Fit the calibrator, leave the scan running, and send this command.
        It measures the buffered signal, derives the sensitivity that makes
        that tone read the calibrator's level, and (by default) applies it.

        Fields: channel (global); level_db (calibrator SPL, default 94);
        seconds (measurement window, default 3); freq (calibrator tone,
        default 1000); bandpass (reject background noise, default true);
        apply (default true -- briefly stops the scan to write it).
        """
        import math
        import numpy as np

        g = int(req['channel'])
        dev_idx, local = self._resolve(g)
        backend = self._backends[dev_idx]
        target_db = float(req.get('level_db', 94.0))
        seconds = float(req.get('seconds', 3.0))
        freq = float(req.get('freq', 1000.0))
        use_bandpass = req.get('bandpass', True)
        do_apply = req.get('apply', True)
        if seconds <= 0:
            raise CommandError('seconds must be > 0')

        buffer_ = self._raw_buffers.get(dev_idx)
        if buffer_ is None:
            raise CommandError('no data buffered; start a scan first')
        rate = self._rate(dev_idx)
        flat = buffer_.get_recent(seconds, rate)
        nch = backend.num_channels
        # A calibrator tone needs only a fraction of a second for a stable
        # RMS, so use whatever is buffered above that floor and report the
        # duration actually used rather than failing on a short buffer.
        min_frames = max(1, int(0.1 * rate))
        if flat.size < nch * min_frames:
            raise CommandError(
                'not enough data buffered ({:.2f} s of {:.1f} s requested, '
                '{:.2f} s minimum); let the scan run a moment first'.format(
                    flat.size / nch / rate, seconds,
                    min_frames / rate))
        ci = backend.channels.index(local)
        x = flat.reshape(-1, nch)[:, ci]

        # A calibrator is a pure tone; a 1/3-octave band around it rejects
        # background noise that would otherwise bias the RMS upward.
        used_bandpass = False
        if use_bandpass:
            try:
                from scipy import signal as _sig
                edge = 2.0 ** (1.0 / 6.0)          # 1/3-octave half-width
                lo, hi = freq / edge, freq * edge
                nyq = rate / 2.0
                if 0 < lo < hi < nyq:
                    sos = _sig.butter(4, [lo, hi], btype='band',
                                      fs=rate, output='sos')
                    # Filter twice (forward/backward) for zero phase, and
                    # drop the edges where the filter is still settling.
                    y = _sig.sosfiltfilt(sos, x)
                    skip = min(int(0.05 * rate), y.size // 4)
                    x = y[skip:y.size - skip] if skip else y
                    used_bandpass = True
            except ImportError:
                pass

        measured_rms = float(np.sqrt(np.mean(np.square(x))))
        if not measured_rms > 0:
            raise CommandError('measured signal is silent; check the '
                               'calibrator, the cable, and IEPE power')

        old_sens = self.sensitivity.get(g, 1000.0)
        # The reading is in Pa when calibrated, volts otherwise; either way
        # the raw volts are measured_rms * old_sens / 1000, and we want the
        # new sensitivity to turn those volts into the calibrator pressure.
        target_pa = 20e-6 * (10.0 ** (target_db / 20.0))
        new_sens = old_sens * measured_rms / target_pa
        if not (0 < new_sens < 1e7):
            raise CommandError(
                'implausible sensitivity {:.4g} mV/Pa; check level_db and '
                'that the calibrator is seated'.format(new_sens))
        # Round once, here: the value reported is exactly the value applied
        # and later saved, so a client can compare them.
        new_sens = round(new_sens, 4)
        change_db = 20.0 * math.log10(new_sens / old_sens)
        # The level the CURRENT calibration reports for this tone: true SPL
        # once the channel is calibrated (ref 20 uPa), dBV while it is not.
        ref = self._ref_for(g)
        measured_db = 20.0 * math.log10(measured_rms / ref)

        result = {
            'channel': g, 'device': dev_idx,
            'target_level_db': target_db,
            'measured_level_db': round(measured_db, 2),
            'measured_units': 'dB re 20uPa' if ref != 1.0 else 'dBV',
            'old_sensitivity': old_sens,
            'new_sensitivity': new_sens,
            'change_db': round(change_db, 2),
            'seconds': round(x.size / rate, 3),
            'freq': freq, 'bandpass': used_bandpass,
            'applied': False,
            'saved': False,
        }
        if not do_apply:
            result['note'] = ('not applied; send set_sensitivity with '
                              'new_sensitivity, or repeat with apply=true')
            return result

        # Sensitivity can only be written while stopped; bounce the scan.
        was_running = self._running
        if was_running:
            self.stop()
        self._backends[dev_idx].set_sensitivity(local, new_sens)
        self.sensitivity[g] = new_sens
        result['applied'] = True
        result['units'] = self._units().get(str(g))
        if was_running:
            self.start()
            result['restarted'] = True
        result['note'] = ('applied to the running configuration only; send '
                          'save_config to keep it across restarts')
        return result

    def _cmd_save_config(self, req):
        """Write the current calibration (and optionally other runtime
        settings) back to config.ini so they survive a restart."""
        from datetime import datetime
        path = req.get('path') or self.config_path
        if not path:
            raise CommandError('no config file path known')

        values = {}
        # Calibration: per-device sensitivity keys, using LOCAL channels.
        for g in self.channels:
            dev_idx, local = self._chan_map.resolve(g)
            section = self._backends[dev_idx].name
            values[(section, 'sensitivity_ch{}'.format(local))] = \
                '{:g}'.format(self.sensitivity.get(g, 1000.0))

        if req.get('include_settings'):
            values.update({
                ('acquisition', 'sample_rate'): '{:g}'.format(
                    self.sample_rate),
                ('acquisition', 'stream_raw'): str(self.stream_raw).lower(),
                ('weighting', 'frequency'): self.freq_weighting,
                ('weighting', 'time'): self.time_weighting,
                ('level', 'enabled'): str(self.level_enabled).lower(),
                ('level', 'output_rate'): '{:g}'.format(self.level_rate),
                ('storage', 'buffer_seconds'): '{:g}'.format(
                    self.buffer_seconds),
                ('dsp', 'workers'): str(self.dsp_workers),
                ('bands', 'enabled'): str(
                    bool(self.band_config.get('enabled'))).lower(),
                ('bands', 'output'): self.band_config.get('output', 'level'),
                ('bands', 'f_min'): '{:g}'.format(
                    self.band_config.get('f_min', 20.0)),
                ('bands', 'f_max'): '{:g}'.format(
                    self.band_config.get('f_max', 20000.0)),
                ('trigger', 'sync_start'): str(
                    bool(self.trigger_cfg.get('enabled'))).lower(),
                ('trigger', 'gpio_pin'): str(
                    self.trigger_cfg.get('gpio_pin', 17)),
            })

        stamp = datetime.now().strftime('%Y-%m-%d %H:%M')
        try:
            written = update_ini(path, values, note='saved ' + stamp)
        except OSError as err:
            raise CommandError('could not write {}: {}'.format(path, err))
        return {'path': path, 'saved': sorted(written), 'timestamp': stamp,
                'sensitivity_mv_per_unit': {
                    str(g): self.sensitivity.get(g, 1000.0)
                    for g in self.channels}}

    def _cmd_set_iepe(self, req):
        self._require_stopped()
        g = int(req['channel'])
        dev_idx, local = self._resolve(g)
        mode = 1 if req.get('mode') in (1, '1', True, 'true', 'on') else 0
        self._backends[dev_idx].set_iepe(local, mode)
        self.iepe[g] = mode
        return {'channel': g, 'mode': mode}

    def _cmd_set_sample_rate(self, req):
        self._require_stopped()
        rate = float(req['sample_rate'])
        self.sample_rate = rate
        self._apply_static_config()   # each device recomputes its actual rate
        self._raw_buffers = {}        # buffered raw no longer matches
        return {'requested_rate': rate,
                'devices': [{'index': i, 'type': b.name,
                             'actual_rate': b.actual_rate}
                            for i, b in enumerate(self._backends)]}

    def _cmd_set_channels(self, req):
        self._require_stopped()
        dev_idx = int(req.get('device', 0))
        if not 0 <= dev_idx < len(self._backends):
            raise CommandError('device must be 0..{}'.format(
                len(self._backends) - 1))
        channels = req.get('channels')
        if not isinstance(channels, list) or not channels:
            raise CommandError('"channels" must be a non-empty list')
        backend = self._backends[dev_idx]
        old = backend.channels
        backend.channels = sorted(set(int(c) for c in channels))
        try:
            if backend.name == 'dt9837a' and \
                    backend.channels != list(range(len(backend.channels))):
                raise CommandError(
                    'dt9837a channels must be contiguous from 0')
            backend.num_channels = len(backend.channels)
        except CommandError:
            backend.channels = old
            backend.num_channels = len(old)
            raise
        # Global numbering changes: rebuild the map and per-channel state.
        self._chan_map = ChannelMap(self._backends)
        self.iepe = {g: self.iepe.get(g, 1) for g in self.channels}
        self.sensitivity = {g: self.sensitivity.get(g, 1000.0)
                            for g in self.channels}
        self._raw_buffers = {}
        return {'device': dev_idx, 'channels': backend.channels,
                'channel_map': self._chan_map.table(self._backends)}

    def _cmd_set_trigger(self, req):
        """Configure the synchronized (shared rising-edge) start.

        Fields: enable (bool); source 'gpio' (the Pi pulses gpio_pin on
        start) or 'external' (the scans arm and wait for a user-supplied
        edge); gpio_pin; pulse_ms. Applies from the next start.
        """
        from gpio_trigger import MCC172_RESERVED_PINS
        self._require_stopped()
        cfg = self.trigger_cfg
        enable = req.get('enable', True)
        cfg['enabled'] = bool(enable) and enable not in ('false', 'off', 0)
        if 'source' in req:
            source = str(req['source']).lower()
            if source not in ('gpio', 'external'):
                raise CommandError("source must be 'gpio' or 'external'")
            cfg['source'] = source
        if 'gpio_pin' in req:
            pin = int(req['gpio_pin'])
            if pin in MCC172_RESERVED_PINS:
                raise CommandError(
                    'GPIO {} is used by the MCC 172 HAT; choose another '
                    'pin (e.g. 17, 27, 22)'.format(pin))
            if not 0 <= pin <= 27:
                raise CommandError('gpio_pin must be a BCM number 0..27')
            cfg['gpio_pin'] = pin
        if 'pulse_ms' in req:
            pulse = float(req['pulse_ms'])
            if pulse <= 0:
                raise CommandError('pulse_ms must be > 0')
            cfg['pulse_ms'] = pulse
        return {'enabled': cfg['enabled'],
                'source': cfg.get('source', 'gpio'),
                'gpio_pin': cfg.get('gpio_pin', 17),
                'pulse_ms': cfg.get('pulse_ms', 10.0),
                'mode': 'RISING_EDGE',
                'note': 'rising edge only (DT9837A external trigger '
                        'supports rising edges only)'}

    def _cmd_set_options(self, req):
        self._require_stopped()
        if 'stream_raw' in req:
            self.stream_raw = bool(req['stream_raw'])
        return {'stream_raw': self.stream_raw}

    def _cmd_set_bands(self, req):
        self._require_stopped()
        cfg = self.band_config
        if 'enabled' in req:
            cfg['enabled'] = bool(req['enabled'])
        if 'output' in req:
            if req['output'] not in ('level', 'waveform'):
                raise CommandError("band output must be 'level' or 'waveform'")
            cfg['output'] = req['output']
        for key in ('f_min', 'f_max', 'margin'):
            if key in req:
                cfg[key] = float(req[key])
        for key in ('fraction', 'order'):
            if key in req:
                cfg[key] = int(req[key])
        table = None
        if cfg.get('enabled'):
            try:
                from band_filter import BandFilterBank
            except ImportError as err:
                raise CommandError(
                    'band output needs numpy + scipy on the Pi ({})'.format(
                        err))
            table = []
            for dev_idx, backend in enumerate(self._backends):
                bank = BandFilterBank(
                    self._rate(dev_idx),
                    self._chan_map.globals_for_device(dev_idx),
                    f_min=cfg.get('f_min', 20.0),
                    f_max=cfg.get('f_max', 20000.0),
                    fraction=cfg.get('fraction', 3),
                    order=cfg.get('order', 3),
                    margin=cfg.get('margin', 1.0))
                table.append(dict(bank.metadata(), device=dev_idx))
        return {'enabled': bool(cfg.get('enabled')),
                'output': cfg.get('output', 'level'),
                'fraction': cfg.get('fraction', 3),
                'order': cfg.get('order', 3),
                'f_min': cfg.get('f_min', 20.0),
                'f_max': cfg.get('f_max', 20000.0),
                'band_table': table}

    def _cmd_set_weighting(self, req):
        self._require_stopped()
        if 'frequency' in req:
            freq = str(req['frequency']).upper()
            if freq not in ('A', 'C', 'Z'):
                raise CommandError("frequency weighting must be A, C, or Z")
            self.freq_weighting = freq
        if 'time' in req:
            tw = str(req['time']).capitalize()
            if tw not in ('Fast', 'Slow', 'Impulse'):
                raise CommandError("time weighting must be Fast, Slow, Impulse")
            self.time_weighting = tw
        return {'frequency': self.freq_weighting, 'time': self.time_weighting}

    def _cmd_set_level(self, req):
        self._require_stopped()
        if 'enabled' in req:
            self.level_enabled = bool(req['enabled'])
        if 'output_rate' in req:
            rate = float(req['output_rate'])
            if rate <= 0:
                raise CommandError('output_rate must be > 0')
            self.level_rate = rate
        return {'enabled': self.level_enabled, 'output_rate': self.level_rate}

    def _cmd_set_storage(self, req):
        self._require_stopped()
        if 'buffer_seconds' in req:
            seconds = float(req['buffer_seconds'])
            if seconds <= 0:
                raise CommandError('buffer_seconds must be > 0')
            self.buffer_seconds = seconds
            self._raw_buffers = {}
        return {'buffer_seconds': self.buffer_seconds}

    def _cmd_set_dsp(self, req):
        """Set the number of DSP worker processes (-1 auto, 0 inline)."""
        self._require_stopped()
        if 'workers' in req:
            workers = int(req['workers'])
            if workers < -1:
                raise CommandError('workers must be -1 (auto), 0, or more')
            self.dsp_workers = workers
        import os as _os
        return {'workers_configured': self.dsp_workers,
                'cpu_count': _os.cpu_count(),
                'note': 'applies from the next start; a worker serves one '
                        'device, so the effective minimum is one per device'}

    def _cmd_get_raw(self, req):
        """Dump the most recent buffered raw samples to the stream clients as
        chunked RAW_DUMP frames. The copy and the send run outside the device
        lock so a large dump can never stall acquisition."""
        if not self._raw_buffers:
            raise CommandError('no data buffered yet; start a scan first')
        if self._registry is None or \
                self._registry.stream_client_count() == 0:
            raise CommandError(
                'no stream client connected to receive the dump')
        seconds = float(req.get('seconds', self.buffer_seconds))
        if seconds <= 0:
            raise CommandError('seconds must be > 0')
        want_devices = req.get('devices')
        if want_devices is not None:
            want_devices = [int(d) for d in want_devices]

        self._dump_id += 1
        dump_id = self._dump_id
        plan = []
        info = []
        for dev_idx, backend in enumerate(self._backends):
            if want_devices is not None and dev_idx not in want_devices:
                continue
            buffer_ = self._raw_buffers.get(dev_idx)
            if buffer_ is None:
                continue
            rate = self._rate(dev_idx)
            data = buffer_.get_recent(seconds, rate)
            nch = backend.num_channels
            if len(data) < nch:
                continue
            total_chunks = (len(data) + RAW_DUMP_CHUNK - 1) // RAW_DUMP_CHUNK
            samples_per_channel = len(data) // nch
            # Same grid as DATA for this device: the ring buffer holds the
            # most recent samples, so the dump's first sample is this many
            # samples behind the device's running total.
            dump_start = self._data_count.get(dev_idx, 0) - samples_per_channel
            plan.append((dev_idx, data, total_chunks, dump_start, nch))
            info.append({
                'device': dev_idx,
                'channels': self._chan_map.globals_for_device(dev_idx),
                'num_channels': nch,
                'sample_rate': rate,
                'samples_per_channel': samples_per_channel,
                'seconds': round(len(data) / nch / rate, 3),
                'total_chunks': total_chunks,
                'start_index': dump_start,
            })
        if not plan:
            raise CommandError('not enough data buffered')

        registry = self._registry

        def send_dump():
            for dev_idx, dump_data, total_chunks, dump_start, nch in plan:
                for i in range(total_chunks):
                    chunk = dump_data[i * RAW_DUMP_CHUNK:
                                      (i + 1) * RAW_DUMP_CHUNK]
                    chunk_start = dump_start + i * (RAW_DUMP_CHUNK // nch)
                    registry.send_stream_reliable(raw_dump_frame(
                        dump_id, dev_idx, i,
                        1 if i == total_chunks - 1 else 0, chunk_start,
                        chunk.tobytes()))

        threading.Thread(target=send_dump, daemon=True).start()
        return {'dump_id': dump_id, 'chunk_samples': RAW_DUMP_CHUNK,
                'units': self._units(), 'devices': info}

    def _cmd_get_metrics(self, req):
        """SLM metrics over the buffered raw data, per global channel."""
        if not self._raw_buffers:
            raise CommandError('no data buffered yet; start a scan first')
        try:
            import numpy as np
            import slm
        except ImportError as err:
            raise CommandError(
                'metrics need numpy + scipy on the Pi ({})'.format(err))

        seconds = float(req.get('seconds', self.buffer_seconds))
        weighting = req.get('weighting', self.freq_weighting)
        time_w = req.get('time_weighting', self.time_weighting)
        pct = req.get('percentiles', [10, 50, 90])
        want = req.get('channels', self.channels)

        # Fetch each device's window once, not once per channel: the window
        # can be hundreds of MB and concatenating it per channel would be
        # both slow and a memory spike.
        windows = {}
        for g in want:
            try:
                dev_idx, _local = self._chan_map.resolve(int(g))
            except ValueError:
                continue
            if dev_idx in windows:
                continue
            backend = self._backends[dev_idx]
            buffer_ = self._raw_buffers.get(dev_idx)
            if buffer_ is None:
                continue
            flat = buffer_.get_recent(seconds, self._rate(dev_idx))
            nch = backend.num_channels
            windows[dev_idx] = (flat.reshape(-1, nch)
                                if flat.size >= nch else None)

        results = {}
        for g in want:
            g = int(g)
            try:
                dev_idx, local = self._chan_map.resolve(g)
            except ValueError:
                continue
            backend = self._backends[dev_idx]
            data = windows.get(dev_idx)
            if data is None:
                continue
            ci = backend.channels.index(local)
            metrics = slm.window_metrics(
                data[:, ci], self._rate(dev_idx), weighting=weighting,
                time_weighting=time_w, ref=self._ref_for(g),
                percentiles=pct)
            metrics['units'] = self._units().get(str(g))
            metrics['calibrated'] = self.sensitivity.get(g, 1000.0) != 1000
            metrics['device'] = dev_idx
            if req.get('include_bands') and self.band_config.get('enabled'):
                metrics['bands'] = self._band_metrics(
                    data[:, ci], self._rate(dev_idx), g, weighting)
            results[str(g)] = metrics

        if not results:
            raise CommandError('not enough data buffered')
        return {'requested_seconds': seconds, 'channels': results}

    def _band_metrics(self, x, rate, g_chan, weighting):
        """Per-band Leq (with A/C offset) over a window, computed on demand."""
        import math
        import numpy as np
        import slm
        from band_filter import BandFilterBank
        cfg = self.band_config
        bank = BandFilterBank(
            rate, [g_chan],
            f_min=cfg.get('f_min', 20.0), f_max=cfg.get('f_max', 20000.0),
            fraction=cfg.get('fraction', 3), order=cfg.get('order', 3),
            margin=cfg.get('margin', 1.0))
        # Single-channel 2-D view -- no Python-list round trip.
        segments = {}
        for band_index, _c, samples in bank.process_2d(
                np.ascontiguousarray(x).reshape(-1, 1)):
            segments.setdefault(band_index, []).append(samples)
        ref2 = self._ref_for(g_chan) ** 2
        out = []
        for band in bank.bands:
            segs = segments.get(band['index'])
            if not segs:
                continue
            sig = np.concatenate(segs)
            leq = 10.0 * math.log10(max(np.mean(sig * sig), 1e-30) / ref2)
            leq += slm.weighting_offset_db(weighting, band['center'])
            out.append({'index': band['index'],
                        'center': round(band['center'], 2),
                        'Leq': round(leq, 2)})
        return out

    def _cmd_calibration_write(self, req):
        self._require_stopped()
        g = int(req['channel'])
        dev_idx, local = self._resolve(g)
        backend = self._backends[dev_idx]
        if not isinstance(backend, Mcc172Backend):
            raise CommandError('calibration_write is mcc172-only')
        slope = float(req['slope'])
        offset = float(req['offset'])
        backend.calibration_write(local, slope, offset)
        return {'channel': g, 'slope': slope, 'offset': offset}

    def _cmd_test_signals_write(self, req):
        self._require_stopped()
        mode = int(req['mode'])
        clock = int(req.get('clock', 0))
        sync = int(req.get('sync', 0))
        self._mcc().test_signals_write(mode, clock, sync)
        return {'mode': mode, 'clock': clock, 'sync': sync}

    _HANDLERS = {
        'ping': _cmd_ping,
        'get_config': _cmd_get_config,
        'status': _cmd_status,
        'info': _cmd_info,
        'get_sensitivity': _cmd_get_sensitivity,
        'get_iepe': _cmd_get_iepe,
        'get_clock': _cmd_get_clock,
        'calibration_read': _cmd_calibration_read,
        'blink_led': _cmd_blink_led,
        'start': _cmd_start,
        'stop': _cmd_stop,
        'set_sensitivity': _cmd_set_sensitivity,
        'set_iepe': _cmd_set_iepe,
        'set_sample_rate': _cmd_set_sample_rate,
        'set_channels': _cmd_set_channels,
        'set_trigger': _cmd_set_trigger,
        'set_options': _cmd_set_options,
        'set_bands': _cmd_set_bands,
        'set_weighting': _cmd_set_weighting,
        'set_level': _cmd_set_level,
        'set_storage': _cmd_set_storage,
        'set_dsp': _cmd_set_dsp,
        'set_resample': _cmd_set_resample,
        'calibrate': _cmd_calibrate,
        'save_config': _cmd_save_config,
        'get_metrics': _cmd_get_metrics,
        'get_raw': _cmd_get_raw,
        'calibration_write': _cmd_calibration_write,
        'test_signals_write': _cmd_test_signals_write,
        'set_output': _cmd_set_output,
        'output_start': _cmd_output_start,
        'output_stop': _cmd_output_stop,
        'output_status': _cmd_output_status,
    }


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------
def main():
    config_path = os.environ.get(
        'PISLM_CONFIG',
        os.path.join(os.path.dirname(os.path.abspath(__file__)), 'config.ini'))
    settings = load_config(config_path)

    backends, errors = open_backends(settings['devices'])
    for name, err in errors.items():
        print('[dev] {}: {}'.format(name, err), flush=True)
    if not backends:
        raise RuntimeError('no acquisition devices available: {}'.format(
            errors))
    for i, backend in enumerate(backends):
        print('[dev] {}: {} ({} ch)'.format(i, backend.name,
                                            backend.num_channels), flush=True)

    controller = Controller(backends, settings, config_path)
    registry = ClientRegistry(controller, settings['max_queue_blocks'])
    controller.attach_registry(registry)

    stop_event = threading.Event()

    def make_server(port):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((settings['host'], port))
        srv.listen(5)
        srv.settimeout(1.0)
        return srv

    def accept_loop(srv, kind):
        while not stop_event.is_set():
            try:
                conn, addr = srv.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            registry.add(conn, addr, kind)

    control_server = make_server(settings['control_port'])
    stream_server = make_server(settings['stream_port'])
    print('[net] control port {}:{}, stream port {}:{}'.format(
        settings['host'], settings['control_port'],
        settings['host'], settings['stream_port']), flush=True)

    def handle_signal(_signum, _frame):
        stop_event.set()
    try:
        signal.signal(signal.SIGINT, handle_signal)
        signal.signal(signal.SIGTERM, handle_signal)
    except ValueError:
        # Not running in the main thread (e.g. under a test harness); the
        # systemd service always runs in the main thread and gets signals.
        pass

    if settings['autostart']:
        try:
            controller.start()
            print('[scan] autostart: streaming', flush=True)
        except Exception as err:    # noqa: BLE001 - report and stay up
            print('[scan] autostart failed: {}'.format(err), flush=True)

    stream_thread = threading.Thread(
        target=accept_loop, args=(stream_server, 'stream'), daemon=True)
    stream_thread.start()
    try:
        accept_loop(control_server, 'control')
    finally:
        stop_event.set()
        try:
            if controller.running:
                controller.stop()
        except Exception:   # noqa: BLE001
            pass
        for srv in (control_server, stream_server):
            try:
                srv.close()
            except OSError:
                pass
        for backend in backends:
            try:
                backend.close()
            except Exception:   # noqa: BLE001
                pass
        controller._close_pool()
        if controller._gpio is not None:
            controller._gpio.close()


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError) as err:
        print('\nError: {}'.format(err), file=sys.stderr)
        sys.exit(1)
