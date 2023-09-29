# pylint: disable=too-many-lines
"""
Wraps all of the methods from the MCC 152 library for use in Python.
"""
from collections import namedtuple
from ctypes import c_ubyte, c_int, c_char_p, c_ulong, c_double, POINTER, \
    create_string_buffer, byref
from enum import IntEnum, unique
from daqhats.hats import Hat, HatError, OptionFlags

@unique
class DIOConfigItem(IntEnum):
    """Digital I/O Configuration Items."""
    DIRECTION = 0       #: Configure channel direction
    PULL_CONFIG = 1     #: Configure pull-up/down resistor
    PULL_ENABLE = 2     #: Enable pull-up/down resistor
    INPUT_INVERT = 3    #: Configure input inversion
    INPUT_LATCH = 4     #: Configure input latching
    OUTPUT_TYPE = 5     #: Configure output type
    INT_MASK = 6        #: Configure interrupt mask

class mcc152(Hat): # pylint: disable=invalid-name,too-many-public-methods
    """
    The class for an MCC 152 board.

    Args:
        address (int): board address, must be 0-7.

    Raises:
        HatError: the board did not respond or was of an incorrect type
    """

    _AOUT_NUM_CHANNELS = 2         # Number of analog output channels
    _DIO_NUM_CHANNELS = 8          # Number of digital I/O channels

    _MIN_CODE = 0.0
    _MAX_CODE = 4095.0

    _MAX_RANGE = 5.0
    _MIN_VOLTAGE = 0.0
    _MAX_VOLTAGE = (_MAX_RANGE * (_MAX_CODE / (_MAX_CODE + 1)))

    _dev_info_type = namedtuple(
        'MCC152DeviceInfo', [
            'NUM_DIO_CHANNELS', 'NUM_AO_CHANNELS', 'AO_MIN_CODE',
            'AO_MAX_CODE', 'AO_MIN_VOLTAGE', 'AO_MAX_VOLTAGE',
            'AO_MIN_RANGE', 'AO_MAX_RANGE'])

    _dev_info = _dev_info_type(
        NUM_DIO_CHANNELS=8,
        NUM_AO_CHANNELS=2,
        AO_MIN_CODE=0,
        AO_MAX_CODE=4095,
        AO_MIN_VOLTAGE=0.0,
        AO_MAX_VOLTAGE=(5.0 - (5.0/4096)),
        AO_MIN_RANGE=0.0,
        AO_MAX_RANGE=5.0)

    def __init__(self, address=0): # pylint: disable=similarities
        """
        Initialize the class.
        """
        # call base class initializer
        Hat.__init__(self, address)

        # set up library argtypes and restypes
        self._lib.mcc152_open.argtypes = [c_ubyte]
        self._lib.mcc152_open.restype = c_int

        self._lib.mcc152_close.argtypes = [c_ubyte]
        self._lib.mcc152_close.restype = c_int

        self._lib.mcc152_serial.argtypes = [c_ubyte, c_char_p]
        self._lib.mcc152_serial.restype = c_int

        self._lib.mcc152_a_out_write.argtypes = [
            c_ubyte, c_ubyte, c_ulong, c_double]
        self._lib.mcc152_a_out_write.restype = c_int

        self._lib.mcc152_a_out_write_all.argtypes = [
            c_ubyte, c_ulong, POINTER(c_double)]
        self._lib.mcc152_a_out_write_all.restype = c_int

        self._lib.mcc152_dio_reset.argtypes = [c_ubyte]
        self._lib.mcc152_dio_reset.restype = c_int

        self._lib.mcc152_dio_input_read_bit.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_input_read_bit.restype = c_int

        self._lib.mcc152_dio_input_read_port.argtypes = [
            c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_input_read_port.restype = c_int

        self._lib.mcc152_dio_output_write_bit.argtypes = [
            c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc152_dio_output_write_bit.restype = c_int

        self._lib.mcc152_dio_output_write_port.argtypes = [c_ubyte, c_ubyte]
        self._lib.mcc152_dio_output_write_port.restype = c_int

        self._lib.mcc152_dio_output_read_bit.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_output_read_bit.restype = c_int

        self._lib.mcc152_dio_output_read_port.argtypes = [
            c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_output_read_port.restype = c_int

        self._lib.mcc152_dio_int_status_read_bit.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_int_status_read_bit.restype = c_int

        self._lib.mcc152_dio_int_status_read_port.argtypes = [
            c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_int_status_read_port.restype = c_int

        self._lib.mcc152_dio_config_write_bit.argtypes = [
            c_ubyte, c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc152_dio_config_write_bit.restype = c_int

        self._lib.mcc152_dio_config_write_port.argtypes = [
            c_ubyte, c_ubyte, c_ubyte]
        self._lib.mcc152_dio_config_write_port.restype = c_int

        self._lib.mcc152_dio_config_read_bit.argtypes = [
            c_ubyte, c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_config_read_bit.restype = c_int

        self._lib.mcc152_dio_config_read_port.argtypes = [
            c_ubyte, c_ubyte, POINTER(c_ubyte)]
        self._lib.mcc152_dio_config_read_port.restype = c_int

        result = self._lib.mcc152_open(self._address)

        if result == self._RESULT_SUCCESS:
            self._initialized = True
        elif result == self._RESULT_INVALID_DEVICE:
            raise HatError(self._address, "Invalid board type.")
        else:
            raise HatError(self._address, "Board not responding.")

        return

    def __del__(self):
        if self._initialized:
            self._lib.mcc152_close(self._address)
        return

    @staticmethod
    def info():
        """
        Return constant information about this type of device.

        Returns:
            namedtuple: A namedtuple containing the following field names:

            * **NUM_DIO_CHANNELS** (int): The number of digital I/O channels
              (8.)
            * **NUM_AO_CHANNELS** (int): The number of analog output channels
              (2.)
            * **AO_MIN_CODE** (int): The minimum DAC code (0.)
            * **AO_MAX_CODE** (int): The maximum DAC code (4095.)
            * **AO_MIN_VOLTAGE** (float): The voltage corresponding to the
              minimum DAC code (0.0.)
            * **AO_MAX_VOLTAGE** (float): The voltage corresponding to the
              maximum DAC code (+5.0 - 1 LSB)
            * **AO_MIN_RANGE** (float): The minimum voltage of the output range
              (0.0.)
            * **AO_MAX_RANGE** (float): The maximum voltage of the output range
              (+5.0.)
        """
        return mcc152._dev_info

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
        if (self._lib.mcc152_serial(self._address, my_buffer)
                != self._RESULT_SUCCESS):
            raise HatError(self._address, "Incorrect response.")
        my_serial = my_buffer.value.decode('ascii')
        return my_serial

    def a_out_write(self, channel, value, options=OptionFlags.DEFAULT):
        """
        Write a single analog output channel value. The value will be limited to
        the range of the DAC without raising an exception.

        **options** is an OptionFlags value. Valid flags for this
        method are:

        * :py:const:`OptionFlags.DEFAULT`: Write a voltage value (0 - 5).
        * :py:const:`OptionFlags.NOSCALEDATA`: Write a DAC code (a value
          between 0 and 4095) rather than voltage.

        Args:
            channel (int): The analog output channel number, 0-1.
            value (float): The value to write.
            options (int): An :py:class:`OptionFlags` value,
                :py:const:`OptionFlags.DEFAULT` if unspecified.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._AOUT_NUM_CHANNELS):
            raise ValueError("Invalid channel {0}. Must be 0-{1}.".
                             format(channel, self._AOUT_NUM_CHANNELS-1))

        if (options & OptionFlags.NOSCALEDATA) != 0:
            if value < self._MIN_CODE:
                value = self._MIN_CODE
            elif value > self._MAX_CODE:
                value = self._MAX_CODE
        else:
            if value < self._MIN_VOLTAGE:
                value = self._MIN_VOLTAGE
            elif value > self._MAX_VOLTAGE:
                value = self._MAX_VOLTAGE

        result = self._lib.mcc152_a_out_write(
            self._address, channel, options, value)

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return

    def a_out_write_all(self, values, options=OptionFlags.DEFAULT):
        """
        Write all analog output channels simultaneously.

        **options** is an OptionFlags value. Valid flags for this
        method are:

        * :py:const:`OptionFlags.DEFAULT`: Write voltage values (0 - 5).
        * :py:const:`OptionFlags.NOSCALEDATA`: Write DAC codes (values
          between 0 and 4095) rather than voltage.

        Args:
            values (list of float): The values to write, in channel order. There
                must be at least two values, but only the first two will be
                used.
            options (int): An :py:class:`OptionFlags` value,
                :py:const:`OptionFlags.DEFAULT` if unspecified.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if len(values) < self._AOUT_NUM_CHANNELS:
            raise ValueError(
                "Not enough elements in values. Must be at least {}".
                format(self._AOUT_NUM_CHANNELS))

        for value in values:
            if (options & OptionFlags.NOSCALEDATA) != 0:
                if value < self._MIN_CODE:
                    value = self._MIN_CODE
                elif value > self._MAX_CODE:
                    value = self._MAX_CODE
            else:
                if value < self._MIN_VOLTAGE:
                    value = self._MIN_VOLTAGE
                elif value > self._MAX_VOLTAGE:
                    value = self._MAX_VOLTAGE

        data_array = (c_double * len(values))(*values)
        result = self._lib.mcc152_a_out_write_all(
            self._address, options, data_array)

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid value in {}.".format(values))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")
        return

    def dio_reset(self):
        """
        Reset the DIO to the default configuration.

        * All channels input
        * Output registers set to 1
        * Input inversion disabled
        * No input latching
        * Pull-up resistors enabled
        * All interrupts disabled
        * Push-pull output type

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if self._lib.mcc152_dio_reset(self._address) != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_input_read_bit(self, channel):
        """
        Read a single digital input channel.

        Returns 0 if the input is low, 1 if it is high.

        If the specified channel is configured as an output this will return the
        value present at the terminal.

        This method reads the entire input register even though a single channel
        is specified, so care must be taken when latched inputs are enabled. If
        a latched input changes between input reads then changes back to its
        original value, the next input read will report the change to the first
        value then the following read will show the original value. If another
        input is read then this input change could be missed so it is best to
        use :py:func:`dio_input_read_port` or :py:func:`dio_input_read_tuple`
        when using latched inputs.

        Args:
            channel (int): The DIO channel number, 0-7.

        Returns:
            int: The input value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._DIO_NUM_CHANNELS):
            raise ValueError("Invalid channel {}.".format(channel))

        value = c_ubyte()
        result = self._lib.mcc152_dio_input_read_bit(
            self._address, channel, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_input_read_port(self):
        """
        Read all digital input channels.

        Returns the values as an integer with a value of 0 - 255. Each channel
        is represented by a bit in the integer (bit 0 is channel 0, etc.)

        The value of a specific input can be found by examining the bit at that
        location. For example, to act on the channel 3 input: ::

            inputs = mcc152.dio_input_read_port()
            if (inputs & (1 << 3)) == 0:
                print("channel 3 is 0")
            else:
                print("channel 3 is 1")

        If a channel is configured as an output this will return the value
        present at the terminal.

        Care must be taken when latched inputs are enabled. If a latched input
        changes between input reads then changes back to its original value, the
        next input read will report the change to the first value then the
        following read will show the original value.

        Returns
            int: The input values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_input_read_port(
            self._address, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_input_read_tuple(self):
        """
        Read all digital inputs at once as a tuple.

        Returns a tuple of all input values in channel order. For example, to
        compare the channel 1 input to the channel 3 input: ::

            inputs = mcc152.dio_input_read_tuple()
            if inputs[1] == inputs[3]:
                print("channel 1 and channel 3 inputs are the same")

        If a channel is configured as an output this will return the value
        present at the terminal.

        Care must be taken when latched inputs are enabled. If a latched input
        changes between input reads then changes back to its original value, the
        next input read will report the change to the first value then the
        following read will show the original value.

        Returns
            tuple of int: The input values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_input_read_port(
            self._address, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        # convert byte to tuple of bits
        reg = value.value
        mytuple = tuple(((reg >> i) & 0x01) for i in range(
            self._DIO_NUM_CHANNELS))

        return mytuple

    def dio_output_write_bit(self, channel, value):
        """
        Write a single digital output channel.

        If the specified channel is configured as an input this will not have
        any effect at the terminal, but allows the output register to be loaded
        before configuring the channel as an output.

        Args:
            channel (int): The digital channel number, 0-7.
            value (int): The output value, 0 or 1.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._DIO_NUM_CHANNELS):
            raise ValueError("Invalid channel {}.".format(channel))

        if value not in (0, 1):
            raise ValueError(
                "Invalid value {}, expecting 0 or 1.".format(value))

        result = self._lib.mcc152_dio_output_write_bit(
            self._address, channel, value)

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_output_write_port(self, values):
        """
        Write all digital output channel values.

        Pass an integer in the range of 0 - 255, where each bit represents the
        value of the associated channel (bit 0 is channel 0, etc.) If a
        specified channel is configured as an input this will not have any
        effect at the terminal, but allows the output register to be loaded
        before configuring the channel as an output.

        To change specific outputs without affecting other outputs first read
        the output values with dio_output_read_port(), change the desired bits
        in the result, then write them back.

        For example, to set channels 0 and 2 to 1 without affecting the other
        outputs: ::

            values = mcc152.dio_output_read_port()
            values |= 0x05
            mcc152.dio_output_write_port(values)

        Args:
            values (integer): The output values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if values not in range(256):
            raise ValueError("Invalid values {}.".format(values))

        result = self._lib.mcc152_dio_output_write_port(
            self._address, values)

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_output_write_dict(self, value_dict):
        """
        Write multiple digital output channel values.

        Pass a dictionary containing channel:value pairs. If a channel is
        repeated in the dictionary then the last value will be used. If a
        specified channel is configured as an input this will not have any
        effect at the terminal, but allows the output register to be loaded
        before configuring the channel as an output.

        For example, to set channels 0 and 2 to 1 without affecting the other
        outputs: ::

            values = { 0:1, 2:1 }
            mcc152.dio_output_write_dict(values)

        Args:
            value_dict (dictionary): The output values in a dictionary of
                channel:value pairs.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        orig_val = c_ubyte()
        result = self._lib.mcc152_dio_output_read_port(
            self._address, byref(orig_val))
        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        values = orig_val.value
        channels = 0

        for channel, value in value_dict.items():
            if channel not in range(self._DIO_NUM_CHANNELS):
                raise ValueError("Invalid channel {}.".format(channel))
            if value not in (0, 1):
                raise ValueError(
                    "Invalid value {}, expecting 0 or 1.".format(value))
            mask = 1 << channel
            channels = channels | mask
            if value == 0:
                values = values & ~mask
            else:
                values = values | mask

        if channels == 0:
            raise ValueError("No channels specified.")

        result = self._lib.mcc152_dio_output_write_port(
            self._address, values)

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_output_read_bit(self, channel):
        """
        Read a single digital output channel value.

        This function returns the value stored in the output register. It may
        not represent the value at the terminal if the channel is configured as
        input or open-drain output.

        Args:
            channel (int): The digital channel number, 0-7.

        Returns:
            int: The output value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._DIO_NUM_CHANNELS):
            raise ValueError("Invalid channel {}.".format(channel))

        value = c_ubyte()
        result = self._lib.mcc152_dio_output_read_bit(
            self._address, channel, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_output_read_port(self):
        """
        Read all digital output channel values.

        Returns the values as an integer with a value of 0 - 255. Each channel
        is represented by a bit in the integer (bit 0 is channel 0, etc.) The
        value of a specific output can be found by examining the bit at that
        location.

        This function returns the values stored in the output register. They may
        not represent the value at the terminal if the channel is configured as
        input or open-drain output.

        Returns
            int: The output values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_output_read_port(
            self._address, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_output_read_tuple(self):
        """
        Read all digital output channel values at once as a tuple.

        Returns a tuple of all output values in channel order. For example, to
        compare the channel 1 output to the channel 3 output: ::

            outputs = mcc152.dio_output_read_tuple()
            if outputs[1] == outputs[3]:
                print("channel 1 and channel 3 outputs are the same")

        This function returns the values stored in the output register. They may
        not represent the value at the terminal if the channel is configured as
        input or open-drain output.

        Returns
            tuple of int: The output values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_output_read_port(
            self._address, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        # convert byte to tuple of bits
        reg = value.value
        mytuple = tuple(((reg >> i) & 0x01) for i in range(
            self._DIO_NUM_CHANNELS))

        return mytuple

    def dio_int_status_read_bit(self, channel):
        """
        Read the interrupt status for a single channel.

        Returns 0 if the input is not generating an interrupt, 1 if it is
        generating an interrupt.

        Returns
            int: The interrupt status value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._DIO_NUM_CHANNELS):
            raise ValueError("Invalid channel {}.".format(channel))

        value = c_ubyte()
        result = self._lib.mcc152_dio_int_status_read_bit(
            self._address, channel, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_int_status_read_port(self):
        """
        Read the interrupt status for all channels.

        Returns the values as an integer with a value of 0 - 255. Each channel
        is represented by a bit in the integer (bit 0 is channel 0, etc.) The
        status for a specific input can be found by examining the bit at that
        location. Each bit will be 0 if the channel is not generating an
        interrupt or 1 if it is generating an interrupt.

        Returns
            int: The interrupt status values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_int_status_read_port(
            self._address, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_int_status_read_tuple(self):
        """
        Read the interrupt status for all channels as a tuple.

        Returns a tuple of all interrupt status values in channel order. Each
        value will be 0 if the channel is not generating an interrupt or 1 if it
        is generating an interrupt. For example, to see if an interrupt has
        occurred on channel 2 or 4: ::

            status = mcc152.dio_int_status_read_tuple()
            if status[2] == 1 or status[4] == 1:
                print("an interrupt has occurred on channel 2 or 4")

        Returns
            tuple of int: The status values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_int_status_read_port(
            self._address, byref(value))

        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        # convert byte to tuple of bits
        reg = value.value
        mytuple = tuple(((reg >> i) & 0x01) for i in range(
            self._DIO_NUM_CHANNELS))

        return mytuple

    def dio_config_write_bit(self, channel, item, value):
        """
        Write a digital I/O configuration value for a single channel.

        There are several configuration items that may be written for the
        digital I/O. The item is selected with the **item** argument, which may
        be one of the :py:class:`DIOConfigItem` values:

        * :py:const:`DIOConfigItem.DIRECTION`: Set the digital I/O channel
          direction by passing 0 for output and 1 for input.
        * :py:const:`DIOConfigItem.PULL_CONFIG`: Configure the pull-up/down
          resistor by passing 0 for pull-down or 1 for pull-up. The resistor may
          be enabled or disabled with the :py:const:`DIOConfigItem.PULL_ENABLE`
          item.
        * :py:const:`DIOConfigItem.PULL_ENABLE`: Enable or disable the
          pull-up/down resistor by passing 0 for disabled or 1 for enabled. The
          resistor is configured for pull-up/down with the
          :py:const:`DIOConfigItem.PULL_CONFIG` item. The resistor is
          automatically disabled if the bit is set to output and is configured
          as open-drain.
        * :py:const:`DIOConfigItem.INPUT_INVERT`: Enable inverting the input by
          passing a 0 for normal input or 1 for inverted.
        * :py:const:`DIOConfigItem.INPUT_LATCH`: Enable input latching by
          passing 0 for non-latched or 1 for latched.

          When the input is non-latched, reads show the current status of the
          input. A state change in the input generates an interrupt (if it is
          not masked). A read of the input clears the interrupt. If the input
          goes back to its initial logic state before the input is read, then
          the interrupt is cleared.

          When the input is latched, a change of state of the input generates an
          interrupt and the input logic value is loaded into the input port
          register. A read of the input will clear the interrupt. If the input
          returns to its initial logic state before the input is read, then the
          interrupt is not cleared and the input register keeps the logic value
          that initiated the interrupt. The next read of the input will show the
          initial state. Care must be taken when using bit reads on the input
          when latching is enabled - the bit method still reads the entire input
          register so a change on another bit could be missed. It is best to use
          port or tuple input reads when using latching.

          If the input is changed from latched to non-latched, a read from the
          input reflects the current terminal logic level. If the input is
          changed from non-latched to latched input, the read from the input
          represents the latched logic level.
        * :py:const:`DIOConfigItem.OUTPUT_TYPE`: Set the output type by writing
          0 for push-pull or 1 for open-drain. This setting affects all outputs
          so is not a per-channel setting and the channel argument is ignored.
          It should be set to the desired type before using
          :py:const:`DIOConfigItem.DIRECTION` item to set channels as outputs.
          Internal pull-up/down resistors are disabled when a bit is set to
          output and is configured as open-drain, so external resistors should
          be used.
        * :py:const:`DIOConfigItem.INT_MASK`: Enable or disable interrupt
          generation by masking the interrupt. Write 0 to enable the interrupt
          or 1 to mask (disable) it.

          All MCC 152s share a single interrupt signal to the CPU, so when an
          interrupt occurs the user must determine the source, optionally act
          on the interrupt, then clear that source so that other interrupts
          may be detected. The current interrupt state may be read with
          :py:func:`interrupt_state`. A user program may wait for the interrupt
          to become active with :py:func:`wait_for_interrupt`, or may register
          an interrupt callback function with
          :py:func:`interrupt_callback_enable`. This allows the user to wait for
          a change on one or more inputs without constantly reading the inputs.
          The source of the interrupt may be determined by reading the interrupt
          status of each MCC 152 with :py:func:`dio_int_status_read_bit`,
          :py:func:`dio_int_status_read_port` or
          :py:func:`dio_int_status_read_tuple`, and all active interrupt
          sources must be cleared before the interrupt will become inactive. The
          interrupt is cleared by reading the input with
          :py:func:`dio_input_read_bit`, :py:func:`dio_input_read_port`, or
          :py:func:`dio_input_read_tuple`.

        Args:
            channel (integer): The digital I/O channel, 0 - 7
            item (integer): The configuration item, one of
                :py:class:`DIOConfigItem`.
            value (integer): The configuration value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._DIO_NUM_CHANNELS):
            raise ValueError("Invalid channel {}.".format(channel))

        if value not in (0, 1):
            raise ValueError(
                "Invalid value {}, expecting 0 or 1.".format(value))

        result = self._lib.mcc152_dio_config_write_bit(
            self._address, channel, item, value)

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid item {}.".format(item))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_config_write_port(self, item, value):
        """
        Write a digital I/O configuration value for all channels.

        There are several configuration items that may be written for the
        digital I/O. They are written for all channels at once using an 8-bit
        value passed in **value**, where each bit corresponds to a channel (bit
        0 is channel 0, etc.) The item is selected with the **item** argument,
        which may be one of the :py:class:`DIOConfigItem` values.

        * :py:const:`DIOConfigItem.DIRECTION`: Set the digital I/O channel
          directions by passing 0 in a bit for output and 1 for input.
        * :py:const:`DIOConfigItem.PULL_CONFIG`: Configure the pull-up/down
          resistors by passing 0 in a bit for pull-down or 1 for pull-up. The
          resistors may be enabled or disabled with the
          :py:const:`DIOConfigItem.PULL_ENABLE` item.
        * :py:const:`DIOConfigItem.PULL_ENABLE`: Enable or disable pull-up/down
          resistors by passing 0 in a bit for disabled or 1 for enabled. The
          resistors are configured for pull-up/down with the
          :py:const:`DIOConfigItem.PULL_CONFIG` item. The resistors are
          automatically disabled if the bits are set to output and configured as
          open-drain.
        * :py:const:`DIOConfigItem.INPUT_INVERT`: Enable inverting inputs by
          passing a 0 in a bit for normal input or 1 for inverted.
        * :py:const:`DIOConfigItem.INPUT_LATCH`: Enable input latching by
          passing 0 in a bit for non-latched or 1 for latched.

          When the input is non-latched, reads show the current status of the
          input. A state change in the corresponding input generates an
          interrupt (if it is not masked). A read of the input clears the
          interrupt. If the input goes back to its initial logic state before
          the input is read, then the interrupt is cleared. When the input is
          latched, a change of state of the input generates an interrupt and the
          input logic value is loaded into the input port register. A read of
          the input will clear the interrupt. If the input returns to its
          initial logic state before the input is read, then the interrupt is
          not cleared and the input register keeps the logic value that
          initiated the interrupt. The next read of the input will show the
          initial state. Care must be taken when using bit reads on the input
          when latching is enabled - the bit method still reads the entire input
          register so a change on another bit could be missed. It is best to use
          port or tuple input reads when using latching.

          If the input is changed from latched to non-latched, a read from the
          input reflects the current terminal logic level. If the input is
          changed from non-latched to latched input, the read from the input
          represents the latched logic level.
        * :py:const:`DIOConfigItem.OUTPUT_TYPE`: Set the output type by writing
          0 for push-pull or 1 for open-drain. This setting affects all outputs
          so is not a per-channel setting. It should be set to the desired type
          before using :py:const:`DIOConfigItem.DIRECTION` to set channels as
          outputs. Internal pull-up/down resistors are disabled when a bit is
          set to output and is configured as open-drain, so external resistors
          should be used.
        * :py:const:`DIOConfigItem.INT_MASK`: Enable or disable interrupt
          generation for specific inputs by masking the interrupt. Write 0 in a
          bit to enable the interrupt from that channel or 1 to mask (disable)
          it.

          All MCC 152s share a single interrupt signal to the CPU, so when an
          interrupt occurs the user must determine the source, optionally act
          on the interrupt, then clear that source so that other interrupts
          may be detected. The current interrupt state may be read with
          :py:func:`interrupt_state`. A user program may wait for the interrupt
          to become active with :py:func:`wait_for_interrupt`, or may register
          an interrupt callback function with
          :py:func:`interrupt_callback_enable`. This allows the user to wait for
          a change on one or more inputs without constantly reading the inputs.
          The source of the interrupt may be determined by reading the interrupt
          status of each MCC 152 with :py:func:`dio_int_status_read_bit`,
          :py:func:`dio_int_status_read_port` or
          :py:func:`dio_int_status_read_tuple`, and all active interrupt
          sources must be cleared before the interrupt will become inactive. The
          interrupt is cleared by reading the input with
          :py:func:`dio_input_read_bit`, :py:func:`dio_input_read_port`, or
          :py:func:`dio_input_read_tuple`.

        Args:
            item (integer): The configuration item, one of
                :py:class:`DIOConfigItem`.
            value (integer): The configuration value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if value not in range(256):
            raise ValueError("Invalid value {}.".format(value))

        result = self._lib.mcc152_dio_config_write_port(
            self._address, item, value)

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid item {}.".format(item))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_config_write_dict(self, item, value_dict):
        """
        Write a digital I/O configuration value for multiple channels.

        There are several configuration items that may be written for the
        digital I/O. They are written for multiple channels at once using a
        dictionary of channel:value pairs. If a channel is repeated in the
        dictionary then the last value will be used. The item is selected with
        the **item** argument, which may be one of the :py:class:`DIOConfigItem`
        values:

        * :py:const:`DIOConfigItem.DIRECTION`: Set the digital I/O channel
          directions by passing 0 in a value for output and 1 for input.
        * :py:const:`DIOConfigItem.PULL_CONFIG`: Configure the pull-up/down
          resistors by passing 0 in a value for pull-down or 1 for pull-up. The
          resistors may be enabled or disabled with the
          :py:const:`DIOConfigItem.PULL_ENABLE` item.
        * :py:const:`DIOConfigItem.PULL_ENABLE`: Enable or disable pull-up/down
          resistors by passing 0 in a value for disabled or 1 for enabled. The
          resistors are configured for pull-up/down with the
          :py:const:`DIOConfigItem.PULL_CONFIG` item. The resistors are
          automatically disabled if the bits are set to output and configured as
          open-drain.
        * :py:const:`DIOConfigItem.INPUT_INVERT`: Enable inverting inputs by
          passing a 0 in a value for normal input or 1 for inverted.
        * :py:const:`DIOConfigItem.INPUT_LATCH`: Enable input latching by
          passing 0 in a value for non-latched or 1 for latched.

          When the input is non-latched, reads show the current status of the
          input. A state change in the corresponding input generates an
          interrupt (if it is not masked). A read of the input clears the
          interrupt. If the input goes back to its initial logic state before
          the input is read, then the interrupt is cleared. When the input is
          latched, a change of state of the input generates an interrupt and the
          input logic value is loaded into the input port register. A read of
          the input will clear the interrupt. If the input returns to its
          initial logic state before the input is read, then the interrupt is
          not cleared and the input register keeps the logic value that
          initiated the interrupt. The next read of the input will show the
          initial state. Care must be taken when using bit reads on the input
          when latching is enabled - the bit method still reads the entire input
          register so a change on another bit could be missed. It is best to use
          port or tuple input reads when using latching.

          If the input is changed from latched to non-latched, a read from the
          input reflects the current terminal logic level. If the input is
          changed from non-latched to latched input, the read from the input
          represents the latched logic level.
        * :py:const:`DIOConfigItem.OUTPUT_TYPE`: Set the output type by writing
          0 for push-pull or 1 for open-drain. This setting affects all outputs
          so is not a per-channel setting. It should be set to the desired type
          before using :py:const:`DIOConfigItem.DIRECTION` to set channels as
          outputs. Internal pull-up/down resistors are disabled when a bit is
          set to output and is configured as open-drain, so external resistors
          should be used.
        * :py:const:`DIOConfigItem.INT_MASK`: Enable or disable interrupt
          generation for specific inputs by masking the interrupt. Write 0 in a
          value to enable the interrupt from that channel or 1 to mask (disable)
          it.

          All MCC 152s share a single interrupt signal to the CPU, so when an
          interrupt occurs the user must determine the source, optionally act
          on the interrupt, then clear that source so that other interrupts
          may be detected. The current interrupt state may be read with
          :py:func:`interrupt_state`. A user program may wait for the interrupt
          to become active with :py:func:`wait_for_interrupt`, or may register
          an interrupt callback function with
          :py:func:`interrupt_callback_enable`. This allows the user to wait for
          a change on one or more inputs without constantly reading the inputs.
          The source of the interrupt may be determined by reading the interrupt
          status of each MCC 152 with :py:func:`dio_int_status_read_bit`,
          :py:func:`dio_int_status_read_port` or
          :py:func:`dio_int_status_read_tuple`, and all active interrupt
          sources must be cleared before the interrupt will become inactive. The
          interrupt is cleared by reading the input with
          :py:func:`dio_input_read_bit`, :py:func:`dio_input_read_port`, or
          :py:func:`dio_input_read_tuple`.

        For example, to set channels 6 and 7 to output: ::

            values = { 6:0, 7:0 }
            mcc152.dio_config_write_dict(DIOConfigItem.DIRECTION, values)

        Args:
            item (integer): The configuration item, one of
                :py:class:`DIOConfigItem`.
            value_dict (dictionary): The configuration values for multiple
                channels in a dictionary of channel:value pairs.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        orig_val = c_ubyte()
        result = self._lib.mcc152_dio_config_read_port(
            self._address, item, byref(orig_val))
        if result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        values = orig_val.value
        channels = 0

        for channel, value in value_dict.items():
            if channel not in range(self._DIO_NUM_CHANNELS):
                raise ValueError("Invalid channel {}.".format(channel))
            if value not in (0, 1):
                raise ValueError(
                    "Invalid value {}, expecting 0 or 1.".format(value))
            mask = 1 << channel
            channels = channels | mask
            if value == 0:
                values = values & ~mask
            else:
                values = values | mask

        if channels == 0:
            raise ValueError("No channels specified.")

        result = self._lib.mcc152_dio_config_write_port(
            self._address, item, values)

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid item {}.".format(item))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return

    def dio_config_read_bit(self, channel, item):
        """
        Read a digital I/O configuration value for a single channel.

        There are several configuration items that may be read for the digital
        I/O. The item is selected with the **item** argument, which may be one
        of the DIOConfigItems values:

        * :py:const:`DIOConfigItem.DIRECTION`: Read the digital I/O channel
          direction setting, where 0 is output and 1 is input.
        * :py:const:`DIOConfigItem.PULL_CONFIG`: Read the pull-up/down resistor
          configuration where 0 is pull-down and 1 is pull-up.
        * :py:const:`DIOConfigItem.PULL_ENABLE`: Read the pull-up/down resistor
          enable setting where 0 is disabled and 1 is enabled.
        * :py:const:`DIOConfigItem.INPUT_INVERT`: Read the input inversion
          setting where 0 is normal input and 1 is inverted.
        * :py:const:`DIOConfigItem.INPUT_LATCH`: Read the input latching setting
          where 0 is non-latched and 1 is latched.
        * :py:const:`DIOConfigItem.OUTPUT_TYPE`: Read the output type setting
          where 0 is push-pull and 1 is open-drain. This setting affects all
          outputs so is not a per-channel setting and the channel argument is
          ignored.
        * :py:const:`DIOConfigItem.INT_MASK`: Read the interrupt mask setting
          where 0 in a bit enables the interrupt and 1 disables it.

        Args:
            channel (integer): The digital I/O channel, 0 - 7.
            item (integer): The configuration item, one of
                :py:class:`DIOConfigItem`.

        Returns
            int: The configuration item value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        if channel not in range(self._DIO_NUM_CHANNELS):
            raise ValueError("Invalid channel {}.".format(channel))

        value = c_ubyte()
        result = self._lib.mcc152_dio_config_read_bit(
            self._address, channel, item, byref(value))

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid item {}.".format(item))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_config_read_port(self, item):
        """
        Read a digital I/O configuration value for all channels.

        There are several configuration items that may be read for the digital
        I/O. They are read for all channels at once, returning an 8-bit integer
        where each bit corresponds to a channel (bit 0 is channel 0, etc.) The
        item is selected with the **item** argument, which may be one of the
        :py:class:`DIOConfigItem` values:

        * :py:const:`DIOConfigItem.DIRECTION`: Read the digital I/O channels
          direction settings, where 0 in a bit is output and 1 is input.
        * :py:const:`DIOConfigItem.PULL_CONFIG`: Read the pull-up/down resistor
          configurations where 0 in a bit is pull-down and 1 is pull-up.
        * :py:const:`DIOConfigItem.PULL_ENABLE`: Read the pull-up/down resistor
          enable settings where 0 in a bit is disabled and 1 is enabled.
        * :py:const:`DIOConfigItem.INPUT_INVERT`: Read the input inversion
          settings where 0 in a bit is normal input and 1 is inverted.
        * :py:const:`DIOConfigItem.INPUT_LATCH`: Read the input latching
          settings where 0 in a bit is non-latched and 1 is latched.
        * :py:const:`DIOConfigItem.OUTPUT_TYPE`: Read the output type setting
          where 0 is push-pull and 1 is open-drain. This setting affects all
          outputs so is not a per-channel setting.
        * :py:const:`DIOConfigItem.INT_MASK`: Read the interrupt mask settings
          where 0 in a bit enables the interrupt and 1 disables it.

        Args:
            item (integer): The configuration item, one of
                :py:class:`DIOConfigItem`.

        Returns
            int: The configuration item value.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_config_read_port(
            self._address, item, byref(value))

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid item {}.".format(item))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        return value.value

    def dio_config_read_tuple(self, item):
        """
        Read a digital I/O configuration value for all channels as a tuple.

        There are several configuration items that may be read for the digital
        I/O. They are read for all channels at once, returning a tuple in
        channel order. The item is selected with the **item** argument, which
        may be one of the :py:class:`DIOConfigItem` values:

        * :py:const:`DIOConfigItem.DIRECTION`: Read the digital I/O channels
          direction settings, where 0 in a value is output and 1 is input.
        * :py:const:`DIOConfigItem.PULL_CONFIG`: Read the pull-up/down resistor
          configurations where 0 in a value is pull-down and 1 is pull-up.
        * :py:const:`DIOConfigItem.PULL_ENABLE`: Read the pull-up/down resistor
          enable settings where 0 in a value is disabled and 1 is enabled.
        * :py:const:`DIOConfigItem.INPUT_INVERT`: Read the input inversion
          settings where 0 in a value is normal input and 1 is inverted.
        * :py:const:`DIOConfigItem.INPUT_LATCH`: Read the input latching
          settings where 0 in a value is non-latched and 1 is latched.
        * :py:const:`DIOConfigItem.OUTPUT_TYPE`: Read the output type setting
          where 0 is push-pull and 1 is open-drain. This setting affects all
          outputs so is not a per-channel setting.
        * :py:const:`DIOConfigItem.INT_MASK`: Read the interrupt mask settings
          where 0 in a value enables the interrupt and 1 disables it.

        Args:
            item (integer): The configuration item, one of
                :py:class:`DIOConfigItem`.

        Returns
            tuple of int: The configuration item values.

        Raises:
            HatError: the board is not initialized, does not respond, or
                responds incorrectly.
            ValueError: an argument is invalid.
        """
        if not self._initialized:
            raise HatError(self._address, "Not initialized.")

        value = c_ubyte()
        result = self._lib.mcc152_dio_config_read_port(
            self._address, item, byref(value))

        if result == self._RESULT_BAD_PARAMETER:
            raise ValueError("Invalid item {}.".format(item))
        elif result != self._RESULT_SUCCESS:
            raise HatError(self._address, "Incorrect response.")

        # convert byte to tuple of bits
        reg = value.value
        mytuple = tuple(((reg >> i) & 0x01) for i in range(
            self._DIO_NUM_CHANNELS))

        return mytuple
