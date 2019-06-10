MCC 134 functions and data
==========================

Functions
---------

==============================================  ===========================================================
Function                                        Description
----------------------------------------------  -----------------------------------------------------------
:c:func:`mcc134_open`                           Open an MCC 134 for use.
:c:func:`mcc134_is_open`                        Check if an MCC 134 is open.
:c:func:`mcc134_close`                          Close an MCC 134.
:c:func:`mcc134_info`                           Return information about this device type.
:c:func:`mcc134_serial`                         Read the serial number.
:c:func:`mcc134_calibration_date`               Read the calibration date.
:c:func:`mcc134_calibration_coefficient_read`   Read the calibration coefficients for a channel.
:c:func:`mcc134_calibration_coefficient_write`  Write the calibration coefficients for a channel.
:c:func:`mcc134_tc_type_write`                  Write the thermocouple type for a channel.
:c:func:`mcc134_tc_type_read`                   Read the thermocouple type for a channel.
:c:func:`mcc134_update_interval_write`          Write the temperature update interval.
:c:func:`mcc134_update_interval_read`           Read the temperature update interval.
:c:func:`mcc134_t_in_read`                      Read a temperature input value.
:c:func:`mcc134_a_in_read`                      Read an analog input value.
:c:func:`mcc134_cjc_read`                       Read a CJC temperature.
==============================================  ===========================================================
    
.. doxygenfunction:: mcc134_open
.. doxygenfunction:: mcc134_is_open
.. doxygenfunction:: mcc134_close
.. doxygenfunction:: mcc134_info
.. doxygenfunction:: mcc134_serial
.. doxygenfunction:: mcc134_calibration_date
.. doxygenfunction:: mcc134_calibration_coefficient_read
.. doxygenfunction:: mcc134_calibration_coefficient_write
.. doxygenfunction:: mcc134_tc_type_write
.. doxygenfunction:: mcc134_tc_type_read
.. doxygenfunction:: mcc134_update_interval_write
.. doxygenfunction:: mcc134_update_interval_read
.. doxygenfunction:: mcc134_t_in_read
.. doxygenfunction:: mcc134_a_in_read
.. doxygenfunction:: mcc134_cjc_read

Data definitions
----------------

.. doxygendefine:: OPEN_TC_VALUE
.. doxygendefine:: OVERRANGE_TC_VALUE
.. doxygendefine:: COMMON_MODE_TC_VALUE

Device Info
~~~~~~~~~~~

.. doxygenstruct:: MCC134DeviceInfo
    :members:

Thermocouple Types
~~~~~~~~~~~~~~~~~~

.. doxygenenum:: TcTypes
