# PiSLM — Installation Manual

Complete build guide for a **PiSLM** node: a Raspberry Pi 4 with an
MCC 172 DAQ HAT and a Data Translation DT9837A, giving 6 calibrated IEPE
channels streamed to a laptop as sound-level-meter data.

Work through the sections in order. Sections 1–4 are hardware, 5–9 are
software, 10–13 are configuration and commissioning, and §14 adds an
optional physical shutdown button.

**Already booted and just want the commands?** §17 is the whole
post-first-boot sequence in one block.

---

## 1. Bill of materials

### Required

| Item | Spec / note |
|------|-------------|
| Raspberry Pi 4 Model B | **4 GB** recommended (2 GB works with a shorter RAM buffer). The DSP needs the A72's single-core speed; a Zero 2 W / Pi 3 cannot run 6 ch with bands. |
| Official Pi 4 PSU | 5.1 V / 3 A USB-C. Do not use a phone charger — supply dips show up as noise. |
| microSD card | 32 GB, A1/A2 class. **OS only** — no measurement data is written to it. |
| MCC 172 DAQ HAT | 2 IEPE channels, 51.2 kS/s/ch. Address 0 (all address jumpers removed). |
| DT9837A | 4 IEPE channels, USB-powered. Optional if 2 channels are enough. |
| IEPE microphones | With a **calibrated sensitivity in mV/Pa** (from the datasheet or a calibration certificate). |
| Ethernet cable | Cat5e or better, Pi ↔ laptop (direct or via switch). |
| Heatsink or fan case | The MCC 172 spec limits operation to 0–55 °C. |

### Recommended

| Item | Why |
|------|-----|
| Powered USB hub | Keeps the DT9837A's 4 IEPE supplies off the Pi's 1.2 A USB budget. Required if you also attach USB storage. |
| Sound level calibrator | 94 dB / 114 dB @ 1 kHz — the only way to verify the calibration end to end (§12). |
| Jumper wires + 2-pin screw terminal | For the GPIO synchronized-start trigger (§4). |
| Acoustic calibrator adapter | Matches the calibrator cavity to your microphone diameter. |

---

## 2. Hardware assembly

**Power off the Pi and unplug it before touching the header.**

1. **Seat the MCC 172** on the Pi's 40-pin header, pressing evenly until it
   is fully home. With one HAT, leave **all address jumpers removed**
   (address 0) — one board must be at address 0 for the OS to read the HAT
   EEPROM.
2. **Fit the heatsink/fan** before mounting the HAT if your case requires it;
   the HAT covers the SoC.
3. **Connect the DT9837A** to a USB 3.0 port on the Pi (blue), or to a
   powered hub. Use the supplied USB cable — the DT9837A is bus-powered.
4. **Connect the microphones**:
   - MCC 172: 10-32 coaxial jacks (CH0, CH1), or the screw terminals — but
     **only one source per channel**, never both at once.
   - DT9837A: BNC inputs (CH0–CH3).
5. **Ethernet** from the Pi to the laptop or switch.

### Grounding (affects measurement quality)

The MCC 172 electrical specification states: *connect the signal source and
the Raspberry Pi to a common ground; if the source is floating, connect the
MCC 172 to earth ground via the DGND screw terminal to minimise common-mode
noise.*

- The DT9837A shares ground with the Pi through USB, so no extra bonding is
  needed between the two devices.
- Avoid ground loops: power the Pi from **one** supply, and prefer a single
  earth reference for the whole measurement chain.

---

## 3. Power

| Load | Draw |
|------|-----:|
| Raspberry Pi 4 (under load) | ~5.5 W |
| MCC 172 (incl. 2 IEPE supplies) | 0.7 W (140 mA @ 5 V max) |
| DT9837A (incl. 4 IEPE supplies) | up to 2.5 W (USB bus-powered) |
| **Total** | **~8.7 W** |

The official 5.1 V / 3 A (15.3 W) PSU covers this with margin. The Pi 4's
total USB budget is 1.2 A; the DT9837A's ≤500 mA fits, but add a **powered
hub** if you also attach USB storage.

**Battery operation**: any USB-PD bank that holds 5 V / 3 A works. At ~8.7 W
(≈7.5 W with §11 tuning), a 20 000 mAh (74 Wh) bank runs roughly 8 hours.

---

## 4. GPIO synchronized-start trigger (optional)

Wire this if you want both devices to begin their scans on the same edge.
Skip it for single-device use or if start alignment does not matter.

```
GPIO 17 (BCM, header pin 11) --+-- MCC 172 "TRIG"  (J5 pin 1)
                               +-- DT9837A "Ext Trigger" input
GND (header pin 9) ------------+-- MCC 172 "GND"   (J5 pin 2)
                               +-- DT9837A ground
```

- **Rising edge only** — the DT9837A's external digital trigger supports no
  other edge. 3.3 V GPIO satisfies both inputs (MCC 172 V_IH 1.48 V max).
- **Do not use** the pins the MCC 172 occupies: BCM 0, 1, 5, 6, 8–13, 16,
  19, 20, 26. Safe alternatives to 17: **27, 22**.
- Keep the trigger wire short and away from the microphone cables.

> This aligns the **start** of the scans (≈±1 sample per device plus each
> ADC's fixed group delay). It does **not** lock the ADC clocks — for that,
> enable the software clock alignment (`[resample]`, see §12 and the README),
> which measures each device's true rate and resamples both onto one grid.

---

## 5. Operating system

**Raspberry Pi OS (64-bit, Lite), Trixie.**

- **Raspberry Pi OS**, not Ubuntu/DietPi: the daqhats installer calls
  `raspi-config` directly to enable SPI and installs its dependencies with
  `apt`. Other distributions break that step.
- **64-bit**: faster numpy/scipy (this system is DSP-bound), and the RAM
  ring buffer reaches ~740 MB for 6 channels at 300 s.
- **Lite**: headless measurement node — no desktop background load. The
  CLI tools (`daqhats_list_boards` etc.) are all still there.
- **Trixie** ships **libgpiod v2** and **Python 3.13**. Both are supported:
  daqhats picks its GPIO backend from `pkg-config --modversion libgpiod`
  and builds `gpio_v2.c` for v2, and PiSLM's trigger tries libgpiod v2
  before v1 and RPi.GPIO. Trixie support in daqhats is recent, so if
  anything fails to build, Bookworm is the conservative fallback.

Steps:

1. Flash **Raspberry Pi OS (64-bit, Lite)** with Raspberry Pi Imager. In the
   Imager's settings (gear icon) pre-configure hostname, username, SSH and
   locale.

   - **Hostname**: `pislm` for a single node; number them (`pislm-01`,
     `pislm-02`, …) if you will ever run more than one — renaming later
     breaks `.local` addresses and your records. Lower-case letters,
     digits and hyphens only.
   - **Username**: Raspberry Pi OS has **no default `pi` account** any
     more, so you must choose one. Anything works; the rest of this manual
     derives the paths from `$USER` and `$HOME`, and §13 generates the
     systemd unit for whichever name you picked. Avoid `pi` itself — it is
     the first name anything scanning the network will try.
2. Boot the Pi, log in over SSH, and update:

   ```sh
   sudo apt update && sudo apt full-upgrade -y
   sudo reboot
   ```

3. Confirm SPI is not claimed by anything else. GPIO-header LCDs and other
   SPI HATs **will** break the MCC 172 — remove their overlays from
   `/boot/firmware/config.txt` if any were ever installed.

4. Check what you actually got — the rest of the manual assumes these:

   ```sh
   cat /etc/os-release | head -2      # expect trixie
   dpkg --print-architecture          # expect arm64
   pkg-config --modversion libgpiod   # expect 2.x on Trixie
   python3 --version                  # expect 3.13.x
   ```

---

## 6. Install the daqhats library (MCC 172)

Clone **the repository that contains PiSLM** — that is this fork, not the
upstream `mccdaq/daqhats`, which does not carry `examples/python/mcc172/pislm`.
Replace the URL with your own fork if it differs:

```sh
cd ~
git clone https://github.com/yohan2256/daqhats.git
cd daqhats
```

If PiSLM has not been merged to the default branch yet, check out its
branch first:

```sh
git checkout claude/raspberry-pi-noise-measurement-6vllbo
ls examples/python/mcc172/pislm     # should list pislm.py, config.ini, ...
```

Then build and install the C library and tools:

```sh
sudo ./install.sh
```

The installer reads the HAT EEPROM and may prompt for a reboot. Verify:

```sh
daqhats_list_boards
```

You should see the MCC 172 at address 0. If not, re-seat the HAT and check
that no other SPI device is configured.

---

## 7. Install uldaq (DT9837A)

Skip this section if you are not using the DT9837A.

```sh
sudo apt install -y gcc g++ make libusb-1.0-0-dev

# C library (check the project page for the current release tag)
cd ~
wget https://github.com/mccdaq/uldaq/releases/download/v1.2.1/libuldaq-1.2.1.tar.bz2
tar -xvjf libuldaq-1.2.1.tar.bz2
cd libuldaq-1.2.1
./configure && make
sudo make install
sudo ldconfig
```

> **Trixie note.** This is the one build in the whole install that is not
> maintained against Trixie, and it uses the distribution's compiler
> (Trixie's GCC is much newer than what the release was written for). If
> `make` stops on an error, the usual fix is to relax the new default
> diagnostics:
>
> ```sh
> make CFLAGS="-w -std=gnu11" CXXFLAGS="-w -std=gnu++14"
> ```
>
> If it still will not build, that is the point to fall back to Bookworm —
> everything else here works on either release.

The Python binding is installed into the virtual environment in §8.

Allow non-root USB access:

```sh
sudo tee /etc/udev/rules.d/99-dt9837a.rules >/dev/null <<'EOF'
# Data Translation DT9837A — allow access for the plugdev group
SUBSYSTEM=="usb", ATTRS{idVendor}=="0a2d", MODE="0666", GROUP="plugdev"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG plugdev "$USER"
```

The service runs as your login user, so that user needs USB access — that
is what the `plugdev` group above is for. Log out and back in, then verify
the device enumerates:

```sh
lsusb | grep -i "data translation"
```

---

## 8. Python environment

Install the heavy scientific packages from `apt` — Debian's builds are
optimised for the platform, and letting `pip` compile numpy/scipy on a Pi is
both slow and slower at runtime:

```sh
sudo apt install -y python3-numpy python3-scipy python3-libgpiod python3-venv
```

- `numpy` / `scipy` — levels, metrics, band filtering, resampling.
- `python3-libgpiod` — on Trixie this is **libgpiod v2**; PiSLM's trigger
  detects v2, v1, or `RPi.GPIO` automatically.

Trixie enforces **PEP 668**, so system-wide `pip install` is refused. Create
a virtual environment that can still see the apt packages, and install the
device bindings into it:

```sh
python3 -m venv --system-site-packages ~/pislm-venv
~/pislm-venv/bin/pip install daqhats
~/pislm-venv/bin/pip install uldaq        # only if using the DT9837A
```

`--system-site-packages` is what makes the apt numpy/scipy visible inside
the venv — without it, pip would try to build them from source.

Verify everything imports together:

```sh
~/pislm-venv/bin/python -c "import numpy, scipy, gpiod, daqhats, uldaq; print('deps ok')"
```

(Drop `uldaq` from that line if you are not using the DT9837A.)

> Prefer not to use a venv? `sudo pip install daqhats --break-system-packages`
> works too, but the venv keeps the measurement system independent of OS
> package updates — worth it on an instrument.

---

## 9. Install PiSLM

PiSLM is already in the checkout from §6 (that is why §6 clones this fork
rather than upstream):

```sh
cd ~/daqhats/examples/python/mcc172/pislm
ls          # pislm.py, config.ini, devices.py, ...
```

Nothing to build — it is plain Python.

---

## 10. Network setup

This sets a static IP on the Pi's **built-in wired Ethernet port** (`eth0`),
for a direct Pi ↔ laptop cable — not Wi-Fi. Gigabit Ethernet carries the
full raw stream (6 ch ≈ 20 Mbit/s) with room to spare, and keeps RF away
from the microphone lines.

> **If you are on SSH over this same wired port** (e.g. through a switch,
> rather than sitting at the Pi with a keyboard), changing its IP mid-session
> can drop your connection before the command finishes, and you may not be
> able to reconnect at the old address. Either run this from the Pi's local
> console, or be ready to reconnect at the new address (`192.168.50.1`)
> immediately after. SSH over Wi-Fi (if enabled) is unaffected either way,
> since only the wired interface is being reconfigured.

```sh
sudo nmcli con mod "Wired connection 1" \
    ipv4.method manual ipv4.addresses 192.168.50.1/24
sudo nmcli con up "Wired connection 1"
```

Set the laptop's wired interface to `192.168.50.2/24`. Verify from the
laptop:

```sh
ping 192.168.50.1
```

---

## 11. Low-power / low-noise tuning (recommended)

On a wired, headless measurement node, disable the radios and video:

```sh
sudo tee -a /boot/firmware/config.txt >/dev/null <<'EOF'

# --- PiSLM: headless measurement node ---
dtoverlay=disable-wifi
dtoverlay=disable-bt
EOF
sudo systemctl disable --now bluetooth
sudo reboot
```

Saves roughly 0.5–1 W and removes the on-board radios as a noise source.
Keep Wi-Fi enabled if a tablet will connect wirelessly for live readout.

---

## 12. Configure and calibrate

Edit `config.ini`. The settings you **must** get right:

```ini
[devices]
enabled = mcc172, dt9837a      ; omit dt9837a for a 2-channel node

[mcc172]
channels = 0, 1
iepe_enable = true
sensitivity_ch0 = 50           ; ← YOUR microphone, in mV/Pa
sensitivity_ch1 = 50

[dt9837a]
channels = 0, 1, 2, 3
iepe_enable = true
sensitivity_ch0 = 50
; ... ch1..ch3
```

**Sensitivity is the calibration.** Enter each microphone's mV/Pa value and
the data comes back in **pascals**, so all levels and metrics are true SPL
re 20 µPa. Left at the default `1000`, the data stays in volts.

Other settings worth reviewing (see the comments in the file): `sample_rate`,
`[weighting] frequency/time`, `[level] output_rate`, `[storage]
buffer_seconds`, `[bands]`, `[dsp] workers`, `[trigger] sync_start`.

### Calibrate with an acoustic calibrator

You do **not** have to know the sensitivity in advance, and you do not have
to do the arithmetic — `calibrate` derives it from the calibrator tone. This
is also how you calibrate later, in the field, without touching the Pi.

1. Start PiSLM (§13) and let the scan run for a few seconds.
2. Fit the calibrator to the microphone on channel 0 and switch it on
   (94 dB @ 1 kHz).
3. From the laptop, using [`pislm_test.py`](pislm_test.py) (§13):

   ```
   > calibrate 0 94
   ```

   or the same thing as a raw command, with any client:

   ```sh
   printf '{"id":1,"cmd":"calibrate","channel":0,"level_db":94}\n' \
       | nc 192.168.50.1 5000
   ```

   The response reports `measured_level_db` (what the old calibration
   thought the tone was), `new_sensitivity` in mV/Pa, and `change_db`. The
   new value is applied immediately — the scan is briefly stopped and
   restarted, which is normal.

4. Move the calibrator to the next microphone, wait a couple of seconds for
   the buffers to refill, and repeat with that channel number.
5. **Persist it** — otherwise the values are lost on the next restart:

   ```
   > save
   ```
   ```sh
   printf '{"id":2,"cmd":"save_config"}\n' | nc 192.168.50.1 5000
   ```

   This rewrites the `sensitivity_chN` values in `config.ini`, keeping the
   file's comments and adding a `; saved <date>` marker per line.

**Checking without changing anything** (drift check before a session):

```sh
printf '{"id":3,"cmd":"calibrate","channel":0,"level_db":94,"apply":false}\n' \
    | nc 192.168.50.1 5000
```

`change_db` is how far the channel has drifted. A well-behaved chain should
be within a few tenths of a dB; note the value in your measurement record.

**Verify** afterwards with the calibrator still fitted:

```sh
printf '{"id":4,"cmd":"get_metrics","seconds":5,"channels":[0]}\n' \
    | nc 192.168.50.1 5000
```

`Leq` should read **94 dB ±0.5** (A-weighting is ≈0 dB at 1 kHz).

> If you already know the sensitivity from the microphone's certificate,
> just put it in `config.ini` (§12) or send
> `{"cmd":"set_sensitivity","channel":0,"value":50}` with the scan stopped —
> `calibrate` is for deriving it from a calibrator instead.

---

## 13. First run and automatic start

**Step 1 — run by hand** (always do this before enabling the service):

```sh
cd ~/daqhats/examples/python/mcc172/pislm
~/pislm-venv/bin/python pislm.py
```

Expected output: each device detected, the DSP worker count, and the two
listening ports. From the laptop, copy `pislm_test.py` over (it needs
nothing but Python 3 — no install) and run it for an interactive shell plus
a live level meter:

```sh
python3 pislm_test.py --host 192.168.50.1
```

Or, for a one-line check without even that:

```sh
printf '{"id":1,"cmd":"status"}\n' | nc 192.168.50.1 5000
```

Stop the Pi side with Ctrl-C.

**Step 2 — install the service:**

The shipped unit uses `pi` as a placeholder, but Raspberry Pi OS has no
default `pi` account any more. Generate the unit for whoever you actually
created, straight from the current login:

```sh
cd ~/daqhats/examples/python/mcc172/pislm
sed -e "s|User=pi|User=$USER|" \
    -e "s|/home/pi|$HOME|g" \
    pislm.service | sudo tee /etc/systemd/system/pislm.service >/dev/null

# Check it points at your user, your home, and the venv interpreter:
grep -E "^(User|WorkingDirectory|ExecStart|Environment)=" \
    /etc/systemd/system/pislm.service

sudo systemctl daemon-reload
sudo systemctl enable --now pislm
```

**Step 3 — check it:**

```sh
systemctl status pislm
journalctl -u pislm -f
```

After editing `config.ini`: `sudo systemctl restart pislm`.

---

## 14. Physical shutdown button (optional)

A momentary switch that powers off the Pi cleanly when held 3 seconds,
with an LED that's unambiguous at a glance: **steady on** while the
service is up and running normally, **blinking** while it's shutting
down, **dark** once it's safe to remove power. It works even if
`pislm.service` has crashed, since it runs as its own independent
service.

```
GPIO 27 (BCM, header pin 13) --+-- switch --+
GND (header pin 9 or 14) --------------------+

GPIO 24 (BCM, header pin 18) --+-- resistor (~330R) --+-- LED --+
GND (header pin 14 or 20) -------------------------------------+
```

- Internal pull-up is used for the button, so no external resistor is
  needed there — just a switch between GPIO 27 and any GND pin.
- The LED is optional: wire it as above (anode toward the GPIO/resistor
  side, cathode toward GND) for a clearly visible external indicator. If
  you skip it, or GPIO 24 is unavailable, this automatically falls back to
  the Pi's own onboard status LED (ACT) instead — note that repurposes it
  away from its normal disk-activity blinking (it goes steady on instead).
  Either way, all three states work with zero required wiring beyond the
  button itself.
- **Do not use** a pin the MCC 172 occupies (BCM 0, 1, 5, 6, 8–13, 16, 19,
  20, 26) or the sync-start trigger pin if §4 is in use (default BCM 17).
  Override the pins with `PISLM_SHUTDOWN_GPIO_PIN` (button) /
  `PISLM_SHUTDOWN_LED_GPIO_PIN` (LED) in the service file below if 27/24
  are unavailable on your wiring.
- Holding for less than 3 s does nothing and resets the timer on release —
  only a continuous 3-second hold starts the blink and shuts down.
- **The LED going dark means the process was killed partway through the
  OS halt, not necessarily "fully powered off."** On a plain USB-powered
  Pi with no smart power controller, the board stays electrically live
  until you physically remove power either way — treat "stopped blinking"
  as *safe to remove power*, and wait a couple of seconds past that before
  actually unplugging it.
- The service deliberately ignores `SIGTERM` once a shutdown starts:
  `systemctl poweroff` stops every other service first, including this
  one, and without that the blink would get cut off almost immediately by
  systemd's own stop signal rather than surviving into the actual halt.

Install the service:

```sh
cd ~/daqhats/examples/python/mcc172/pislm
sed -e "s|/home/pi|$HOME|g" pislm-shutdown-button.service \
    | sudo tee /etc/systemd/system/pislm-shutdown-button.service >/dev/null

# Check it points at your home and the venv interpreter:
grep -E "^(WorkingDirectory|ExecStart)=" \
    /etc/systemd/system/pislm-shutdown-button.service

sudo systemctl daemon-reload
sudo systemctl enable --now pislm-shutdown-button
```

It runs as root (needed for `systemctl poweroff`, the GPIO LED pin, and
the fallback status LED's sysfs files), independently of `pislm.service` —
deliberately so the button still works to power the Pi off even if the
acquisition service is down.

To power back on, unplug/replug power (or use a smart plug / PoE switch
with remote power control) — there is no soft power-on without extra
hardware, only a clean soft power-off.

### 14.1 UPS battery monitoring (optional)

If you have an INA219-based UPS HAT (e.g. Waveshare's UPS HAT family),
`pislm-shutdown-button.service` can also watch its battery over I2C and
trigger the same blink+poweroff sequence on sustained low charge — not
just the button.

I2C is a shared bus: if something else (an RTC, another sensor) is
already on SDA1/SCL1 (BCM 2/3, header pins 3/5), just wire the UPS's
SDA/SCL to the same two pins in parallel — multiple devices coexist on
one I2C bus as long as their addresses differ. Confirm with:

```sh
sudo raspi-config nonint do_i2c 0     # enable I2C if not already on
i2cdetect -y 1
```

Look for `0x41` (the common default for Waveshare's INA219-based boards;
some models/wiring use a different address — whatever responds and isn't
`UU`-claimed by another driver is your UPS). Install the driver dependency
and enable monitoring:

```sh
~/pislm-venv/bin/pip install smbus2
sudo systemctl edit pislm-shutdown-button --full
# uncomment/adjust the PISLM_UPS_* Environment= lines (§14's service file
# has all of them, commented, with their defaults)
sudo systemctl daemon-reload
sudo systemctl restart pislm-shutdown-button
journalctl -u pislm-shutdown-button -f   # confirm "UPS monitor on I2C bus ..."
```

Defaults: shuts down once the battery reads ≤10% continuously for 30s
(a sustained-low requirement, like the button's hold, so one noisy
reading can't trigger it), polled every 10s.

**Set `PISLM_UPS_V_MIN`/`PISLM_UPS_V_MAX` to match your actual pack --
this is not optional.** The percentage is linear between these two bus
voltages (0% at `V_MIN`, 100% at `V_MAX`); getting them wrong doesn't
error, it just silently clamps to a meaningless 0% or 100% forever,
which **quietly disables the low-battery shutdown** (it can never cross
a threshold it's already pinned past). The default, 6.0V/8.4V, is
Waveshare's own assumption for a 2S Li-ion pack (2 cells in series, 2 *
3.0V empty .. 2 * 4.2V full) -- confirmed in the field that this does
**not** universally hold across boards/packs, e.g. a 3S pack (3 cells in
series) is 9.0V/12.6V. Multiple cells wired in *parallel* don't change
this (parallel raises capacity, not voltage) -- only series count does.
Check what you actually have with `cat /run/pislm-ups-status.json` right
after a fresh full charge and again near what you believe is empty; if
`percent` reads 100 or 0 immediately and stays pinned there while the
`bus_voltage_v` is clearly still changing, `V_MIN`/`V_MAX` don't match
your pack.

This is also only a linear approximation of state of charge, not a true
one -- real Li-ion discharge curves are S-shaped (fast drop near full,
a flatter plateau through the middle, fast drop near empty), so treat
the percentage as a rough estimate and leave real margin on
`PISLM_UPS_LOW_PERCENT` (e.g. 20-25%) rather than cutting it close to 0.

Every poll is also written to `/run/pislm-ups-status.json` (tmpfs,
ephemeral) purely as a live status, not a log — `pislm.py` reads this
(best-effort, `[ups] status_file` in `config.ini`) and surfaces it in the
handshake/`get_config`'s `ups` field, so you can check battery status
through the same client you already use for everything else, without a
second connection or touching I2C directly:

```
> get_config
...
"ups": {"available": true, "stale": false, "age_seconds": 3.2,
        "percent": 76.0, "bus_voltage_v": 7.82, "current_ma": -215.0,
        "power_w": 1.68, "low_battery_hold_seconds": 0.0}
```

`pislm.py` never touches the I2C bus itself — only the independent
shutdown-button service does, so the safety-critical low-battery shutdown
still works even if `pislm.service` has crashed. If that service isn't
running (or the UPS isn't wired), `available` is `false` — check just
means the value is missing, never a crash.

---

## 15. Field checklist

Before each measurement session:

- [ ] Microphones connected; IEPE enabled for every channel in use
- [ ] `sensitivity_chN` matches the microphone actually fitted
- [ ] Calibrator check passed (§12) — note the deviation
- [ ] Windscreens fitted outdoors
- [ ] `systemctl status pislm` active; laptop can reach both ports
- [ ] `buffer_seconds` long enough to cover your longest event
- [ ] Laptop has disk space if recording raw (6 ch ≈ 8.8 GB/hour)
- [ ] For impact/shock measurement (hammer, tapping machine, floor impact):
      leave headroom below full-scale. A calibrator check only validates
      steady-state sensitivity, not peak margin -- a real impact can be
      20-40 dB above its own RMS. Check `overload` in `status`/handshake
      after a trial impact; if it's nonzero, back off sensitivity or check
      the sensor is actually rated for that shock level (see Troubleshooting).

**Cross-device phase.** The two ADC clocks are independent (±50 ppm each).
Per-channel levels and metrics are unaffected either way, but for phase or
correlation *between* devices you need both the GPIO trigger (§4, aligns the
start) **and** `[resample] enabled = true` (aligns the rates). Check
`clock` in `status`: each device's `settled` should be true and the run
should be at least a minute old before you trust cross-device phase — the
rate estimate reaches ~2 ppm at 60 s and ~0.15 ppm at 300 s. Without
resampling, keep phase-coherent channel pairs on the same device.

---

## 16. Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `daqhats_list_boards` finds nothing | HAT not fully seated, or another SPI device configured. Check `/boot/firmware/config.txt` for display/SPI overlays. |
| `No DT9837A device found` | udev rule not applied, or user not in `plugdev`. Re-log in; check `lsusb`. |
| `dt9837a: libuldaq.so: cannot open shared object file` | The `uldaq` C library was never built/installed (the `pip install uldaq` wrapper needs it separately). Redo §7's `./configure && make && sudo make install && sudo ldconfig`, then confirm with `ldconfig -p \| grep uldaq`. |
| `dt9837a: list index out of range` | Fixed in this repo — update to the latest `pislm` (`git pull`) and restart the service; the DT9837A only supports single-ended inputs. |
| `overrun` events, scan stops | The Pi cannot keep up. Lower `sample_rate`, lower `[bands] f_max`, turn off `[resample]`, or confirm `[dsp] workers` is `-1` (not `0`). |
| Periodic spikes in BAND_LEVEL, raw recording looks fine | A DSP worker fell behind and dropped a block (`dsp.dropped_blocks` rising in `get_config`, §8 of PROTOCOL.md). The gap is now correctly reflected in `start_index` and the affected filters reset cleanly across it, so a rising `dropped_blocks` means the Pi's DSP genuinely can't keep up at the current config -- reduce `[bands]` scope (narrower `f_min`/`f_max`, lower `fraction`), lower `sample_rate`, or add workers/a faster Pi. |
| Cross-device phase drifts over time | Enable `[resample]`; wait for `clock.settled` on both devices (~60 s). |
| `clock.ppm` reads hundreds of ppm | Not a crystal error — usually a stalled or restarted scan. Restart and re-check; values beyond ±500 ppm are rejected as implausible. |
| Levels ~0 dB or nonsense | IEPE off, or `sensitivity` left at 1000 (data in volts, not Pa). |
| Level is off by a fixed amount | Recalibrate with the calibrator (§12). |
| Level "hooks up then slowly decays" after an impact/shock, longer than the configured time weighting should allow (Fast should settle in <1 s) | Check for an `overload` event at the same instant (`status`, or the `overload` handshake/event field) — this is almost always ADC/sensor clipping, not a DSP bug: a clipped IEPE input can take much longer to recover than its normal small-signal time constant. Reduce sensitivity/gain for headroom, or use a sensor actually rated for that shock level. A DSP filter cannot recover data lost to clipping after the fact. |
| Hum / mains buzz | Ground loop. Use one PSU, bond DGND to earth for floating sources, keep cables away from mains. |
| Broadband/RF-ish noise on MCC 172, clean on DT9837A | The Wi-Fi/BT antenna sits right under the HAT. Disable the radios (§11) — `rfkill block` alone does not survive a reboot, use the `dtoverlay` in §11. |
| Shutdown button does nothing | `systemctl status pislm-shutdown-button`; check the pin isn't shared with the sync-start trigger (§4/§14) and that `python3-libgpiod` is installed (§8). |
| `trigger GPIO unavailable` | Missing `python3-libgpiod`, or the pin collides with the MCC 172 (§4). On Trixie this package is libgpiod v2, which PiSLM detects automatically. |
| `error: externally-managed-environment` from pip | Trixie enforces PEP 668. Install into the venv (§8), or append `--break-system-packages`. |
| `ModuleNotFoundError: daqhats` / `uldaq` under systemd but not by hand | The unit is running the system python. Point `ExecStart` at `~/pislm-venv/bin/python` (§13). |
| uldaq `make` fails on Trixie | Newer GCC diagnostics; retry with `make CFLAGS="-w -std=gnu11" CXXFLAGS="-w -std=gnu++14"` (§7). |
| Service dies at boot, works by hand | Wrong `User=`/paths in the unit, or it started before the HAT was ready — `Restart=always` retries; check `journalctl -u pislm`. |
| Client cannot connect | Check the Pi's IP and that `config.ini` binds `host = 0.0.0.0`. |

---

## 17. Quick reference — everything after the first boot

The full sequence, condensed. Each block links back to the section that
explains it. Run them in order on a freshly booted Pi.

```sh
# --- 5. update, then check what you got --------------------------------
sudo apt update && sudo apt full-upgrade -y && sudo reboot
# (log back in)
cat /etc/os-release | head -2; dpkg --print-architecture
pkg-config --modversion libgpiod; python3 --version

# --- 6. daqhats (clone THIS fork -- upstream has no pislm/) ------------
cd ~
git clone https://github.com/yohan2256/daqhats.git
cd daqhats
git checkout claude/raspberry-pi-noise-measurement-6vllbo   # until merged
sudo ./install.sh
daqhats_list_boards                    # expect MCC 172 at address 0

# --- 7. uldaq, only if using the DT9837A -------------------------------
sudo apt install -y gcc g++ make libusb-1.0-0-dev
cd ~ && wget https://github.com/mccdaq/uldaq/releases/download/v1.2.1/libuldaq-1.2.1.tar.bz2
tar -xvjf libuldaq-1.2.1.tar.bz2 && cd libuldaq-1.2.1
./configure && make && sudo make install && sudo ldconfig
sudo tee /etc/udev/rules.d/99-dt9837a.rules >/dev/null <<'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="0a2d", MODE="0666", GROUP="plugdev"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG plugdev "$USER"       # log out and back in after this

# --- 8. Python environment ---------------------------------------------
sudo apt install -y python3-numpy python3-scipy python3-libgpiod python3-venv
python3 -m venv --system-site-packages ~/pislm-venv
~/pislm-venv/bin/pip install daqhats
~/pislm-venv/bin/pip install uldaq     # DT9837A only
~/pislm-venv/bin/python -c "import numpy, scipy, gpiod, daqhats; print('deps ok')"

# --- 10. static IP for the direct laptop link --------------------------
sudo nmcli con mod "Wired connection 1" \
    ipv4.method manual ipv4.addresses 192.168.50.1/24
sudo nmcli con up "Wired connection 1"

# --- 11. low-power / low-noise (optional, recommended) -----------------
sudo tee -a /boot/firmware/config.txt >/dev/null <<'EOF'

# --- PiSLM: headless measurement node ---
dtoverlay=disable-wifi
dtoverlay=disable-bt
EOF
sudo systemctl disable --now bluetooth && sudo reboot

# --- 12. configure, then 13. run by hand once --------------------------
cd ~/daqhats/examples/python/mcc172/pislm
nano config.ini                        # sensitivities, devices, ports
~/pislm-venv/bin/python pislm.py       # Ctrl-C to stop

# --- 13. install the service (rewrites user/paths for you) -------------
sed -e "s|User=pi|User=$USER|" -e "s|/home/pi|$HOME|g" pislm.service \
    | sudo tee /etc/systemd/system/pislm.service >/dev/null
grep -E "^(User|WorkingDirectory|ExecStart|Environment)=" \
    /etc/systemd/system/pislm.service
sudo systemctl daemon-reload && sudo systemctl enable --now pislm
systemctl status pislm

# --- 14. physical shutdown button (optional) ----------------------------
sed -e "s|/home/pi|$HOME|g" pislm-shutdown-button.service \
    | sudo tee /etc/systemd/system/pislm-shutdown-button.service >/dev/null
sudo systemctl daemon-reload && sudo systemctl enable --now pislm-shutdown-button
```

Then calibrate (§12) from the laptop and you are measuring.

---

## 18. Next steps

- **Write your client** against [`PROTOCOL.md`](PROTOCOL.md) — the complete
  wire specification (both ports, frame types, every command).
- **Operating notes and tuning** are in [`README.md`](README.md).
