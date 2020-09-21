MCC 172 functions and data
==========================

Functions
---------

==============================================  =========================================================
Function                                        Description
----------------------------------------------  ---------------------------------------------------------
:c:func:`mcc172_open`                           Open an MCC 172 for use.
:c:func:`mcc172_is_open`                        Check if an MCC 172 is open.
:c:func:`mcc172_close`                          Close an MCC 172.
:c:func:`mcc172_info`                           Return information about this device type.
:c:func:`mcc172_blink_led`                      Blink the MCC 172 LED.
:c:func:`mcc172_firmware_version`               Get the firmware version.
:c:func:`mcc172_serial`                         Read the serial number.
:c:func:`mcc172_calibration_date`               Read the calibration date.
:c:func:`mcc172_calibration_coefficient_read`   Read the calibration coefficients for a channel.
:c:func:`mcc172_calibration_coefficient_write`  Write the calibration coefficients for a channel.
:c:func:`mcc172_iepe_config_read`               Read the IEPE configuration for a channel.
:c:func:`mcc172_iepe_config_write`              Write the IEPE configuration for a channel.
:c:func:`mcc172_a_in_sensitivity_read`          Read the sensitivity scaling for a channel.
:c:func:`mcc172_a_in_sensitivity_write`         Write the sensitivity scaling for a channel.
:c:func:`mcc172_a_in_clock_config_read`         Read the sampling clock configuration.
:c:func:`mcc172_a_in_clock_config_write`        Write the sampling clock configuration.
:c:func:`mcc172_trigger_config`                 Configure the external trigger input.
:c:func:`mcc172_a_in_scan_start`                Start a hardware-paced analog input scan.
:c:func:`mcc172_a_in_scan_buffer_size`          Read the size of the internal scan data buffer.
:c:func:`mcc172_a_in_scan_status`               Read the scan status.
:c:func:`mcc172_a_in_scan_read`                 Read scan data and status.
:c:func:`mcc172_a_in_scan_channel_count`        Get the number of channels in the current scan.
:c:func:`mcc172_a_in_scan_stop`                 Stop the scan.
:c:func:`mcc172_a_in_scan_cleanup`              Free scan resources.
==============================================  =========================================================
    
.. doxygenfunction:: mcc172_open
.. doxygenfunction:: mcc172_close
.. doxygenfunction:: mcc172_is_open
.. doxygenfunction:: mcc172_info
.. doxygenfunction:: mcc172_blink_led
.. doxygenfunction:: mcc172_firmware_version
.. doxygenfunction:: mcc172_serial
.. doxygenfunction:: mcc172_calibration_date
.. doxygenfunction:: mcc172_calibration_coefficient_read
.. doxygenfunction:: mcc172_calibration_coefficient_write
.. doxygenfunction:: mcc172_iepe_config_read
.. doxygenfunction:: mcc172_iepe_config_write
.. doxygenfunction:: mcc172_a_in_sensitivity_read
.. doxygenfunction:: mcc172_a_in_sensitivity_write
.. doxygenfunction:: mcc172_a_in_clock_config_read
.. doxygenfunction:: mcc172_a_in_clock_config_write
.. doxygenfunction:: mcc172_trigger_config
.. doxygenfunction:: mcc172_a_in_scan_start
.. doxygenfunction:: mcc172_a_in_scan_buffer_size
.. doxygenfunction:: mcc172_a_in_scan_status
.. doxygenfunction:: mcc172_a_in_scan_read
.. doxygenfunction:: mcc172_a_in_scan_channel_count
.. doxygenfunction:: mcc172_a_in_scan_stop
.. doxygenfunction:: mcc172_a_in_scan_cleanup

Data definitions
----------------

Device Info
~~~~~~~~~~~

.. doxygenstruct:: MCC172DeviceInfo
    :members:

Source Types
~~~~~~~~~~~~

.. doxygenenum:: SourceType
