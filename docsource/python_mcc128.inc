.. currentmodule:: daqhats

MCC 128 class
=============

Methods
-------

.. autoclass:: mcc128
    :inherited-members:
    :members:
    :exclude-members: test_clock, test_trigger

    .. tabularcolumns:: |p{230pt}|p{190pt}|

    .. rubric:: Methods

    ==================================================  ========================================================
    Method                                              Description
    --------------------------------------------------  --------------------------------------------------------
    :py:func:`mcc128.info`                              Get info about this device type.
    :py:func:`mcc128.firmware_version`                  Get the firmware version.
    :py:func:`mcc128.serial`                            Read the serial number.
    :py:func:`mcc128.blink_led`                         Blink the MCC 128 LED.
    :py:func:`mcc128.calibration_date`                  Read the calibration date.
    :py:func:`mcc128.calibration_coefficient_read`      Read the calibration coefficients for a mode/range.
    :py:func:`mcc128.calibration_coefficient_write`     Write the calibration coefficients for a mode/range.
    :py:func:`mcc128.trigger_mode`                      Set the external trigger input mode.
    :py:func:`mcc128.a_in_mode_read`                    Read the analog input mode.
    :py:func:`mcc128.a_in_mode_write`                   Write the analog input mode.
    :py:func:`mcc128.a_in_range_read`                   Read the analog input range.
    :py:func:`mcc128.a_in_range_write`                  Write the analog input range.
    :py:func:`mcc128.a_in_read`                         Read an analog input channel.
    :py:func:`mcc128.a_in_scan_actual_rate`             Read the actual sample rate for a requested sample rate.
    :py:func:`mcc128.a_in_scan_start`                   Start a hardware-paced analog input scan.
    :py:func:`mcc128.a_in_scan_buffer_size`             Read the size of the internal scan data buffer.
    :py:func:`mcc128.a_in_scan_read`                    Read scan status / data (list).
    :py:func:`mcc128.a_in_scan_read_numpy`              Read scan status / data (NumPy array).
    :py:func:`mcc128.a_in_scan_channel_count`           Get the number of channels in the current scan.
    :py:func:`mcc128.a_in_scan_stop`                    Stop the scan.
    :py:func:`mcc128.a_in_scan_cleanup`                 Free scan resources.
    :py:func:`mcc128.address`                           Read the board's address.
    ==================================================  ========================================================

Data
----

Analog input modes
~~~~~~~~~~~~~~~~~~

.. autoclass:: AnalogInputMode
    :members:

Analog input ranges
~~~~~~~~~~~~~~~~~~~

.. autoclass:: AnalogInputRange
    :members:
