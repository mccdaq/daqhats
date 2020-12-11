MCC 128 functions and data
==========================

Functions
---------

==============================================  =========================================================
Function                                        Description
----------------------------------------------  ---------------------------------------------------------
:c:func:`mcc128_open`                           Open an MCC 128 for use.
:c:func:`mcc128_is_open`                        Check if an MCC 128 is open.
:c:func:`mcc128_close`                          Close an MCC 128.
:c:func:`mcc128_info`                           Return information about this device type.
:c:func:`mcc128_blink_led`                      Blink the MCC 128 LED.
:c:func:`mcc128_firmware_version`               Get the firmware version.
:c:func:`mcc128_serial`                         Read the serial number.
:c:func:`mcc128_calibration_date`               Read the calibration date.
:c:func:`mcc128_calibration_coefficient_read`   Read the calibration coefficients for a channel.
:c:func:`mcc128_calibration_coefficient_write`  Write the calibration coefficients for a channel.
:c:func:`mcc128_trigger_mode`                   Set the external trigger input mode.
:c:func:`mcc128_a_in_mode_read`                 Read the analog input mode.
:c:func:`mcc128_a_in_mode_write`                Write the analog input mode.
:c:func:`mcc128_a_in_range_read`                Read the analog input range.
:c:func:`mcc128_a_in_range_write`               Write the analog input range.
:c:func:`mcc128_a_in_read`                      Read an analog input value.
:c:func:`mcc128_a_in_scan_actual_rate`          Read the actual sample rate for a set of scan parameters.
:c:func:`mcc128_a_in_scan_start`                Start a hardware-paced analog input scan.
:c:func:`mcc128_a_in_scan_buffer_size`          Read the size of the internal scan data buffer.
:c:func:`mcc128_a_in_scan_status`               Read the scan status.
:c:func:`mcc128_a_in_scan_read`                 Read scan data and status.
:c:func:`mcc128_a_in_scan_channel_count`        Get the number of channels in the current scan.
:c:func:`mcc128_a_in_scan_stop`                 Stop the scan.
:c:func:`mcc128_a_in_scan_cleanup`              Free scan resources.
==============================================  =========================================================
    
.. doxygenfunction:: mcc128_open
.. doxygenfunction:: mcc128_close
.. doxygenfunction:: mcc128_is_open
.. doxygenfunction:: mcc128_info
.. doxygenfunction:: mcc128_blink_led
.. doxygenfunction:: mcc128_firmware_version
.. doxygenfunction:: mcc128_serial
.. doxygenfunction:: mcc128_calibration_date
.. doxygenfunction:: mcc128_calibration_coefficient_read
.. doxygenfunction:: mcc128_calibration_coefficient_write
.. doxygenfunction:: mcc128_a_in_mode_read
.. doxygenfunction:: mcc128_a_in_mode_write
.. doxygenfunction:: mcc128_a_in_range_read
.. doxygenfunction:: mcc128_a_in_range_write
.. doxygenfunction:: mcc128_trigger_mode
.. doxygenfunction:: mcc128_a_in_read
.. doxygenfunction:: mcc128_a_in_scan_actual_rate
.. doxygenfunction:: mcc128_a_in_scan_start
.. doxygenfunction:: mcc128_a_in_scan_buffer_size
.. doxygenfunction:: mcc128_a_in_scan_status
.. doxygenfunction:: mcc128_a_in_scan_read
.. doxygenfunction:: mcc128_a_in_scan_channel_count
.. doxygenfunction:: mcc128_a_in_scan_stop
.. doxygenfunction:: mcc128_a_in_scan_cleanup

Data definitions
----------------

Device Info
~~~~~~~~~~~

.. doxygenstruct:: MCC128DeviceInfo
    :members:

Analog Input Modes
~~~~~~~~~~~~~~~~~~

.. doxygenenum:: AnalogInputMode

Analog Input Ranges
~~~~~~~~~~~~~~~~~~~

.. doxygenenum:: AnalogInputRange