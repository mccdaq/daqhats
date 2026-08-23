#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
GPIO trigger-pulse output for the synchronized start of the PiSLM.

One Raspberry Pi GPIO pin, driven low at rest, is wired in parallel to the
trigger inputs of every acquisition device:

    GPIO <pin> ----+---- MCC 172 "TRIG" screw terminal (J5 pin 1)
                   +---- DT9837A "Ext Trigger" input
    GND       -----+---- MCC 172 "GND"  (J5 pin 2) / DT9837A ground

Both devices are armed to start on a RISING edge (the DT9837A's external
digital trigger only supports rising), so a single low->high pulse starts
both scans on the same edge. 3.3 V GPIO levels are sufficient: the MCC 172
trigger input threshold is 1.48 V max-min, and the DT9837A input is
TTL-compatible (V_IH 2.0 V).

Three GPIO access methods are tried in order, so this works across OS
generations (including Pi 5 / Trixie where the legacy interfaces are gone):

    1. libgpiod v2 Python bindings (python3-libgpiod >= 2.x)
    2. libgpiod v1 Python bindings
    3. RPi.GPIO (legacy)
"""
from __future__ import print_function

from time import sleep

#: BCM pins used by the MCC 172 HAT (SPI, EEPROM ID, address, clock/trigger
#: sharing, reset, IRQ) -- never drive these as the trigger output.
MCC172_RESERVED_PINS = frozenset(
    (0, 1, 5, 6, 8, 9, 10, 11, 12, 13, 16, 19, 20, 26))


class GpioTrigger:
    """A single output pin that idles low and emits rising-edge pulses."""

    def __init__(self, pin, chip_path='/dev/gpiochip0'):
        pin = int(pin)
        if pin in MCC172_RESERVED_PINS:
            raise ValueError(
                'GPIO {} is used by the MCC 172 HAT; choose another pin '
                '(e.g. 17, 27, 22)'.format(pin))
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
        from gpiod.line import Direction, Value
        self._request = gpiod.request_lines(
            chip_path, consumer='pislm-trigger',
            config={pin: gpiod.LineSettings(direction=Direction.OUTPUT,
                                            output_value=Value.INACTIVE)})
        self._v2_value = Value
        self._backend = 'gpiod-v2'

    def _open_gpiod_v1(self, pin, chip_path):
        import gpiod
        if not hasattr(gpiod, 'Chip'):
            raise ImportError('gpiod module has no v1 Chip API')
        chip = gpiod.Chip(chip_path)
        line = chip.get_line(pin)
        line.request(consumer='pislm-trigger',
                     type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
        self._line = line
        self._backend = 'gpiod-v1'

    def _open_rpi_gpio(self, pin, _chip_path):
        import RPi.GPIO as GPIO
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)
        self._gpio = GPIO
        self._backend = 'RPi.GPIO'

    # -- operations --------------------------------------------------------
    def _set(self, high):
        if self._backend == 'gpiod-v2':
            value = self._v2_value.ACTIVE if high else self._v2_value.INACTIVE
            self._request.set_value(self.pin, value)
        elif self._backend == 'gpiod-v1':
            self._line.set_value(1 if high else 0)
        else:
            self._gpio.output(self.pin, 1 if high else 0)

    def pulse(self, width_s=0.01):
        """Emit one rising edge (low -> high, hold, -> low)."""
        self._set(False)
        sleep(0.001)            # guarantee a clean low before the edge
        self._set(True)
        sleep(max(0.0001, width_s))
        self._set(False)

    def set(self, high):
        """Drive the pin directly (e.g. for a status LED, not just a
        trigger pulse)."""
        self._set(high)

    def close(self):
        try:
            self._set(False)
        except Exception:       # noqa: BLE001 - releasing best-effort
            pass
        try:
            if self._backend == 'gpiod-v2':
                self._request.release()
            elif self._backend == 'gpiod-v1':
                self._line.release()
            elif self._backend == 'RPi.GPIO':
                self._gpio.cleanup(self.pin)
        except Exception:       # noqa: BLE001
            pass
