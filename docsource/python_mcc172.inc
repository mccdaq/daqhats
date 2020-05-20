.. currentmodule:: daqhats

MCC 172 class
=============

Methods
-------

.. autoclass:: mcc172
    :inherited-members:
    :members:
    :exclude-members: test_signals_write, test_signals_read

    .. tabularcolumns:: |p{230pt}|p{190pt}|

    .. rubric:: Methods

    ==================================================  ========================================================
    Method                                              Description
    --------------------------------------------------  --------------------------------------------------------
    :py:func:`mcc172.info`                              Get info about this device type.
    :py:func:`mcc172.firmware_version`                  Get the firmware version.
    :py:func:`mcc172.serial`                            Read the serial number.
    :py:func:`mcc172.blink_led`                         Blink the MCC 172 LED.
    :py:func:`mcc172.calibration_date`                  Read the calibration date.
    :py:func:`mcc172.calibration_coefficient_read`      Read the calibration coefficients for a channel.
    :py:func:`mcc172.calibration_coefficient_write`     Write the calibration coefficients for a channel.
    :py:func:`mcc172.iepe_config_read`                  Read the IEPE configuration for a channel.
    :py:func:`mcc172.iepe_config_write`                 Write the IEPE configuration for a channel.
    :py:func:`mcc172.a_in_sensitivity_read`             Read the sensitivity factor for a channel.
    :py:func:`mcc172.a_in_sensitivity_write`            Write the sensitivity factor for a channel.
    :py:func:`mcc172.a_in_clock_config_read`            Read the sampling clock configuration.
    :py:func:`mcc172.a_in_clock_config_write`           Write the sampling clock configuration.
    :py:func:`mcc172.trigger_config`                    Configure the external trigger input.
    :py:func:`mcc172.a_in_scan_actual_rate`             Read the actual sample rate for a requested sample rate.
    :py:func:`mcc172.a_in_scan_start`                   Start a hardware-paced analog input scan.
    :py:func:`mcc172.a_in_scan_buffer_size`             Read the size of the internal scan data buffer.
    :py:func:`mcc172.a_in_scan_read`                    Read scan status / data (list).
    :py:func:`mcc172.a_in_scan_read_numpy`              Read scan status / data (NumPy array).
    :py:func:`mcc172.a_in_scan_channel_count`           Get the number of channels in the current scan.
    :py:func:`mcc172.a_in_scan_stop`                    Stop the scan.
    :py:func:`mcc172.a_in_scan_cleanup`                 Free scan resources.
    :py:func:`mcc172.address`                           Read the board's address.
    ==================================================  ========================================================

Data
----

Source types
~~~~~~~~~~~~

.. autoclass:: SourceType
    :members:
