MCC 118 functions and data
==========================

Functions
---------

==============================================  =========================================================
Function                                        Description
----------------------------------------------  ---------------------------------------------------------
:c:func:`mcc118_open`                           Open an MCC 118 for use.
:c:func:`mcc118_is_open`                        Check if an MCC 118 is open.
:c:func:`mcc118_close`                          Close an MCC 118.
:c:func:`mcc118_info`                           Return information about this device type.
:c:func:`mcc118_blink_led`                      Blink the MCC 118 LED.
:c:func:`mcc118_firmware_version`               Get the firmware version.
:c:func:`mcc118_serial`                         Read the serial number.
:c:func:`mcc118_calibration_date`               Read the calibration date.
:c:func:`mcc118_calibration_coefficient_read`   Read the calibration coefficients for a channel.
:c:func:`mcc118_calibration_coefficient_write`  Write the calibration coefficients for a channel.
:c:func:`mcc118_a_in_read`                      Read an analog input value.
:c:func:`mcc118_trigger_mode`                   Set the external trigger input mode.
:c:func:`mcc118_a_in_scan_actual_rate`          Read the actual sample rate for a set of scan parameters.
:c:func:`mcc118_a_in_scan_start`                Start a hardware-paced analog input scan.
:c:func:`mcc118_a_in_scan_buffer_size`          Read the size of the internal scan data buffer.
:c:func:`mcc118_a_in_scan_status`               Read the scan status.
:c:func:`mcc118_a_in_scan_read`                 Read scan data and status.
:c:func:`mcc118_a_in_scan_channel_count`        Get the number of channels in the current scan.
:c:func:`mcc118_a_in_scan_stop`                 Stop the scan.
:c:func:`mcc118_a_in_scan_cleanup`              Free scan resources.
==============================================  =========================================================
    
.. doxygenfunction:: mcc118_open
.. doxygenfunction:: mcc118_close
.. doxygenfunction:: mcc118_is_open
.. doxygenfunction:: mcc118_info
.. doxygenfunction:: mcc118_blink_led
.. doxygenfunction:: mcc118_firmware_version
.. doxygenfunction:: mcc118_serial
.. doxygenfunction:: mcc118_calibration_date
.. doxygenfunction:: mcc118_calibration_coefficient_read
.. doxygenfunction:: mcc118_calibration_coefficient_write
.. doxygenfunction:: mcc118_a_in_read
.. doxygenfunction:: mcc118_trigger_mode
.. doxygenfunction:: mcc118_a_in_scan_actual_rate
.. doxygenfunction:: mcc118_a_in_scan_start
.. doxygenfunction:: mcc118_a_in_scan_buffer_size
.. doxygenfunction:: mcc118_a_in_scan_status
.. doxygenfunction:: mcc118_a_in_scan_read
.. doxygenfunction:: mcc118_a_in_scan_channel_count
.. doxygenfunction:: mcc118_a_in_scan_stop
.. doxygenfunction:: mcc118_a_in_scan_cleanup

Data definitions
----------------

Device Info
~~~~~~~~~~~

.. doxygenstruct:: MCC118DeviceInfo
    :members:
