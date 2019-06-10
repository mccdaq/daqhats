.. currentmodule:: daqhats

MCC 152 class
=============

Methods
-------

.. autoclass:: mcc152
    :inherited-members:
    :members:

    .. tabularcolumns:: |p{210pt}|p{210pt}|

    .. rubric:: Methods

    ===========================================  ========================================================================
    Method                                       Description
    -------------------------------------------  ------------------------------------------------------------------------
    :py:func:`mcc152.info`                       Get info about this device type.
    :py:func:`mcc152.serial`                     Read the serial number.
    :py:func:`mcc152.a_out_write`                Write an analog output channel.
    :py:func:`mcc152.a_out_write_all`            Write all analog output channels.
    :py:func:`mcc152.dio_reset`                  Reset the digital I/O to the default configuration.
    :py:func:`mcc152.dio_input_read_bit`         Read a digital input.
    :py:func:`mcc152.dio_input_read_port`        Read all digital inputs.
    :py:func:`mcc152.dio_input_read_tuple`       Read all digital inputs as a tuple.
    :py:func:`mcc152.dio_output_write_bit`       Write a digital output.
    :py:func:`mcc152.dio_output_write_port`      Write all digital outputs.
    :py:func:`mcc152.dio_output_write_dict`      Write multiple digital outputs with a dictionary.
    :py:func:`mcc152.dio_output_read_bit`        Read the state of a digital output.
    :py:func:`mcc152.dio_output_read_port`       Read the state of all digital outputs.
    :py:func:`mcc152.dio_output_read_tuple`      Read the state of all digital outputs as a tuple.
    :py:func:`mcc152.dio_int_status_read_bit`    Read the interrupt status for a single channel.
    :py:func:`mcc152.dio_int_status_read_port`   Read the interrupt status for all channels.
    :py:func:`mcc152.dio_int_status_read_tuple`  Read the interrupt status for all channels as a tuple.
    :py:func:`mcc152.dio_config_write_bit`       Write a digital I/O configuration item value for a single channel.
    :py:func:`mcc152.dio_config_write_port`      Write a digital I/O configuration item value for all channels.
    :py:func:`mcc152.dio_config_write_dict`      Write a digital I/O configuration item value for multiple channels.
    :py:func:`mcc152.dio_config_read_bit`        Read a digital I/O configuration item value for a single channel.
    :py:func:`mcc152.dio_config_read_port`       Read a digital I/O configuration item value for all channels.
    :py:func:`mcc152.dio_config_read_tuple`      Read a digital I/O configuration item value for all channels as a tuple.
    :py:func:`mcc152.address`                    Read the board's address.
    ===========================================  ========================================================================

Data
----

DIO Config Items
~~~~~~~~~~~~~~~~

.. autoclass:: DIOConfigItem
    :members:
