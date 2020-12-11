"""
Wraps all of the methods from the MCC 134 library for use in Python.
"""
from collections import namedtuple
from ctypes import c_ubyte, c_char_p, c_int, c_double, byref, POINTER, \
    create_string_buffer
from enum import IntEnum, unique
from daqhats.hats import Hat, HatError, OptionFlags

@unique
class TcTypes(IntEnum):
    """Thermocouple types."""
    TYPE_J = 0      #: Type J
    TYPE_K = 1      #: Type K
    TYPE_T = 2      #: Type T
    TYPE_E = 3      #: Type E
    TYPE_R = 4      #: Type R
    TYPE_S = 5      #: Type S
    TYPE_B = 6      #: Type B
    TYPE_N = 7      #: Type N
    DISABLED = 255  #: Disabled

class mcc134(Hat): # pylint: disable=invalid-name
    """
    The class for an MCC 134 board.

    Args:
        address (int): board address, must be 0-7.

    Raises:
        HatError: the board did not respond or was of an incorrect type
    """

    #: Return value for an open thermocouple.
    OPEN_TC_VALUE = -9999.0
    #: Return value for thermocouple voltage outside the valid range.
    OVERRANGE_TC_VALUE = -8888.0
    #: Return value for thermocouple input outside the common-mode range.
    COMMON_MODE_TC_VALUE = -7777.0

    _AIN_NUM_CHANNELS = 4           # Number of analog channels

    _dev_info_type = namedtuple(
        'MCC134DeviceInfo', [
            'NUM_AI_CHANNELS', 'AI_MIN_CODE', 'AI_MAX_CODE',
            'AI_MIN_VOLTAGE', 'AI_MAX_VOLTAGE', 'AI_MIN_RANGE',
            'AI_MAX_RANGE'])

    _dev_info = _dev_info_type(
        NUM_AI_CHANNELS=_AIN_NUM_CHANNELS,
        AI_MIN_CODE=-8388608,
        AI_MAX_CODE=8388607,
        AI_MIN_VOLTAGE=-0.078125,
        AI_MAX_VOLTAGE=(0.078125 - (0.15625/16777216)),
        AI_MIN_RANGE=-0.078125,
        AI_MAX_RANGE=+0.078125)

    def __init__(self, address=0):
        """
        Initialize the class.
        """
        # call base class initializer
        Hat.__init__(self, address)

        self.callback = None

        # set up library argtypes and restypes
        self._lib.mcc134_open.argtypes = [c_ubyte]
        self._lib.mcc134_open.restype = c_int

        self._lib.mcc134_close.argtypes = [c_ubyte]
        self._lib.mcc134_close.restype = c_int

        self._lib.mcc134_serial.argtypes = [c_ubyte, c_char_p]
        self._lib.mcc134_serial.restype = c_int

        self._lib.mcc134_calibration_date.argtypes = [c_ubyte, c_char_p]
        self._lib.mcc134_calibration_date.restype = c_int

        self._lib.mcc134_calibration_coefficient_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_double), POINTER(c_double)]
        self._lib.mcc134_calibration_coefficient_read.restype = c_int

        self._lib.mcc134_calibration_coefficient_write.argtypes = [
            c_ubyte, c_ubyte, c_double, c_double]
        self._lib.mcc134_calibration_coefficient_write.restype = c_int

        self._lib.mcc134_tc_type_write.argtypes = [c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc134_tc_type_write.restype = c_int

        self._lib.mcc134_tc_type_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc134_tc_type_read.restype = c_int

        self._lib.mcc134_update_interval_write.argtypes = [c_ubyte, c_ubyte]
        self._lib.mcc134_update_interval_write.restype = c_int

        self._lib.mcc134_update_interval_read.argtypes = [
            c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc134_update_interval_read.restype = c_int

        self._lib.mcc134_t_in_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_double)]
        self._lib.mcc134_t_in_read.restype = c_int

        self._lib.mcc134_a_in_read.argtypes = [
            c_ubyte, c_ubyte, c_int, POINTER(c_double)]
        self._lib.mcc134_a_in_read.restype = c_int

        self._lib.mcc134_cjc_read.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_double)]
        self._lib.mcc134_cjc_read.restype = c_int

        result = self._lib.mcc134_open(self._address)

        if result == self._RESULT_SUCCESS:
            self._initialized = True
        elif result == self._RESULT_INVALID_DEVICE:
            raise HatError(self._address, "Invalid board type.")
        else:
            raise HatError(self._address, "Board not responding.")

        return

    def __del__(self):
        if self._initialized:
            self._lib.mcc134_close(self._address)
        return

    @staticmethod
    def info():
        """
        Return constant information about this type of device.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **NUM_AI_CHANNELS** (int): The number of analog input channels
              (4.)
            * **AI_MIN_CODE** (int): The minimum ADC code (-8,388,608.)
            * **AI_MAX_CODE** (int): The maximum ADC code (8,388,607.)
            * **AI_MIN_VOLTAGE** (float): The voltage corresponding to the
              minimum ADC code (-0.078125.)
            * **AI_MAX_VOLTAGE** (float): The voltage corresponding to the
              maximum ADC code (+0.078125 - 1 LSB)
            * **AI_MIN_RANGE** (float): The minimum voltage of the input range
              (-0.078125.)
            * **AI_MAX_RANGE** (float): The maximum voltage of the input range
              (+0.078125.)
        """
        return mcc134._dev_info

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
        if (self._lib.mcc134_serial(self._address, my_buffer)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        my_serial = my_buffer.value.decode('ascii')
        return my_serial

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
        if (self._lib.mcc134_calibration_date(self._address, my_buffer)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        my_date = my_buffer.value.decode('ascii')
        return my_date

    def calibration_coefficient_read(self, channel):
        """
        Read the calibration coefficients for a single channel.

        The coefficients are applied in the library as: ::

            calibrated_ADC_code = (raw_ADC_code * slope) + offset

        Args:
            channel (int): The thermocouple channel (0-3.)

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
        if (self._lib.mcc134_calibration_coefficient_read(
                self._address, channel, byref(slope), byref(offset))
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        cal_info = namedtuple('MCC134CalInfo', ['slope', 'offset'])
        return cal_info(
            slope=slope.value,
            offset=offset.value)

    def calibration_coefficient_write(self, channel, slope, offset):
        """
        Temporarily write the calibration coefficients for a single channel.

        The user can apply their own calibration coefficients by writing to
        these values. The values will reset to the factory values from the
        EEPROM whenever the class is initialized.

        The coefficients are applied in the library as: ::

            calibrated_ADC_code = (raw_ADC_code * slope) + offset

        Args:
            channel (int): The thermocouple channel (0-3.)
            slope (float): The new slope value.
            offset (float): The new offset value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        if (self._lib.mcc134_calibration_coefficient_write(
                self._address, channel, slope, offset) != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return

    def tc_type_write(self, channel, tc_type):
        """
        Write the thermocouple type for a channel.

        Enables a channel and tells the library what thermocouple type is
        connected to the channel. This is needed for correct temperature
        calculations. The type is one of :py:class:`TcTypes` and the board will
        default to all channels disabled (set to :py:const:`TcTypes.DISABLED`)
        when it is first opened.

        Args:
            channel (int): The analog input channel number, 0-3.
            tc_type (:py:class:`TcTypes`): The thermocouple type.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        if (self._lib.mcc134_tc_type_write(self._address, channel, tc_type)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return

    def tc_type_read(self, channel):
        """
        Read the thermocouple type for a channel.

        Reads the current thermocouple type for the specified channel. The type
        is one of :py:class:`TcTypes` and the board will default to all channels
        disable (set to :py:const:`TcTypes.DISABLED`) when it is first opened.

        Args:
            channel (int): The analog input channel number, 0-3.

        Returns
            int: The thermocouple type.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        type_value = c_ubyte()
        if (self._lib.mcc134_tc_type_read(
                self._address, channel, byref(type_value))
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return type_value.value

    def update_interval_write(self, interval):
        """
        Write the temperature update interval.

        Tells the MCC 134 library how often to update temperatures, with the
        interval specified in seconds.  The library defaults to updating every
        second, but you may increase this interval if you do not plan to call
        :py:func:`t_in_read` very often. This will reduce the load on shared
        resources for other DAQ HATs.

        Args:
            interval (int): The interval in seconds, 1 - 255.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")
        if (self._lib.mcc134_update_interval_write(self._address, interval)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return


    def update_interval_read(self):
        """
        Read the temperature update interval.

        Reads the library temperature update rate in seconds.

        Returns
            int: The update interval.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        interval = c_ubyte()
        if (self._lib.mcc134_update_interval_read(
                self._address, byref(interval)) != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return interval.value

    def t_in_read(self, channel):
        """
        Read a thermocouple input channel temperature.

        The channel must be enabled with :py:func:`tc_type_write` or the
        method will raise a ValueError exception.

        This method returns immediately with the most recent temperature reading
        for the specified channel. When a board is open, the library will read
        each channel approximately once per second. There will be a delay when
        the board is first opened because the read thread has to read the cold
        junction compensation sensors and thermocouple inputs before it can
        return the first value.

        The method returns the value as degrees Celsius. The temperature value
        can have some special values for abnormal conditions:

            - :py:const:`mcc134.OPEN_TC_VALUE` if an open thermocouple is
              detected.
            - :py:const:`mcc134.OVERRANGE_TC_VALUE` if a value outside valid
              thermocouple voltage is detected.
            - :py:const:`mcc134.COMMON_MODE_TC_VALUE` if a common-mode voltage
              error is detected. This occurs when thermocouples on the same MCC
              134 are at different voltages.

        Args:
            channel (int): The analog input channel number, 0-3.

        Returns:
            float: The thermocouple temperature.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: the channel number is invalid or the channel is
                disabled.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._AIN_NUM_CHANNELS):
            raise ValueError("Invalid channel {0}. Must be 0-{1}.".
                             format(channel, self._AIN_NUM_CHANNELS-1))

        temp = c_double()

        result = self._lib.mcc134_t_in_read(self._address, channel, byref(temp))
        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid channel {}, the channel must be enabled.".
                             format(channel))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return temp.value

    def a_in_read(self, channel, options=OptionFlags.DEFAULT):
        """
        Read an analog input channel and return the value.

        The channel must be enabled with :py:func:`tc_type_write` or the
        method will raise a ValueError exception.

        The returned voltage can have a special value to indicate abnormal
        conditions:

        * :py:const:`mcc134.COMMON_MODE_TC_VALUE` if a common-mode voltage
          error is detected. This occurs when thermocouples on the same MCC
          134 are at different voltages.

        **options** is an ORed combination of OptionFlags. Valid flags for this
        method are:

        * :py:const:`OptionFlags.DEFAULT`: Return a calibrated voltage value.
          Any other flags will override DEFAULT behavior.
        * :py:const:`OptionFlags.NOSCALEDATA`: Return an ADC code (a value
          between -8,388,608 and 8,388,607) rather than voltage.
        * :py:const:`OptionFlags.NOCALIBRATEDATA`: Return data without the
          calibration factors applied.

        Args:
            channel (int): The analog input channel number, 0-3.
            options (int): ORed combination of :py:class:`OptionFlags`,
                :py:const:`OptionFlags.DEFAULT` if unspecified.

        Returns:
            float: The read value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: the channel number is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._AIN_NUM_CHANNELS):
            raise ValueError("Invalid channel {0}. Must be 0-{1}.".
                             format(channel, self._AIN_NUM_CHANNELS-1))

        data_value = c_double()

        result = self._lib.mcc134_a_in_read(self._address, channel, options,
                                            byref(data_value))
        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid channel {}, the channel must be enabled.".
                             format(channel))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return data_value.value

    def cjc_read(self, channel):
        """
        Read the cold junction compensation temperature for a specified channel.

        Reads the cold junction sensor temperature for the specified
        thermocouple terminal. The library automatically performs cold junction
        compensation, so this function is only needed for informational use or
        if you want to perform your own compensation. The temperature is
        returned in degress C.

        Args:
            channel (int): The analog input channel number, 0-3.

        Returns:
            float: The read value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: the channel number is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._AIN_NUM_CHANNELS):
            raise ValueError("Invalid channel {0}. Must be 0-{1}.".
                             format(channel, self._AIN_NUM_CHANNELS-1))

        data_value = c_double()

        if (self._lib.mcc134_cjc_read(self._address, channel, byref(data_value))
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        return data_value.value
