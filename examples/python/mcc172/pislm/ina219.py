#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
INA219 battery/current monitor driver, as used on the Waveshare UPS HAT
family (and most other INA219-based Raspberry Pi UPS boards).

Register map and calibration constants are the same ones Waveshare's own
UPS HAT demo code uses (32V/2A calibration, current_lsb=0.1mA,
power_lsb=2mW, cal_value=4096), so voltage/current/power readings match
what any Waveshare tool would show.

The bus-voltage-to-percentage formula is linear between v_min (empty) and
v_max (full), defaulting to 6.0V/8.4V -- Waveshare's own default, for a 2S
Li-ion pack (2 * 3.0V .. 2 * 4.2V). **This does NOT universally hold** --
confirmed in the field on a board whose actual pack is 3S (9.0V/12.6V),
reading a real ~95%-full 12.4V as a meaningless 100% under the 2S default
(silently wrong, not an error, since anything above v_max just clamps).
Always set v_min/v_max (constructor args, or PISLM_UPS_V_MIN/_V_MAX in
shutdown_button.py) to match your actual pack's series cell count -- see
INSTALL.md 14.1.

Requires smbus2 (``pip install smbus2`` -- pure Python, no compile step).
"""
from __future__ import print_function

#: Register addresses.
_REG_CONFIG = 0x00
_REG_SHUNT_VOLTAGE = 0x01
_REG_BUS_VOLTAGE = 0x02
_REG_POWER = 0x03
_REG_CURRENT = 0x04
_REG_CALIBRATION = 0x05

#: 32V bus range, /8 gain (320mV shunt range), 12-bit ADC, continuous
#: shunt+bus conversion -- the standard configuration Waveshare's demo
#: code uses for the UPS HAT's 0.1 ohm shunt.
_CONFIG_32V_2A = 0x399F

#: Calibration for a 0.1 ohm shunt at up to ~3.2A: current_lsb = 0.1 mA,
#: power_lsb = 20 * current_lsb = 2 mW, cal = 0.04096 / (current_lsb * Rshunt).
_CAL_VALUE = 4096
_CURRENT_LSB_MA = 0.1
_POWER_LSB_W = 0.002


class INA219:
    """One INA219 chip on the given I2C bus/address.

    Register reads/writes are manual big-endian byte pairs (not
    smbus2's word_data helpers, which are little-endian per the SMBus
    spec) -- the INA219 registers are MSB-first, so using word_data
    directly would silently byte-swap every value.
    """

    def __init__(self, bus=1, address=0x41, v_min=6.0, v_max=8.4):
        import smbus2
        self.address = address
        self.v_min = v_min
        self.v_max = v_max
        self._bus = smbus2.SMBus(bus)
        self._write16(_REG_CONFIG, _CONFIG_32V_2A)
        self._write16(_REG_CALIBRATION, _CAL_VALUE)

    def _write16(self, register, value):
        value &= 0xFFFF
        self._bus.write_i2c_block_data(
            self.address, register, [(value >> 8) & 0xFF, value & 0xFF])

    def _read16(self, register):
        data = self._bus.read_i2c_block_data(self.address, register, 2)
        return (data[0] << 8) | data[1]

    def _read16_signed(self, register):
        value = self._read16(register)
        return value - 0x10000 if value & 0x8000 else value

    def read_shunt_voltage_mv(self):
        return self._read16_signed(_REG_SHUNT_VOLTAGE) * 0.01

    def read_bus_voltage_v(self):
        # Bits 15..3 are the 13-bit reading; bits 2..0 are status flags.
        return (self._read16(_REG_BUS_VOLTAGE) >> 3) * 0.004

    def read_current_ma(self):
        # Calibration must be reloaded before each current/power read --
        # the chip's internal calibration register self-clears under some
        # brownout/reset conditions, silently zeroing subsequent readings
        # otherwise. Matches Waveshare's own driver.
        self._write16(_REG_CALIBRATION, _CAL_VALUE)
        return self._read16_signed(_REG_CURRENT) * _CURRENT_LSB_MA

    def read_power_w(self):
        self._write16(_REG_CALIBRATION, _CAL_VALUE)
        return self._read16(_REG_POWER) * _POWER_LSB_W

    def read_percentage(self, v_min=None, v_max=None):
        """Battery percentage from bus voltage, linear between v_min
        (empty) and v_max (full), clamped to [0, 100]. Defaults to the
        instance's v_min/v_max (constructor args) if not given here --
        Waveshare's own formula uses 6.0V/8.4V for a 2S Li-ion pack (2 *
        3.0V .. 2 * 4.2V), but boards/packs vary: e.g. a 3S pack (3 *
        3.0V .. 3 * 4.2V) is 9.0V/12.6V. Passing the wrong range doesn't
        error -- it just clamps to a meaningless but harmless 0% or 100%,
        so get this right for your actual pack (see INSTALL.md 14.1)."""
        v_min = self.v_min if v_min is None else v_min
        v_max = self.v_max if v_max is None else v_max
        pct = (self.read_bus_voltage_v() - v_min) / (v_max - v_min) * 100.0
        return max(0.0, min(100.0, pct))

    def read_all(self):
        """One snapshot: bus voltage, current, power, and percentage.
        Reads bus voltage once and derives percent from that same value
        (rather than calling read_percentage(), which would read it
        again) so both fields are consistent with each other."""
        bus_v = self.read_bus_voltage_v()
        pct = (bus_v - self.v_min) / (self.v_max - self.v_min) * 100.0
        return {
            'bus_voltage_v': bus_v,
            'shunt_voltage_mv': self.read_shunt_voltage_mv(),
            'current_ma': self.read_current_ma(),
            'power_w': self.read_power_w(),
            'percent': max(0.0, min(100.0, pct)),
        }

    def close(self):
        try:
            self._bus.close()
        except Exception:      # noqa: BLE001
            pass
