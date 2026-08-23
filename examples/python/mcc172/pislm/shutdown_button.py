#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
Physical shutdown button for the PiSLM.

One Raspberry Pi GPIO pin, pulled up internally and grounded by a momentary
switch, is polled for a continuous 3-second hold:

    GPIO <pin> ----+---- switch ----+
                    (internal pull-up)
    GND       -----------------------+

No external pull-up/pull-down resistor is needed for the button itself --
the GPIO line uses the SoC's internal pull-up. For the "shutting down"
indicator, a dedicated LED wired to a second GPIO pin is tried first:

    GPIO <led_pin> ----+---- resistor ----+---- LED (anode) ---- LED (cathode)
    GND       -----------------------------------------------------+

Three LED states, meant to be unambiguous at a glance: **steady on** while
this service is up and running normally, **blinking** while a shutdown
(button hold or low battery) is in progress, and **dark** once the
process is killed partway through the OS halt -- treat "stopped
blinking" as safe to remove power, not necessarily "fully powered off"
the instant it goes dark. If that pin can't be opened (not wired, or in
use for something else), this falls back to the Raspberry Pi's own
onboard status LED (ACT / led0) over sysfs, so all three states still
work with zero extra wiring either way (note this does repurpose the
onboard LED away from its normal disk-activity blinking).

This is intentionally a separate, standalone service from pislm.service
(see pislm-shutdown-button.service) so the button still works to cleanly
power off the Pi even if the acquisition service has crashed or is not
running.

Also (optionally) watches a UPS's INA219 battery monitor over I2C -- see
ina219.py -- and triggers the same shutdown+blink sequence if the battery
stays below a threshold for a sustained period, not just on the button.
Every poll is written to a small status file (PISLM_UPS_STATUS_FILE) so it
can be checked without touching I2C directly -- pislm.py reads this file
(best-effort) to surface battery status in its own status/get_config.

Same three-backend GPIO strategy as gpio_trigger.py, so this works across
OS generations (including Pi 5 / Trixie where the legacy interfaces are
gone):

    1. libgpiod v2 Python bindings (python3-libgpiod >= 2.x)
    2. libgpiod v1 Python bindings
    3. RPi.GPIO (legacy)
"""
from __future__ import print_function

import glob
import json
import os
import signal
import subprocess
import sys
import threading
import time

from gpio_trigger import GpioTrigger    # reused as a generic single-
                                        # output-pin driver for the LED

#: BCM pins used by the MCC 172 HAT -- never use these for the button either
#: (kept in sync with gpio_trigger.MCC172_RESERVED_PINS).
MCC172_RESERVED_PINS = frozenset(
    (0, 1, 5, 6, 8, 9, 10, 11, 12, 13, 16, 19, 20, 26))

#: BCM pin the button is wired to. Must not collide with the sync-start
#: trigger pin (config.ini [trigger] gpio_pin, default 17) if that feature
#: is also in use. Override with the PISLM_SHUTDOWN_GPIO_PIN env var.
BUTTON_PIN = int(os.environ.get('PISLM_SHUTDOWN_GPIO_PIN', 27))

#: BCM pin for a dedicated shutdown-indicator LED (optional -- falls back
#: to the onboard status LED if this pin can't be opened). Must not
#: collide with BUTTON_PIN or the trigger pin above. Override with the
#: PISLM_SHUTDOWN_LED_GPIO_PIN env var.
LED_PIN = int(os.environ.get('PISLM_SHUTDOWN_LED_GPIO_PIN', 24))

#: How long the button must be held continuously to trigger a shutdown.
#: Override with the PISLM_SHUTDOWN_HOLD_SECONDS env var.
HOLD_SECONDS = float(os.environ.get('PISLM_SHUTDOWN_HOLD_SECONDS', 3.0))

#: UPS battery monitoring (optional -- an INA219-based UPS HAT, e.g. the
#: Waveshare UPS HAT family). Disabled automatically if no INA219 answers
#: at the configured bus/address; never blocks the button itself.
UPS_ENABLED = os.environ.get('PISLM_UPS_ENABLED', '1') not in ('0', 'false', '')
UPS_I2C_BUS = int(os.environ.get('PISLM_UPS_I2C_BUS', 1))
UPS_I2C_ADDRESS = int(os.environ.get('PISLM_UPS_I2C_ADDRESS', '0x41'), 0)
#: Bus voltage at 0% / 100% battery -- MUST match your actual pack's
#: series cell count (default is Waveshare's own 2S Li-ion assumption,
#: 2*3.0V .. 2*4.2V; confirmed in the field that this does not universally
#: hold -- a 3S pack is 9.0V/12.6V). Getting this wrong doesn't error, it
#: just silently clamps to a meaningless 0%/100% -- see ina219.py and
#: INSTALL.md 14.1.
UPS_V_MIN = float(os.environ.get('PISLM_UPS_V_MIN', 6.0))
UPS_V_MAX = float(os.environ.get('PISLM_UPS_V_MAX', 8.4))
#: Shut down once the battery has read at or below this percentage for
#: PISLM_UPS_LOW_HOLD_SECONDS continuously (a sustained-low requirement,
#: same idea as the button's hold -- one noisy/transient low reading must
#: not trigger a shutdown).
UPS_LOW_PERCENT = float(os.environ.get('PISLM_UPS_LOW_PERCENT', 10.0))
UPS_LOW_HOLD_SECONDS = float(
    os.environ.get('PISLM_UPS_LOW_HOLD_SECONDS', 30.0))
UPS_POLL_SECONDS = float(os.environ.get('PISLM_UPS_POLL_SECONDS', 10.0))
#: Where the latest UPS reading is written (tmpfs -- ephemeral by design,
#: it is a live status, not a log). pislm.py reads this, best-effort.
UPS_STATUS_FILE = os.environ.get(
    'PISLM_UPS_STATUS_FILE', '/run/pislm-ups-status.json')

POLL_INTERVAL_S = 0.05
BLINK_INTERVAL_S = 0.125
GPIO_CHIP_PATH = '/dev/gpiochip0'


class ButtonInput:
    """A single input pin, pulled up, read as True when grounded (pressed)."""

    def __init__(self, pin, chip_path=GPIO_CHIP_PATH):
        pin = int(pin)
        if pin in MCC172_RESERVED_PINS:
            raise ValueError(
                'GPIO {} is used by the MCC 172 HAT; choose another pin '
                '(e.g. 27, 22, 23)'.format(pin))
        self.pin = pin
        self._backend = None
        self._request = None
        self._line = None
        self._gpio = None

        errors = []
        for opener in (self._open_gpiod_v2, self._open_gpiod_v1,
                       self._open_rpi_gpio):
            try:
                opener(pin, chip_path)
                return
            except ImportError as err:
                errors.append(str(err))
            except Exception as err:    # noqa: BLE001 - try next method
                errors.append('{}: {}'.format(type(err).__name__, err))
        raise RuntimeError(
            'no usable GPIO access method (tried gpiod v2, gpiod v1, '
            'RPi.GPIO): ' + ' | '.join(errors))

    # -- access-method openers ---------------------------------------------
    def _open_gpiod_v2(self, pin, chip_path):
        import gpiod
        from gpiod.line import Bias, Direction, Value
        self._request = gpiod.request_lines(
            chip_path, consumer='pislm-shutdown-button',
            config={pin: gpiod.LineSettings(direction=Direction.INPUT,
                                            bias=Bias.PULL_UP)})
        self._v2_value = Value
        self._backend = 'gpiod-v2'

    def _open_gpiod_v1(self, pin, chip_path):
        import gpiod
        if not hasattr(gpiod, 'Chip'):
            raise ImportError('gpiod module has no v1 Chip API')
        chip = gpiod.Chip(chip_path)
        line = chip.get_line(pin)
        try:
            line.request(consumer='pislm-shutdown-button',
                        type=gpiod.LINE_REQ_DIR_IN,
                        flags=gpiod.LINE_REQ_FLAG_BIAS_PULL_UP)
        except AttributeError:
            # Older libgpiod v1 bindings have no bias flag -- fall back to
            # a plain input request (needs an external pull-up resistor).
            line.request(consumer='pislm-shutdown-button',
                        type=gpiod.LINE_REQ_DIR_IN)
        self._line = line
        self._backend = 'gpiod-v1'

    def _open_rpi_gpio(self, pin, _chip_path):
        import RPi.GPIO as GPIO
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        self._gpio = GPIO
        self._backend = 'RPi.GPIO'

    # -- operations --------------------------------------------------------
    def is_pressed(self):
        """True when the pin reads low (button grounding the pulled-up pin)."""
        if self._backend == 'gpiod-v2':
            return self._request.get_value(self.pin) != self._v2_value.ACTIVE
        elif self._backend == 'gpiod-v1':
            return self._line.get_value() == 0
        else:
            return self._gpio.input(self.pin) == self._gpio.LOW

    def close(self):
        try:
            if self._backend == 'gpiod-v2':
                self._request.release()
            elif self._backend == 'gpiod-v1':
                self._line.release()
            elif self._backend == 'RPi.GPIO':
                self._gpio.cleanup(self.pin)
        except Exception:       # noqa: BLE001
            pass


class GpioLed:
    """Blinks a dedicated LED wired to a GPIO output pin.

    Raises on construction if the pin can't be opened (busy, no GPIO
    backend available, etc.) -- the caller is expected to fall back to
    StatusLed in that case. Note this doesn't confirm an LED is actually
    wired there (a GPIO output opens fine either way); it only confirms
    the pin itself is usable.
    """

    def __init__(self, pin):
        self._pin = GpioTrigger(pin)   # idles low, matching an LED off
        self._thread = None

    def on(self):
        """Steady on -- meant for "the system is up and running normally",
        distinct from the shutdown blink."""
        self._pin.set(True)

    def start_blink(self, interval=None):
        interval = BLINK_INTERVAL_S if interval is None else interval

        def _loop():
            on = True
            while True:
                try:
                    self._pin.set(on)
                except Exception:      # noqa: BLE001
                    return
                on = not on
                time.sleep(interval)

        self._thread = threading.Thread(target=_loop, daemon=True)
        self._thread.start()

    def close(self):
        self._pin.close()


class StatusLed:
    """Blinks the Pi's own onboard status LED (ACT / led0) over sysfs --
    no dedicated LED or resistor needed. Best-effort: if the board has no
    controllable status LED, or /sys is not writable, blinking is silently
    skipped so a missing LED never blocks the shutdown itself."""

    def __init__(self):
        self._brightness_path = None
        for name in ('ACT', 'led0', 'PWR', 'led1'):
            path = '/sys/class/leds/{}/brightness'.format(name)
            if os.path.exists(path):
                self._brightness_path = path
                break
        if self._brightness_path is None:
            candidates = glob.glob('/sys/class/leds/*/brightness')
            if candidates:
                self._brightness_path = candidates[0]
        trigger_path = self._brightness_path and self._brightness_path.replace(
            'brightness', 'trigger')
        if trigger_path and os.path.exists(trigger_path):
            try:
                with open(trigger_path, 'w') as f:
                    f.write('none')
            except OSError as err:
                print('shutdown_button: cannot take over status LED '
                      'trigger ({}); blinking will be skipped'.format(err),
                      file=sys.stderr)
                self._brightness_path = None
        self._thread = None

    def _write(self, value):
        with open(self._brightness_path, 'w') as f:
            f.write(str(value))

    def on(self):
        """Steady on -- meant for "the system is up and running normally",
        distinct from the shutdown blink. Best-effort, like everything
        else on this class."""
        if self._brightness_path is None:
            return
        try:
            self._write(1)
        except OSError:
            pass

    def start_blink(self, interval=BLINK_INTERVAL_S):
        if self._brightness_path is None:
            return

        def _loop():
            on = True
            while True:
                try:
                    self._write(1 if on else 0)
                except OSError:
                    return
                on = not on
                time.sleep(interval)

        self._thread = threading.Thread(target=_loop, daemon=True)
        self._thread.start()


def _open_ups():
    """Best-effort: returns an INA219 if one answers at the configured
    bus/address, else None. A missing/unwired UPS is not an error -- the
    button must keep working either way."""
    if not UPS_ENABLED:
        return None
    try:
        from ina219 import INA219
        ups = INA219(bus=UPS_I2C_BUS, address=UPS_I2C_ADDRESS,
                    v_min=UPS_V_MIN, v_max=UPS_V_MAX)
        ups.read_all()      # confirms the chip actually answers
        print('shutdown_button: UPS monitor on I2C bus {} address 0x{:02x} '
              '({:.1f}V-{:.1f}V = 0%-100%)'
              .format(UPS_I2C_BUS, UPS_I2C_ADDRESS, UPS_V_MIN, UPS_V_MAX))
        return ups
    except Exception as err:
        print('shutdown_button: no UPS at I2C bus {} address 0x{:02x} '
              '({}); battery monitoring disabled'
              .format(UPS_I2C_BUS, UPS_I2C_ADDRESS, err), file=sys.stderr)
        return None


def _write_ups_status(reading, low_since):
    """Best-effort status-file write -- a failure here (e.g. /run not
    writable) must never interrupt monitoring."""
    if UPS_STATUS_FILE is None:
        return
    payload = dict(reading, timestamp=time.time(),
                   low_battery_hold_seconds=(
                       round(time.monotonic() - low_since, 1)
                       if low_since is not None else 0.0))
    try:
        tmp = UPS_STATUS_FILE + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(payload, f)
        os.replace(tmp, UPS_STATUS_FILE)     # atomic -- no partial reads
    except OSError:
        pass


def _trigger_shutdown(led, reason):
    print('shutdown_button: {} -- powering off'.format(reason))
    # `systemctl poweroff` stops every other service first, and that
    # includes THIS one -- systemd sends SIGTERM as part of the normal
    # shutdown-target ordering, which (Python's default disposition)
    # kills the process almost immediately, cutting the blink off after
    # a fraction of a cycle or none at all. Ignore SIGTERM from here on so
    # the blink actually survives until the OS is truly going down (the
    # eventual SIGKILL in the last teardown phase, or our own 60s cap,
    # whichever comes first) -- safe to do only now, not for the whole
    # process lifetime, so `systemctl stop` still works normally at any
    # other time.
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    led.start_blink()
    subprocess.Popen(['systemctl', 'poweroff'])
    # Keep blinking (and this service alive) until the system actually
    # halts; the eventual SIGKILL (or this timeout) ends it.
    time.sleep(60.0)


def main():
    try:
        led = GpioLed(LED_PIN)
        print('shutdown_button: shutdown LED on GPIO {}'.format(LED_PIN))
    except Exception as err:
        print('shutdown_button: GPIO {} unavailable for the LED ({}); '
              'falling back to the onboard status LED'
              .format(LED_PIN, err), file=sys.stderr)
        led = StatusLed()
    led.on()   # steady on while the system is running; start_blink() takes
              # over (and this never goes solid again) once shutdown starts
    try:
        button = ButtonInput(BUTTON_PIN)
    except Exception as err:
        print('shutdown_button: {}'.format(err), file=sys.stderr)
        sys.exit(1)
    ups = _open_ups()

    print('shutdown_button: watching GPIO {} (hold {:.1f}s to power off)'
          .format(BUTTON_PIN, HOLD_SECONDS))

    pressed_since = None
    low_battery_since = None
    next_ups_poll = 0.0
    try:
        while True:
            now = time.monotonic()
            if button.is_pressed():
                if pressed_since is None:
                    pressed_since = now
                elif now - pressed_since >= HOLD_SECONDS:
                    _trigger_shutdown(
                        led, 'button held for {:.1f}s'.format(HOLD_SECONDS))
                    return
            else:
                pressed_since = None

            if ups is not None and now >= next_ups_poll:
                next_ups_poll = now + UPS_POLL_SECONDS
                try:
                    reading = ups.read_all()
                except Exception as err:
                    print('shutdown_button: UPS read failed ({}); will '
                          'retry'.format(err), file=sys.stderr)
                    low_battery_since = None
                else:
                    if reading['percent'] <= UPS_LOW_PERCENT:
                        if low_battery_since is None:
                            low_battery_since = now
                    else:
                        low_battery_since = None
                    _write_ups_status(reading, low_battery_since)
                    if (low_battery_since is not None and
                            now - low_battery_since >= UPS_LOW_HOLD_SECONDS):
                        _trigger_shutdown(
                            led, 'battery at {:.0f}% for >{:.0f}s'.format(
                                reading['percent'], UPS_LOW_HOLD_SECONDS))
                        return

            time.sleep(POLL_INTERVAL_S)
    finally:
        button.close()
        if ups is not None:
            ups.close()


if __name__ == '__main__':
    main()
