.. currentmodule:: daqhats

MCC 118 class
=============

Methods
-------

.. autoclass:: mcc118
    :inherited-members:
    :members:
    :exclude-members: test_clock, test_trigger

    .. tabularcolumns:: |p{230pt}|p{190pt}|

    .. rubric:: Methods

    ==================================================  ========================================================
    Method                                              Description
    --------------------------------------------------  --------------------------------------------------------
    :py:func:`mcc118.info`                              Get info about this device type.
    :py:func:`mcc118.firmware_version`                  Get the firmware version.
    :py:func:`mcc118.serial`                            Read the serial number.
    :py:func:`mcc118.blink_led`                         Blink the MCC 118 LED.
    :py:func:`mcc118.calibration_date`                  Read the calibration date.
    :py:func:`mcc118.calibration_coefficient_read`      Read the calibration coefficients for a channel.
    :py:func:`mcc118.calibration_coefficient_write`     Write the calibration coefficients for a channel.
    :py:func:`mcc118.trigger_mode`                      Set the external trigger input mode.
    :py:func:`mcc118.a_in_read`                         Read an analog input channel.
    :py:func:`mcc118.a_in_scan_actual_rate`             Read the actual sample rate for a requested sample rate.
    :py:func:`mcc118.a_in_scan_start`                   Start a hardware-paced analog input scan.
    :py:func:`mcc118.a_in_scan_buffer_size`             Read the size of the internal scan data buffer.
    :py:func:`mcc118.a_in_scan_read`                    Read scan status / data (list).
    :py:func:`mcc118.a_in_scan_read_numpy`              Read scan status / data (NumPy array).
    :py:func:`mcc118.a_in_scan_channel_count`           Get the number of channels in the current scan.
    :py:func:`mcc118.a_in_scan_stop`                    Stop the scan.
    :py:func:`mcc118.a_in_scan_cleanup`                 Free scan resources.
    :py:func:`mcc118.address`                           Read the board's address.
    ==================================================  ========================================================
