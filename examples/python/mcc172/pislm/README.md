# PiSLM — Raspberry Pi Sound Level Meter

A headless setup that turns a Raspberry Pi with an **MCC 172** DAQ HAT and
(optionally) a **Data Translation DT9837A** USB module into a network
**sound level meter** you control from a laptop — up to **6 IEPE channels**.
On boot the Pi powers the IEPE microphones, applies your calibration, and
streams the **Fast time-weighted, A-weighted level** (the SLM "needle")
continuously. Raw samples are kept in ring buffers on the Pi, and on request
it computes **Leq, Lmax, Lmin, Lpeak, and LN percentiles** over a window —
like a class sound level meter. A separate control port drives every
configurable feature of both devices.

```
[IEPE mics] --2mA--> [MCC 172 HAT] ---SPI--> [Raspberry Pi]
                       (2 ch, global 0-1)          |  (systemd @ boot)
[IEPE mics] --4mA--> [DT9837A USB] ---USB--> ...   |  pislm.py
                       (4 ch, global 2-5)          |  + raw ring buffers
                                  control port 5000 <-> commands / metrics
                                  stream  port 5001  -> levels (+ raw/bands)
                                                Wi-Fi / USB-ethernet
                                                       v
                                                   [Laptop]
                                            (your own TCP client)
```

Both devices provide 24-bit simultaneous IEPE sampling: the MCC 172 at up
to 51.2 kHz/ch (2 ch), the DT9837A at up to 52.7 kHz/ch (4 ch). Channels
are numbered **globally** in device order (0–1 = MCC 172, 2–5 = DT9837A);
all commands and stream frames use global numbers.

> **Clock sync:** the two devices run separate ADC crystals (±50 ppm each),
> so they slip up to ~100 µs/s. The GPIO trigger aligns the scans' **start**;
> the software clock alignment below keeps them aligned afterwards. Without
> that, cross-device phase analysis is only valid for a moment after the
> trigger — per-channel levels and metrics are unaffected either way.

## Synchronized start (GPIO trigger)

With `[trigger] sync_start = true`, `start` arms both devices to begin on a
shared **rising edge** and the Pi then pulses a GPIO pin wired to both
trigger inputs — so both scans start on the same pulse (±1 sample per
device, plus each ADC's fixed group delay):

```
GPIO 17 (BCM) --+-- MCC 172 "TRIG" screw terminal (J5 pin 1)
                +-- DT9837A "Ext Trigger" input
GND -----------++-- MCC 172 "GND" (J5 pin 2) / DT9837A ground
```

- Rising edge only — the DT9837A's external digital trigger supports no
  other edge. 3.3 V GPIO levels satisfy both inputs (MCC 172 V_IH 1.48 V,
  DT9837A TTL).
- The pin must avoid those used by the MCC 172 HAT (0, 1, 5, 6, 8–13, 16,
  19, 20, 26); 17, 27, 22 are safe. Works with libgpiod v2/v1 or RPi.GPIO,
  whichever the OS provides.
- `source = external` instead arms the scans and waits for an edge you
  supply (e.g. a measurement-chain sync pulse); a `triggered` event is
  broadcast per device when its first samples arrive.
- Runtime control: `set_trigger {"enable": true, "gpio_pin": 27}` etc.;
  the `start` response reports per-device trigger status.

## Two ports

| Port | Default | Direction | Content |
|------|:-------:|-----------|---------|
| **Control** | 5000 | both ways | Newline-delimited JSON commands, responses (incl. on-demand metrics), and events. No binary. |
| **Stream**  | 5001 | Pi → client | Typed length-prefixed frames: JSON handshake, time-weighted **LEVEL** frames (default), optional per-band levels, optional raw waveform / band waveforms, plus events. |

The client is yours to write — see **[`PROTOCOL.md`](PROTOCOL.md)** for the
complete, language-agnostic wire specification (framing, handshake, the full
command list with request/response schemas, events, and examples).

## Files

| File | Runs on | Purpose |
|------|---------|---------|
| `pislm.py`     | Raspberry Pi | Multi-device acquisition; level streaming, raw ring buffers + on-demand metrics; control/stream servers. |
| `devices.py`           | Raspberry Pi | Device backends: MCC 172 (daqhats) and DT9837A (uldaq), plus the global channel map. |
| `slm.py`               | Raspberry Pi | Sound-level-meter DSP: IEC 61672 A/C/Z weighting, IEC 61672 Fast/Slow time weighting plus a legacy IEC 60651/60804 Impulse weighting, Leq/Lmax/Lmin/Lpeak/LN. |
| `band_filter.py`       | Raspberry Pi | Fractional-octave (1/3-octave) decimating Butterworth filter bank. |
| `dsp_pool.py`          | Raspberry Pi | Multi-core DSP: worker processes + shared memory for the level/band computation. |
| `gpio_trigger.py`      | Raspberry Pi | GPIO trigger-pulse output for the synchronized start (gpiod v2/v1 or RPi.GPIO); also reused by shutdown_button.py to drive the LED pin. |
| `clock_sync.py`        | Raspberry Pi | Cross-device clock alignment: true-rate tracking and arbitrary-ratio resampling to a common grid. |
| `excitation.py`        | Raspberry Pi | Analog-output excitation signal generation (sweep/MLS/noise) for reverberation measurement. |
| `shutdown_button.py`   | Raspberry Pi | Physical shutdown button (hold 3s) + optional UPS low-battery auto-shutdown; standalone service, independent of pislm.service. |
| `ina219.py`            | Raspberry Pi | INA219 battery monitor driver (Waveshare UPS HAT family and similar), used by shutdown_button.py. |
| `config.ini`           | Raspberry Pi | Boot defaults: devices, channels, IEPE, sensitivity, sample rate, weighting, level rate, buffer, bands, ports. |
| `pislm.service`| Raspberry Pi | systemd unit for automatic start at boot. |
| `pislm-shutdown-button.service` | Raspberry Pi | systemd unit for the shutdown button / UPS monitor (independent of pislm.service). |
| `pislm_test.py`        | Laptop       | Simple stdlib-only test client: interactive shell + live level meter. |
| `PROTOCOL.md`          | —            | Communication protocol specification for your client. |
| `INSTALL.md`           | —            | Full field installation manual (parts, wiring, OS, drivers, calibration, service). |

Python dependencies on the Pi. Install the scientific stack from `apt`
(Debian's builds are optimised for the platform), then put the device
bindings in a venv — Trixie enforces PEP 668, so system-wide `pip` is
refused:

```sh
sudo apt install python3-numpy python3-scipy python3-libgpiod python3-venv
python3 -m venv --system-site-packages ~/pislm-venv
~/pislm-venv/bin/pip install daqhats
```

For the DT9837A, additionally build/install the **uldaq** C library
(https://github.com/mccdaq/uldaq) and add its binding to the same venv:

```sh
sudo apt install gcc g++ make libusb-1.0-0-dev
# build & install the C library from the uldaq release tarball, then:
~/pislm-venv/bin/pip install uldaq
```

See [`INSTALL.md`](INSTALL.md) for the full sequence, including the udev
rule for non-root USB access and the systemd unit paths.

A device listed in `config.ini` but not attached is skipped at startup with
a log message — the monitor runs with whatever is present.

> **New build?** Follow **[`INSTALL.md`](INSTALL.md)** — the complete field
> installation manual (bill of materials, wiring, grounding, OS and driver
> setup, calibration with an acoustic calibrator, and commissioning). The
> quick steps below assume the hardware is already assembled.

## 1. Hardware

1. Power off the Pi, seat the MCC 172 on the 40-pin header (single board =
   address 0, all address jumpers removed).
2. Connect the DT9837A to a USB port (a powered hub is recommended on a
   Pi Zero 2 W — the DT9837A is USB-powered and drives 4 IEPE supplies).
3. Connect the IEPE microphones to the BNC inputs.
4. Power on the Pi.

## 2. Install the daqhats library on the Pi

Follow the top-level repository instructions:

```sh
cd ~
git clone https://github.com/mccdaq/daqhats.git
cd daqhats
sudo ./install.sh
```

Verify the board is detected:

```sh
daqhats_list_boards
```

## 3. Configure boot defaults

Edit `config.ini`. These are the initial settings the Pi boots with; a
client can change most of them live afterwards over the control port.

- **`[devices] enabled`** — which devices to open (`mcc172, dt9837a`), in
  order. Global channel numbers follow this order.
- **`[mcc172]` / `[dt9837a]`** — per-device local channels, IEPE on/off, and
  per-channel **sensitivity in mV/Pa**.
  - Leave at `1000` (the default = no scaling) and the data is in **volts**.
  - Set it to your mic's value (e.g. `50` for 50 mV/Pa) and the data is in
    **pascals**, so levels and metrics are true SPL (re 20 µPa).
- **`sample_rate`** (`[acquisition]`) — requested rate for every device;
  51200 gives the full audio bandwidth (~25 kHz).
- **`control_port` / `stream_port`** — the two TCP ports.
- **`autostart`** (`[control]`) — `true` starts streaming at boot; `false`
  boots idle and waits for a `start` command.

## 4. Run by hand (recommended before enabling the service)

On the Pi:

```sh
cd ~/daqhats/examples/python/mcc172/pislm
python3 pislm.py
```

On the laptop, `pislm_test.py` is a self-contained test client (stdlib
only — no install step) that gives you an interactive shell plus a live
per-channel level readout:

```sh
python3 pislm_test.py --host 192.168.50.1
```
```
> start
[ok] {...}
[level] ch0:  62.1 dB  ch1:  61.8 dB
> metrics 5
[ok] {"requested_seconds": 5.0, "channels": {...}}
> calibrate 0 94
[ok] {"applied": true, "new_sensitivity": 51.3, ...}
> save
> quit
```

Type `help` at its prompt for the full command list (`status`, `metrics`,
`calibrate`, `sens`, `iepe`, `rate`, `raw` to dump buffered audio to a file,
`bench` to measure streaming bandwidth, `output`/`outstart`/`outstop`/
`outstatus` to play an excitation signal through the DT9837A's analog
output (reverberation-time measurement), `send <json>` for anything not
covered by a shortcut). It is meant for quick checks during commissioning,
not as your production client — implement that against
[`PROTOCOL.md`](PROTOCOL.md), which documents both ports in full.

To check how much bandwidth the current configuration actually uses on
your network (useful before deciding between Wi-Fi and Ethernet, or before
enabling `stream_raw`/bands at a high sample rate):

```sh
> start
> bench 15
[bench] measuring for 15.0s ... control RTT ~2.1 ms
  total:   2312.4 KB/s  (18.94 Mbps), 1583.2 frames/s
  DATA         2312.4 KB/s   180.1 fps
  dsp dropped_blocks: +0   stream frames dropped: +0
```

See [`PROTOCOL.md` §8](PROTOCOL.md#8-bandwidth--performance-testing) for
expected bandwidth per mode and what a non-zero drop count means.

For a one-off check without even that:

```sh
printf '{"id":1,"cmd":"status"}\n' | nc 192.168.50.1 5000
```

## 5. Start automatically at boot (systemd)

Edit `pislm.service` if your username/paths differ from `pi`, then:

```sh
sudo cp pislm.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now pislm
```

Check status and logs:

```sh
systemctl status pislm
journalctl -u pislm -f
```

After editing `config.ini`, apply changes with:

```sh
sudo systemctl restart pislm
```

## Sound-level-meter behavior

What streams continuously, and what is computed on demand:

- **LEVEL frames (default on).** The Pi applies the configured frequency
  weighting (A/C/Z) and time weighting (Fast/Slow/Impulse) and streams the
  broadband level in dB at `[level] output_rate` (default 10/s) — the live
  needle. A few hundred bytes per second.
- **Raw ring buffer + `get_metrics` + `get_raw`.** Raw samples are kept in
  RAM only (`[storage] buffer_seconds`, default 300 s, packed 8 B/sample —
  **nothing is ever written to the SD card**). The `get_metrics` command
  computes **Leq, Lmax, Lmin, Lpeak, LN (L10/L50/L90...)** over a requested
  window — while running or after a stop — optionally with per-band Leq
  (`include_bands`). The `get_raw` command dumps the buffered raw waveform
  itself to the laptop (chunked `RAW_DUMP` frames, reliable delivery), so
  after an event you can pull the last N seconds for post-analysis.
- **1/3-octave spectrum (optional).** `[bands] enabled = true` adds per-band
  output. `output = level` (default) streams Fast time-weighted band levels
  in dB (`BAND_LEVEL` frames) — the octave-analyzer bar display, tiny
  bandwidth. `output = waveform` streams each band's decimated time signal
  (`BAND` frames) — heavy; the full 20 Hz–20 kHz set for 2 ch is ~36 Mbit/s,
  so lower `f_max` to fit Wi-Fi if you need waveforms.
- **Raw waveform streaming (optional).** `stream_raw = true` additionally
  streams the full-rate raw waveform (~6.6 Mbit/s for 2 ch) if you want to
  record everything on the laptop.

Frequency weighting, time weighting, level rate, buffer length, and band
setup are all changeable at runtime (`set_weighting`, `set_level`,
`set_storage`, `set_bands`) — stop the scan, set, start.

## Cross-device clock alignment

The MCC 172 and the DT9837A have independent crystals, so their streams slip
by up to ~100 µs per second — 36° of phase at 1 kHz after one second. The
GPIO trigger fixes the *start*; this fixes the rest.

**Rate tracking is always on and costs almost nothing.** Each device's true
sample rate is measured against the Pi's monotonic clock and reported per
device under `clock` (`get_config` / `status`), including its `ppm` offset.
The Pi's own clock error cancels in the *ratio* between two devices measured
against the same reference — verified: a 200 ppm reference bias leaves
0.05 ppm of ratio error. The estimate tightens with run time: ~6 ppm at 30 s,
2 ppm at 60 s, 0.15 ppm at 300 s.

Even with resampling off, those numbers let a client correct drift offline on
`get_raw` data.

**Resampling** puts every device on one common rate, using the *measured*
rates — that is what removes the drift. Resampling to the nominal rates
would only line both up on the same nominal grid and leave the slip.

```ini
[resample]
enabled = true
output_rate = 48000
```

or at runtime: `{"cmd": "set_resample", "enabled": true, "output_rate": 48000}`.

With it active, ring buffers, levels, bands, metrics and `get_raw` all run at
`output_rate`; the ADCs keep their own hardware rates (`actual_rate` in
`status` stays put, `effective_rate` shows the common grid). Note the MCC 172
can only sample at `51200/n`, so 48 kHz is unreachable in its hardware and is
produced in software here.

Quality at the defaults (32 taps, 4096 phases), 51.2 kHz → 48 kHz: −100 dB at
1 kHz, −93 dB at 5 kHz, −88 dB at 10 kHz — at or below the MCC 172's own
−93 dB THD. Cost: about 20% of one Pi 4 core for 6 channels.

## Calibration from the laptop

Calibration is a runtime command — no SSH, no editing files on the Pi, and
no arithmetic. Fit an acoustic calibrator, leave the scan running, and:

```json
{"cmd": "calibrate", "channel": 0, "level_db": 94}
```

The Pi measures the tone (1/3-octave bandpass around it, so background noise
does not bias the result), derives the sensitivity in mV/Pa that makes it
read 94 dB, and applies it. Add `"apply": false` to measure only — useful as
a drift check before a session, since `change_db` tells you how far the
channel has moved.

Applied values are runtime state; `{"cmd": "save_config"}` writes them into
`config.ini` so they survive a restart, preserving the file's comments and
tagging each line with the save date. Field sequence: `calibrate` per
channel, then one `save_config`.

You can still set a known sensitivity directly with `set_sensitivity` (scan
stopped), or put it in `config.ini` before boot.

## Multi-core DSP

The level/band computation is the only heavy work, and it parallelizes by
channel, so it runs in **worker processes** (`[dsp] workers`, default `-1`
= `cpu_count-1`, i.e. 3 on a Pi 4). Each worker owns the filter state for
its channels and receives blocks through **shared memory** — sample data is
never pickled. Threads are not an option: `scipy.signal.sosfilt` holds the
GIL, so a thread pool measures *slower* than serial.

Measured, 6 ch at 51.2 kHz with 1/3-octave bands to 20 kHz:

| Mode | One-core load (Pi 4) |
|------|---------------------:|
| inline (`workers = 0`) | ~91% — no headroom |
| 3 workers (default) | **~41%**, spread over 3 cores |

Set `workers = 0` to keep everything in the acquisition thread (simplest,
fine for levels-only use). The acquisition loop always keeps its own core:
it reads the devices, fills the RAM ring buffer, and broadcasts frames,
while the workers only compute.

## Notes & tips

- **Changing settings while streaming.** The MCC 172 rejects configuration
  changes during an active scan, so the server does too — send `stop`, make
  your change, then `start`.
- **Calibration → SPL.** With `set_sensitivity` in mV/Pa the samples are in
  pascals; `SPL = 20*log10(Prms/20e-6)` dB. The stream is unweighted
  (Z-weighting); apply A-weighting on the client for dB(A).
- **Storage-free operation.** With a wired-LAN Pi 4 the recommended workflow
  needs no SSD/SD data storage at all: record live on the laptop
  (`stream_raw` at ~0.82 MB/s per 2 ch is trivial for gigabit Ethernet), and
  use `get_raw` to backfill any gap or grab the pre-event window from the
  RAM buffer. RAM budget for the buffer: 2 ch ≈ 0.82 MB/s (300 s ≈ 246 MB),
  6 ch ≈ 2.46 MB/s (300 s ≈ 740 MB — fine on a 4 GB Pi 4).
- **Raspberry Pi Zero 2 W.** Streaming raw 2 ch × 51.2 kHz float64 is
  ~820 kB/s. That is fine over Wi-Fi/USB-ethernet, but if you see buffer
  overruns lower the sample rate or use one channel — and shorten
  `buffer_seconds` to fit its 512 MB RAM.
- **Backpressure.** A slow stream client never stalls acquisition: the
  server drops its oldest live frames (`[network] max_queue_blocks`).
  `get_raw` dump chunks are the exception — they are delivered reliably.
