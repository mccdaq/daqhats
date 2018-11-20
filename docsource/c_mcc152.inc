MCC 152 functions and data
==========================

Functions
---------

==============================================  ==================================================================
Function                                        Description
----------------------------------------------  ------------------------------------------------------------------
:c:func:`mcc152_open`                           Open an MCC 152 for use.
:c:func:`mcc152_is_open`                        Check if an MCC 152 is open.
:c:func:`mcc152_close`                          Close an MCC 152.
:c:func:`mcc152_info`                           Return information about this device type.
:c:func:`mcc152_serial`                         Read the serial number.
:c:func:`mcc152_a_out_write`                    Write an analog output channel value.
:c:func:`mcc152_a_out_write_all`                Write all analog output channels simultaneously.
:c:func:`mcc152_dio_reset`                      Reset the digital I/O to the default configuration.
:c:func:`mcc152_dio_input_read_bit`             Read a digital input.
:c:func:`mcc152_dio_input_read_port`            Read all digital inputs.
:c:func:`mcc152_dio_output_write_bit`           Write a digital output.
:c:func:`mcc152_dio_output_write_port`          Write all digital outputs.
:c:func:`mcc152_dio_output_read_bit`            Read the state of a digital output.
:c:func:`mcc152_dio_output_read_port`           Read the state of all digital outputs.
:c:func:`mcc152_dio_int_status_read_bit`        Read the interrupt status for a single channel.
:c:func:`mcc152_dio_int_status_read_port`       Read the interrupt status for all channels.
:c:func:`mcc152_dio_config_write_bit`           Write a digital I/O configuration item value for a single channel.
:c:func:`mcc152_dio_config_write_port`          Write a digital I/O configuration item value for all channels.
:c:func:`mcc152_dio_config_read_bit`            Read a digital I/O configuration item value for a single channel.
:c:func:`mcc152_dio_config_read_port`           Read a digital I/O configuration item value for all channels.
==============================================  ==================================================================
    
.. doxygenfunction:: mcc152_open
.. doxygenfunction:: mcc152_is_open
.. doxygenfunction:: mcc152_close
.. doxygenfunction:: mcc152_info
.. doxygenfunction:: mcc152_serial
.. doxygenfunction:: mcc152_a_out_write
.. doxygenfunction:: mcc152_a_out_write_all
.. doxygenfunction:: mcc152_dio_reset
.. doxygenfunction:: mcc152_dio_input_read_bit
.. doxygenfunction:: mcc152_dio_input_read_port
.. doxygenfunction:: mcc152_dio_output_write_bit
.. doxygenfunction:: mcc152_dio_output_write_port
.. doxygenfunction:: mcc152_dio_output_read_bit
.. doxygenfunction:: mcc152_dio_output_read_port
.. doxygenfunction:: mcc152_dio_int_status_read_bit
.. doxygenfunction:: mcc152_dio_int_status_read_port
.. doxygenfunction:: mcc152_dio_config_write_bit
.. doxygenfunction:: mcc152_dio_config_write_port
.. doxygenfunction:: mcc152_dio_config_read_bit
.. doxygenfunction:: mcc152_dio_config_read_port

Data types and definitions
--------------------------

Device Info
~~~~~~~~~~~

.. doxygenstruct:: MCC152DeviceInfo
    :members:

DIO Config Items
~~~~~~~~~~~~~~~~

.. doxygenenum:: DIOConfigItem
