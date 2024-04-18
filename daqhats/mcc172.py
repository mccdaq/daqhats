# pylint: disable=too-many-lines
"""
Wraps all of the methods from the MCC 172 library for use in Python.
"""
import sys
from collections import namedtuple
from ctypes import c_ubyte, c_int, c_ushort, c_ulong, c_long, c_double, \
    POINTER, c_char_p, byref, create_string_buffer
from enum import IntEnum, unique
from daqhats.hats import Hat, HatError

@unique
class SourceType(IntEnum):
    """Clock / trigger source options."""
    LOCAL = 0     #: Use a local-only source.
    MASTER = 1    #: Use a local source and set it as master.
    SLAVE = 2     #: Use a master source from another MCC 172.

class mcc172(Hat): # pylint: disable=invalid-name, too-many-public-methods
    """
    The class for an MCC 172 board.

    Args:
        address (int): board address, must be 0-7.

    Raises:
        HatError: the board did not respond or was of an incorrect type
    """

    _AIN_NUM_CHANNELS = 2         # Number of analog channels

    _STATUS_HW_OVERRUN = 0x0001
    _STATUS_BUFFER_OVERRUN = 0x0002
    _STATUS_TRIGGERED = 0x0004
    _STATUS_RUNNING = 0x0008

    _MAX_SAMPLE_RATE = 51200.0

    _dev_info_type = namedtuple(
        'MCC172DeviceInfo', [
            'NUM_AI_CHANNELS', 'AI_MIN_CODE', 'AI_MAX_CODE',
            'AI_MIN_VOLTAGE', 'AI_MAX_VOLTAGE', 'AI_MIN_RANGE',
            'AI_MAX_RANGE'])

    _dev_info = _dev_info_type(
        NUM_AI_CHANNELS=2,
        AI_MIN_CODE=-8388608,
        AI_MAX_CODE=8388607,
        AI_MIN_VOLTAGE=-5.0,
        AI_MAX_VOLTAGE=(5.0 - (10.0/16777216)),
        AI_MIN_RANGE=-5.0,
        AI_MAX_RANGE=+5.0)

    def __init__(self, address=0): # pylint: disable=too-many-statements
        """
        Initialize the class.
        """
        # call base class initializer
        Hat.__init__(self, address)

        # set up library argtypes and restypes
        self._lib.mcc172_open.argtypes = [c_ubyte]
        self._lib.mcc172_open.restype = c_int

        self._lib.mcc172_close.argtypes = [c_ubyte]
        self._lib.mcc172_close.restype = c_int

        self._lib.mcc172_blink_led.argtypes = [c_ubyte, c_ubyte]
        self._lib.mcc172_blink_led.restype = c_int

        self._lib.mcc172_firmware_version.argtypes = [
            c_ubyte, POINTER(c_ushort)]
        self._lib.mcc172_firmware_version.restype = c_int

        self._lib.mcc172_serial.argtypes = [c_ubyte, c_char_p]
        self._lib.mcc172_serial.restype = c_int

        self._lib.mcc172_calibration_date.argtypes = [c_ubyte, c_char_p]
        self._lib.mcc172_calibration_date.restype = c_int

        self._lib.mcc172_calibration_coefficient_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_double), POINTER(c_double)]
        self._lib.mcc172_calibration_coefficient_read.restype = c_int

        self._lib.mcc172_calibration_coefficient_write.argtypes = [
            c_ubyte, c_ubyte, c_double, c_double]
        self._lib.mcc172_calibration_coefficient_write.restype = c_int

        self._lib.mcc172_iepe_config_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc172_iepe_config_read.restype = c_int

        self._lib.mcc172_iepe_config_write.argtypes = [
            c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc172_iepe_config_write.restype = c_int

        self._lib.mcc172_a_in_sensitivity_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_double)]
        self._lib.mcc172_a_in_sensitivity_read.restype = c_int

        self._lib.mcc172_a_in_sensitivity_write.argtypes = [
            c_ubyte, c_ubyte, c_double]
        self._lib.mcc172_a_in_sensitivity_write.restype = c_int

        self._lib.mcc172_a_in_clock_config_read.argtypes = [
            c_ubyte, POINTER(c_ubyte), POINTER(c_double),
            POINTER(c_ubyte)]
        self._lib.mcc172_a_in_clock_config_read.restype = c_int

        self._lib.mcc172_a_in_clock_config_write.argtypes = [
            c_ubyte, c_ubyte, c_double]
        self._lib.mcc172_a_in_clock_config_write.restype = c_int

        self._lib.mcc172_trigger_config.argtypes = [c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc172_trigger_config.restype = c_int

        self._lib.mcc172_a_in_scan_start.argtypes = [
            c_ubyte, c_ubyte, c_ulong, c_ulong]
        self._lib.mcc172_a_in_scan_start.restype = c_int

        self._lib.mcc172_a_in_scan_status.argtypes = [
            c_ubyte, POINTER(c_ushort), POINTER(c_ulong)]
        self._lib.mcc172_a_in_scan_status.restype = c_int

        self._lib.mcc172_a_in_scan_buffer_size.argtypes = [
            c_ubyte, POINTER(c_ulong)]
        self._lib.mcc172_a_in_scan_buffer_size.restype = c_int

        self._lib.mcc172_a_in_scan_read.restype = c_int

        self._lib.mcc172_a_in_scan_stop.argtypes = [c_ubyte]
        self._lib.mcc172_a_in_scan_stop.restype = c_int

        self._lib.mcc172_a_in_scan_cleanup.argtypes = [c_ubyte]
        self._lib.mcc172_a_in_scan_cleanup.restype = c_int

        self._lib.mcc172_a_in_scan_channel_count.argtypes = [c_ubyte]
        self._lib.mcc172_a_in_scan_channel_count.restype = c_ubyte

        self._lib.mcc172_test_signals_read.argtypes = [
            c_ubyte, POINTER(c_ubyte), POINTER(c_ubyte), POINTER(c_ubyte)]
        self._lib.mcc172_test_signals_read.restype = c_int

        self._lib.mcc172_test_signals_write.argtypes = [
            c_ubyte, c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc172_test_signals_write.restype = c_int

        result = self._lib.mcc172_open(self._address)

        if result == self._RESULT_SUCCESS:
            self._initialized = True
        elif result == self._RESULT_INVALID_DEVICE:
            raise HatError(self._address, "Invalid board type.")
        else:
            raise HatError(self._address, "Board not responding.")

        return

    def __del__(self):
        if self._initialized:
            self._lib.mcc172_a_in_scan_cleanup(self._address)
            self._lib.mcc172_close(self._address)
        return

    @staticmethod
    def info():
        """
        Return constant information about this type of device.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **NUM_AI_CHANNELS** (int): The number of analog input channels
              (2.)
            * **AI_MIN_CODE** (int): The minimum ADC code (-8388608.)
            * **AI_MAX_CODE** (int): The maximum ADC code (8388607.)
            * **AI_MIN_VOLTAGE** (float): The voltage corresponding to the
              minimum ADC code (-5.0.)
            * **AI_MAX_VOLTAGE** (float): The voltage corresponding to the
              maximum ADC code (+5.0 - 1 LSB)
            * **AI_MIN_RANGE** (float): The minimum voltage of the input range
              (-5.0.)
            * **AI_MAX_RANGE** (float): The maximum voltage of the input range
              (+5.0.)
        """
        return mcc172._dev_info

    def firmware_version(self):
        """
        Read the board firmware and bootloader versions.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **version** (string): The firmware version, i.e "1.03".

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        version = c_ushort()
        if (self._lib.mcc172_firmware_version(self._address, byref(version))
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        version_str = "{0:X}.{1:02X}".format(
            version.value >> 8, version.value & 0x00FF)
        version_info = namedtuple(
            'MCC172VersionInfo', ['version'])
        return version_info(version=version_str)

    def serial(self):
        """
        Read the serial number.

        Returns:
            string: The serial number.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        # create string to hold the result
        my_buffer = create_string_buffer(9)
        if (self._lib.mcc172_serial(self._address, my_buffer)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        my_serial = my_buffer.value.decode('ascii')
        return my_serial

    def blink_led(self, count):
        """
        Blink the MCC 172 LED.

        Setting count to 0 will cause the LED to blink continuously until
        blink_led() is called again with a non-zero count.

        Args:
            count (int): The number of times to blink (max 255).

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        if (self._lib.mcc172_blink_led(self._address, count)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return

    def calibration_date(self):
        """
        Read the calibration date.

        Returns:
            string: The calibration date in the format "YYYY-MM-DD".

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        # create string to hold the result
        my_buffer = create_string_buffer(11)
        if (self._lib.mcc172_calibration_date(self._address, my_buffer)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        my_date = my_buffer.value.decode('ascii')
        return my_date

    def calibration_coefficient_read(self, channel):
        """
        Read the calibration coefficients for a single channel.

        The coefficients are applied in the library as: ::

            calibrated_ADC_code = (raw_ADC_code - offset) * slope

        Args:
            channel (int): The analog input channel (0-1.)

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **slope** (float): The slope.
            * **offset** (float): The offset.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        slope = c_double()
        offset = c_double()
        if (self._lib.mcc172_calibration_coefficient_read(
                self._address, channel, byref(slope), byref(offset))
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        cal_info = namedtuple('MCC172CalInfo', ['slope', 'offset'])
        return cal_info(
            slope=slope.value,
            offset=offset.value)

    def calibration_coefficient_write(self, channel, slope, offset):
        """
        Temporarily write the calibration coefficients for a single channel.

        The user can apply their own calibration coefficients by writing to
        these values. The values will reset to the factory values from the
        EEPROM whenever the class is initialized.  This function will fail and
        raise a HatError exception if a scan is active when it is called.

        The coefficients are applied in the library as: ::

            calibrated_ADC_code = (raw_ADC_code - offset) * slope

        Args:
            channel (int): The analog input channel (0-1.)
            slope (float): The new slope value.
            offset (float): The new offset value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        if (self._lib.mcc172_calibration_coefficient_write(
                self._address, channel, slope, offset)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return

    def iepe_config_write(self, channel, mode):
        """
        Configure a channel for an IEPE sensor.

        This method turns on / off the IEPE power supply for the specified
        channel. The power-on default is IEPE power off.

        Args:
            channel (int): The channel, 0 or 1.
            mode (int): The IEPE mode for the channel, 0 = IEPE off,
                1 = IEPE on.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        if (self._lib.mcc172_iepe_config_write(self._address, channel, mode) !=
                self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return

    def iepe_config_read(self, channel):
        """
        Read the IEPE configuration for a channel.

        This method returns the state of the IEPE power supply for the specified
        channel

        Args:
            channel (int): The channel, 0 or 1.

        Returns
            int: The IEPE mode for the channel, 0 = IEPE off, 1 = IEPE on.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        mode = c_ubyte()
        if (self._lib.mcc172_iepe_config_read(
                self._address, channel, byref(mode)) != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return mode.value

    def a_in_sensitivity_write(self, channel, value):
        """
        Write the MCC 172 analog input sensitivity scaling factor for a single
        channel.

        This applies a scaling factor to the analog input data so it returns
        values that are meaningful for the connected sensor.

        The sensitivity is specified in mV / mechanical unit. The default value
        when opening the library is 1000, resulting in no scaling of the input
        voltage.  Changing this value will not change the values reported by
        :py:func:`info` since it is simply sensor scaling applied to the data
        before returning it.

        Examples:

        * A seismic sensor with a sensitivity of 10 V/g. Set the sensitivity to
          10,000 and the returned data will be in units of g.
        * A vibration sensor with a sensitivity of 100 mV/g.  Set the
          sensitivity to 100 and the returned data will be in units of g.

        Args:
            channel (int): The channel, 0 or 1.
            value (float): The sensitivity for the specified channel.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        result = self._lib.mcc172_a_in_sensitivity_write(
            self._address, channel, value)
        if result == self._RESULT_BUSY:
            raise HatError(
                self._address, "Cannot change the sensitivity "
                "while a scan is active.")
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return

    def a_in_sensitivity_read(self, channel):
        """
        Read the MCC 172 analog input sensitivity scaling factor for a single
            channel.

        The sensitivity is returned in mV / mechanical unit. The default value
        when opening the library is 1000, resulting in no scaling of the input
        voltage.

        Args:
            channel (int): The channel, 0 or 1.

        Returns
            float: The sensitivity factor for the channel.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        value = c_double()
        if (self._lib.mcc172_a_in_sensitivity_read(
                self._address, channel, byref(value)) != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return value.value

    def a_in_clock_config_write(
            self, clock_source, sample_rate_per_channel):
        """
        Configure the ADC sampling clock.

        This method will configure the ADC sampling clock. The default
        configuration after opening the device is local mode, 51.2 KHz sampling
        rate. The clock source must be one of:

        * :py:const:`SourceType.LOCAL`: the clock is generated on this MCC 172
          and not shared with any other devices.
        * :py:const:`SourceType.MASTER`: the clock is generated on this MCC 172
          and shared over the Raspberry Pi header with other MCC 172s. All other
          MCC 172s must be configured for local or slave clock.
        * :py:const:`SourceType.SLAVE`: no clock is generated on this MCC 172,
          it receives its clock from the Raspberry Pi header. Another MCC 172
          must be configured for master clock.

        The ADCs will be synchronized so they sample the inputs at the same
        time. This requires 128 clock cycles before the first sample is
        available. When using a master - slave clock configuration there are
        additional considerations:

        * There should be only one master device; otherwise, you will be
          connecting multiple outputs together and could damage a device.
        * Configure the clock on the slave device(s) first, master last. The
          synchronization will occur when the master clock is configured,
          causing the ADCs on all the devices to be in sync.
        * If you change the clock configuration on one device after configuring
          the master, then the data will no longer be in sync. The devices
          cannot detect this and will still report that they are synchronized.
          Always write the clock configuration to all devices when modifying the
          configuration.
        * Slave devices must have a master clock source or scans will never
          complete.
        * A trigger must be used for the data streams from all devices to start
          on the same sample.

        The MCC 172 can generate a sampling clock equal to 51.2 KHz divided by
        an integer between 1 and 256. The sample_rate_per_channel will be
        internally converted to the nearest valid rate. The actual rate can be
        read back using :py:func:`a_in_clock_config_read`. When used in slave
        clock configuration, the device will measure the frequency of the
        incoming master clock after the synchronization period is complete.
        Calling :py:func:`a_in_clock_config_read` after this will return the
        measured sample rate.

        Args:
            clock_source (:py:class:`SourceType`): The ADC clock source.
            sample_rate_per_channel (float): The requested sampling rate in
                samples per second per channel.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        result = self._lib.mcc172_a_in_clock_config_write(
            self._address, clock_source, sample_rate_per_channel)
        if result == self._RESULT_BUSY:
            raise HatError(
                self._address, "Cannot change the clock "
                "configuration while a scan is active.")
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return

    def a_in_clock_config_read(self):
        """
        Read the sampling clock configuration.

        This method will return the sample clock configuration and rate. If the
        clock is configured for local or master source, then the rate will be
        the internally adjusted rate set by the user.  If the clock is
        configured for slave source, then the rate will be measured from the
        master clock after the synchronization period has ended. The
        synchronization status is also returned.

        The clock source will be one of:

        * :py:const:`SourceType.LOCAL`: the clock is generated on this MCC 172
          and not shared with any other devices.
        * :py:const:`SourceType.MASTER`: the clock is generated on this MCC 172
          and shared over the Raspberry Pi header with other MCC 172s.
        * :py:const:`SourceType.SLAVE`: no clock is generated on this MCC 172,
          it receives its clock from the Raspberry Pi header.

        The sampling rate will not be valid in slave mode if synced is False.
        The device will not detect a loss of the master clock when in slave
        mode; it only monitors the clock when a sync is initiated.

        Returns
            namedtuple: a namedtuple containing the following field names:

            * **clock_source** (:py:class:`SourceType`): The ADC clock source.
            * **sample_rate_per_channel** (float): The sample rate in
              samples per second per channel.
            * **synchronized** (bool): True if the ADCs are synchronized, False
              if a synchronization is in progress.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        clock_source = c_ubyte()
        sample_rate_per_channel = c_double()
        synced = c_ubyte()
        result = self._lib.mcc172_a_in_clock_config_read(
            self._address, byref(clock_source),
            byref(sample_rate_per_channel), byref(synced))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        clock_config = namedtuple(
            'MCC172ClockConfig',
            ['clock_source', 'sample_rate_per_channel',
             'synchronized'])
        return clock_config(
            clock_source=clock_source.value,
            sample_rate_per_channel=sample_rate_per_channel.value,
            synchronized=synced.value != 0)

    def trigger_config(self, trigger_source, trigger_mode):
        """
        Configure the digital trigger.

        The analog input scan may be configured to start saving the acquired
        data when the digital trigger is in the desired state. A single device
        trigger may also be shared with multiple boards. This command sets the
        trigger source and mode.

        The trigger source must be one of:

        * :py:const:`SourceType.LOCAL`: the trigger terminal on this MCC 172 is
          used and not shared with any other devices.
        * :py:const:`SourceType.MASTER`: the trigger terminal on this MCC 172 is
          used and is shared as the master trigger for other MCC 172s.
        * :py:const:`SourceType.SLAVE`: the trigger terminal on this MCC 172 is
          not used, it receives its trigger from the master MCC 172.

        The trigger mode must be one of:

        * :py:const:`TriggerModes.RISING_EDGE`: Start saving data when the
          trigger transitions from low to high.
        * :py:const:`TriggerModes.FALLING_EDGE`: Start saving data when the
          trigger transitions from high to low.
        * :py:const:`TriggerModes.ACTIVE_HIGH`: Start saving data when the
          trigger is high.
        * :py:const:`TriggerModes.ACTIVE_LOW`: Start saving data when the
          trigger is low.

        Due to the nature of the filtering in the A/D converters there is an
        input delay of 39 samples, so the data coming from the converters at any
        time is delayed by 39 samples from the current time.  This is most
        noticeable when using a trigger - there will be approximately 39 samples
        prior to the trigger event in the captured data.

        Care must be taken when using master / slave triggering; the input
        trigger signal on the master will be passed through to the slave(s), but
        the mode is set independently on each device. For example, it is
        possible for the master to trigger on the rising edge of the signal and
        the slave to trigger on the falling edge.

        Args:
            trigger_source (:py:class:`SourceType`): The trigger source.
            trigger_mode (:py:class:`TriggerModes`): The trigger mode.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        result = self._lib.mcc172_trigger_config(
            self._address, trigger_source, trigger_mode)
        if result == self._RESULT_BUSY:
            raise HatError(
                self._address, "Cannot write trigger configuration "
                "while a scan is active.")
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return

    @staticmethod
    def a_in_scan_actual_rate(sample_rate_per_channel):
        """
        Calculate the actual sample rate per channel for a requested sample
        rate.

        The scan clock is generated from a 51.2 KHz clock source divided by an
        integer between 1 and 256, so only discrete frequency steps can be
        achieved.  This method will return the actual rate for a requested
        sample rate.

        This function does not perform any actions with a board, it simply
        calculates the rate.

        Args:
            sample_rate_per_channel (float): The desired per-channel rate of the
                internal sampling clock.

        Returns:
            float: The actual sample rate.
        """
        divisor = 51200.0 / sample_rate_per_channel + 0.5
        if divisor < 1.0:
            divisor = 1.0
        elif divisor > 256.0:
            divisor = 256.0

        sample_rate = 51200.0 / int(divisor)
        return sample_rate

    def a_in_scan_start(self, channel_mask, samples_per_channel, options):
        """
        Start capturing analog input data.

        The scan runs as a separate thread from the user's code. This function
        will allocate a scan buffer and start the thread that reads data from
        the device into that buffer. The user reads the data from the scan
        buffer and the scan status using the :py:func:`a_in_scan_read` function.
        :py:func:`a_in_scan_stop` is used to stop a continuous scan, or to stop
        a finite scan before it completes. The user must call
        :py:func:`a_in_scan_cleanup` after the scan has finished and all desired
        data has been read; this frees all resources from the scan and allows
        additional scans to be performed.

        The scan cannot be started until the ADCs are synchronized, so this
        function will not return until that has completed. It is best to wait
        for sync using :py:func:`a_in_clock_config_read` before starting the
        scan.

        The scan state has defined terminology:

        * **Active**: :py:func:`a_in_scan_start` has been called and the device
          may be acquiring data or finished with the acquisition. The scan has
          not been cleaned up by calling :py:func:`a_in_scan_cleanup`, so
          another scan may not be started.
        * **Running**: The scan is active and the device is still acquiring
          data. Certain methods like :py:func:`a_in_clock_config_write` will
          return an error because the device is busy.

        The scan options that may be used are below. Multiple options can be 
        combined with OR (|).

        * :py:const:`OptionFlags.DEFAULT`: Return scaled and calibrated data,
          do not use a trigger, and finite operation. Any other flags will
          override DEFAULT behavior.
        * :py:const:`OptionFlags.NOSCALEDATA`: Return ADC codes (values between
          -8,388,608 and 8,388,607) rather than voltage.
        * :py:const:`OptionFlags.NOCALIBRATEDATA`: Return data without the
          calibration factors applied.
        * :py:const:`OptionFlags.EXTTRIGGER`: Do not start saving data (after
          calling :py:func:`a_in_scan_start`) until the trigger condition is
          met. The trigger is configured with :py:func:`trigger_config`.
        * :py:const:`OptionFlags.CONTINUOUS`: Read analog data continuously
          until stopped by the user by calling :py:func:`a_in_scan_stop` and
          write data to a circular buffer. The data must be read before being
          overwritten to avoid a buffer overrun error. **samples_per_channel**
          is only used for buffer sizing.

        The :py:const:`OptionFlags.EXTCLOCK` option is not supported for this
        device and will raise a ValueError.

        The scan buffer size will be allocated as follows:

        **Finite mode:** Total number of samples in the scan.

        **Continuous mode:** Either **samples_per_channel** or the value in the
        table below, whichever is greater.

        =================      =========================
        Sample Rate            Buffer Size (per channel)
        =================      =========================
        200-1024 S/s             1 kS
        1280-10.24 kS/s         10 kS
        12.8 kS or higher      100 kS
        =================      =========================

        Specifying a very large value for samples_per_channel could use too much
        of the Raspberry Pi memory. If the memory allocation fails, the function
        will raise a HatError with this description. The allocation could
        succeed, but the lack of free memory could cause other problems in the
        Raspberry Pi. If you need to acquire a high number of samples then it is
        better to run the scan in continuous mode and stop it when you have
        acquired the desired amount of data. If a scan is active this method
        will raise a HatError.

        Args:
            channel_mask (int): A bit mask of the desired channels (0x01 -
                0x03).
            samples_per_channel (int): The number of samples to acquire per
                channel (finite mode,) or or can be used to set a larger scan
                buffer size than the default value (continuous mode.)
            options (int): An ORed combination of :py:class:`OptionFlags` flags
                that control the scan.

        Raises:
            HatError: a scan is active; memory could not be allocated; the board
                is not initialized, does not respond, or responds incorrectly.
            ValueError: a scan argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        # Perform some argument checking
        if (channel_mask == 0) or (channel_mask > 3):
            raise ValueError("channel_mask must be 1 - 3")

        num_channels = 0
        for index in range(2):
            bit_mask = 1 << index
            if (channel_mask & bit_mask) != 0:
                num_channels += 1

        result = self._lib.mcc172_a_in_scan_start(
            self._address, channel_mask, samples_per_channel, options)

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid scan parameter.")
        elif result == self._RESULT_BUSY:
            raise HatError(self._address, "A scan is already active.")
        elif result == self._RESULT_RESOURCE_UNAVAIL:
            raise HatError(self._address, "Memory could not be allocated.")
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response {}.".format(
                result))
        return

    def a_in_scan_buffer_size(self):
        """
        Read the internal scan data buffer size.

        An internal data buffer is allocated for the scan when
        :py:func:`a_in_scan_start` is called. This function returns the total
        size of that buffer in samples.

        Returns:
            int: The buffer size in samples.

        Raises:
            HatError: the board is not initialized or no scan buffer is
                allocated (a scan is not active).
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        data_value = c_ulong()

        result = self._lib.mcc172_a_in_scan_buffer_size(
            self._address, byref(data_value))

        if result == self._RESULT_RESOURCE_UNAVAIL:
            raise HatError(self._address, "No scan is active.")
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response {}.".format(
                result))
        return data_value.value

    def a_in_scan_status(self):
        """
        Read scan status and number of available samples per channel.

        The analog input scan is started with :py:func:`a_in_scan_start` and
        runs in the background.  This function reads the status of that
        background scan and the number of samples per channel available in
        the scan thread buffer.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **running** (bool): True if the scan is running, False if it has
              stopped or completed.
            * **hardware_overrun** (bool): True if the hardware could not
              acquire and unload samples fast enough and data was lost.
            * **buffer_overrun** (bool): True if the background scan buffer was
              not read fast enough and data was lost.
            * **triggered** (bool): True if the trigger conditions have been met
              and data acquisition started.
            * **samples_available** (int): The number of samples per channel
              currently in the scan buffer.

        Raises:
            HatError: A scan is not active, the board is not initialized, does
                not respond, or responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        status = c_ushort(0)
        samples_available = c_ulong(0)

        result = self._lib.mcc172_a_in_scan_status(
            self._address, byref(status), byref(samples_available))

        if result == self._RESULT_RESOURCE_UNAVAIL:
            raise HatError(self._address, "Scan not active.")
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response {}.".format(
                result))

        scan_status = namedtuple(
            'MCC172ScanStatus',
            ['running', 'hardware_overrun', 'buffer_overrun', 'triggered',
             'samples_available'])
        return scan_status(
            running=(status.value & self._STATUS_RUNNING) != 0,
            hardware_overrun=(status.value & self._STATUS_HW_OVERRUN) != 0,
            buffer_overrun=(status.value & self._STATUS_BUFFER_OVERRUN) != 0,
            triggered=(status.value & self._STATUS_TRIGGERED) != 0,
            samples_available=samples_available.value)

    def a_in_scan_read(self, samples_per_channel, timeout):
        # pylint: disable=too-many-locals
        """
        Read scan status and data (as a list).

        The analog input scan is started with :py:func:`a_in_scan_start` and
        runs in the background.  This function reads the status of that
        background scan and optionally reads sampled data from the scan buffer.

        Args:
            samples_per_channel (int): The number of samples per channel to read
                from the scan buffer. Specify a negative number to return all
                available samples immediately and ignore **timeout** or 0 to
                only read the scan status and return no data.
            timeout (float): The amount of time in seconds to wait for the
                samples to be read. Specify a negative number to wait
                indefinitely, or 0 to return immediately with the samples that
                are already in the scan buffer (up to **samples_per_channel**.)
                If the timeout is met and the specified number of samples have
                not been read, then the function will return all the available
                samples and the timeout status set.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **running** (bool): True if the scan is running, False if it has
              stopped or completed.
            * **hardware_overrun** (bool): True if the hardware could not
              acquire and unload samples fast enough and data was lost.
            * **buffer_overrun** (bool): True if the background scan buffer was
              not read fast enough and data was lost.
            * **triggered** (bool): True if the trigger conditions have been met
              and data acquisition started.
            * **timeout** (bool): True if the timeout time expired before the
              specified number of samples were read.
            * **data** (list of float): The data that was read from the scan
              buffer.

        Raises:
            HatError: A scan is not active, the board is not initialized, does
                not respond, or responds incorrectly.
            ValueError: Incorrect argument.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        num_channels = self._lib.mcc172_a_in_scan_channel_count(self._address)

        self._lib.mcc172_a_in_scan_read.argtypes = [
            c_ubyte, POINTER(c_ushort), c_long, c_double, POINTER(c_double),
            c_ulong, POINTER(c_ulong)]

        samples_read_per_channel = c_ulong(0)
        samples_to_read = 0
        status = c_ushort(0)
        timed_out = False

        if samples_per_channel < 0:
            # read all available data

            # first, get the number of available samples
            samples_available = c_ulong(0)
            result = self._lib.mcc172_a_in_scan_status(
                self._address, byref(status), byref(samples_available))

            if result != self._RESULT_SUCCESS:
                raise HatError(self._address, "Incorrect response {}.".format(
                    result))

            # allocate a buffer large enough for all the data
            samples_to_read = samples_available.value
            buffer_size = samples_available.value * num_channels
            data_buffer = (c_double * buffer_size)()
        elif samples_per_channel == 0:
            # only read the status
            samples_to_read = 0
            buffer_size = 0
            data_buffer = None
        elif samples_per_channel > 0:
            # read the specified number of samples
            samples_to_read = samples_per_channel
            # create a C buffer for the read
            buffer_size = samples_per_channel * num_channels
            data_buffer = (c_double * buffer_size)()
        else:
            # invalid samples_per_channel
            raise ValueError("Invalid samples_per_channel {}.".format(
                samples_per_channel))

        result = self._lib.mcc172_a_in_scan_read(
            self._address, byref(status), samples_to_read, timeout, data_buffer,
            buffer_size, byref(samples_read_per_channel))

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid parameter.")
        elif result == self._RESULT_RESOURCE_UNAVAIL:
            raise HatError(self._address, "Scan not active.")
        elif result == self._RESULT_TIMEOUT:
            timed_out = True
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response {}.".format(
                result))

        total_read = samples_read_per_channel.value * num_channels

        # python 2 / 3 workaround for xrange
        if sys.version_info.major > 2:
            data_list = [data_buffer[i] for i in range(total_read)]
        else:
            data_list = [data_buffer[i] for i in xrange(total_read)]

        scan_status = namedtuple(
            'MCC172ScanRead',
            ['running', 'hardware_overrun', 'buffer_overrun', 'triggered',
             'timeout', 'data'])
        return scan_status(
            running=(status.value & self._STATUS_RUNNING) != 0,
            hardware_overrun=(status.value & self._STATUS_HW_OVERRUN) != 0,
            buffer_overrun=(status.value & self._STATUS_BUFFER_OVERRUN) != 0,
            triggered=(status.value & self._STATUS_TRIGGERED) != 0,
            timeout=timed_out,
            data=data_list)

    def a_in_scan_read_numpy(self, samples_per_channel, timeout):
        # pylint: disable=too-many-locals
        """
        Read scan status and data (as a NumPy array).

        This function is similar to :py:func:`a_in_scan_read` except that the
        *data* key in the returned namedtuple is a NumPy array of float64 values
        and may be used directly with NumPy functions.

        Args:
            samples_per_channel (int): The number of samples per channel to read
                from the scan buffer.  Specify a negative number to read all
                available samples or 0 to only read the scan status and return
                no data.
            timeout (float): The amount of time in seconds to wait for the
                samples to be read.  Specify a negative number to wait
                indefinitely, or 0 to return immediately with the samples that
                are already in the scan buffer.  If the timeout is met and the
                specified number of samples have not been read, then the
                function will return with the amount that has been read and the
                timeout status set.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **running** (bool): True if the scan is running, False if it has
              stopped or completed.
            * **hardware_overrun** (bool): True if the hardware could not
              acquire and unload samples fast enough and data was lost.
            * **buffer_overrun** (bool): True if the background scan buffer was
              not read fast enough and data was lost.
            * **triggered** (bool): True if the trigger conditions have been met
              and data acquisition started.
            * **timeout** (bool): True if the timeout time expired before the
              specified number of samples were read.
            * **data** (NumPy array of float64): The data that was read from the
              scan buffer.

        Raises:
            HatError: A scan is not active, the board is not initialized, does
                not respond, or responds incorrectly.
            ValueError: Incorrect argument.
        """
        try:
            import numpy
            from numpy.ctypeslib import ndpointer
        except ImportError:
            raise

        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        self._lib.mcc172_a_in_scan_read.argtypes = [
            c_ubyte, POINTER(c_ushort), c_long, c_double,
            ndpointer(c_double, flags="C_CONTIGUOUS"), c_ulong,
            POINTER(c_ulong)]

        num_channels = self._lib.mcc172_a_in_scan_channel_count(self._address)
        samples_read_per_channel = c_ulong()
        status = c_ushort()
        timed_out = False
        samples_to_read = 0

        if samples_per_channel < 0:
            # read all available data

            # first, get the number of available samples
            samples_available = c_ulong(0)
            result = self._lib.mcc172_a_in_scan_status(
                self._address, byref(status), byref(samples_available))

            if result != self._RESULT_SUCCESS:
                raise HatError(self._address, "Incorrect response {}.".format(
                    result))

            # allocate a buffer large enough for all the data
            samples_to_read = samples_available.value
            buffer_size = samples_available.value * num_channels
            data_buffer = numpy.empty(buffer_size, dtype=numpy.float64)
        elif samples_per_channel == 0:
            # only read the status
            samples_to_read = 0
            buffer_size = 0
            data_buffer = None
        elif samples_per_channel > 0:
            # read the specified number of samples
            samples_to_read = samples_per_channel
            # create a C buffer for the read
            buffer_size = samples_per_channel * num_channels
            data_buffer = numpy.empty(buffer_size, dtype=numpy.float64)
        else:
            # invalid samples_per_channel
            raise ValueError("Invalid samples_per_channel {}.".format(
                samples_per_channel))

        result = self._lib.mcc172_a_in_scan_read(
            self._address, byref(status), samples_to_read, timeout, data_buffer,
            buffer_size, byref(samples_read_per_channel))

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid parameter.")
        elif result == self._RESULT_RESOURCE_UNAVAIL:
            raise HatError(self._address, "Scan not active.")
        elif result == self._RESULT_TIMEOUT:
            timed_out = True
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response {}.".format(
                result))

        total_read = samples_read_per_channel.value * num_channels

        if total_read < buffer_size:
            data_buffer = numpy.resize(data_buffer, (total_read,))

        scan_status = namedtuple(
            'MCC172ScanRead',
            ['running', 'hardware_overrun', 'buffer_overrun', 'triggered',
             'timeout', 'data'])
        return scan_status(
            running=(status.value & self._STATUS_RUNNING) != 0,
            hardware_overrun=(status.value & self._STATUS_HW_OVERRUN) != 0,
            buffer_overrun=(status.value & self._STATUS_BUFFER_OVERRUN) != 0,
            triggered=(status.value & self._STATUS_TRIGGERED) != 0,
            timeout=timed_out,
            data=data_buffer)

    def a_in_scan_channel_count(self):
        """
        Read the number of channels in the current analog input scan.

        Returns:
            int: The number of channels (0 if no scan is active, 1-2 otherwise.)

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        num_channels = self._lib.mcc172_a_in_scan_channel_count(self._address)
        return num_channels

    def a_in_scan_stop(self):
        """
        Stops an analog input scan.

        The device stops acquiring data immediately. The scan data that has been
        read into the scan buffer is available until
        :py:func:`a_in_scan_cleanup` is called.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if (self._lib.mcc172_a_in_scan_stop(self._address)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")

        return

    def a_in_scan_cleanup(self):
        """
        Free analog input scan resources after the scan is complete.

        This will free the scan buffer and other resources used by the
        background scan and make it possible to start another scan with
        :py:func:`a_in_scan_start`.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if (self._lib.mcc172_a_in_scan_cleanup(self._address)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")

        return

    def test_signals_read(self):
        """
        Read the state of shared signals for testing.

        This function reads the state of the ADC clock, sync, and trigger
        signals. Use it in conjunction with :py:func:`a_in_clock_config_write`
        and :py:func:`trigger_config` to put the signals into slave mode then
        set values on the signals using the Raspberry Pi GPIO pins. This method
        will return the values present on those signals.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **clock** (int): The current value of the clock signal (0 or 1).
            * **sync** (int): The current value of the sync signal (0 or 1).
            * **trigger** (int): The current value of the trigger signal
              (0 or 1).

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        clock = c_ubyte()
        sync = c_ubyte()
        trigger = c_ubyte()
        result = self._lib.mcc172_test_signals_read(
            self._address, byref(clock), byref(sync), byref(trigger))
        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        test_status = namedtuple(
            'MCC172TestRead',
            ['clock', 'sync', 'trigger'])
        return test_status(
            clock=clock.value,
            sync=sync.value,
            trigger=trigger.value)

    def test_signals_write(self, mode, clock, sync):
        """
        Write values to shared signals for testing.

        This function puts the shared signals into test mode and sets them to
        the specified state. The signal levels can then be read on the Raspberry
        Pi GPIO pins to confirm values. Return the device to normal mode when
        testing is complete.

        ADC conversions will not occur while in test mode. The ADCs require
        synchronization after exiting test mode, so use
        :py:func:`a_in_clock_config_write` to perform synchronization.

        Args:
            mode (int): Set to 1 for test mode or 0 for normal mode.
            clock (int): The value to write to the clock signal in test mode
                (0 or 1).
            sync (int): The value to write to the sync signal in test mode
                (0 or 1).

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        result = self._lib.mcc172_test_signals_write(
            self._address, mode, clock, sync)
        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return
