.. currentmodule:: daqhats

MCC 134 class
=============

Methods
-------

.. autoclass:: mcc134
    :inherited-members:
    :members:

    .. tabularcolumns:: |p{210pt}|p{210pt}|

    .. rubric:: Methods

    ==================================================  ===========================================================
    Method                                              Description
    --------------------------------------------------  -----------------------------------------------------------
    :py:func:`mcc134.info`                              Get info about this device type.
    :py:func:`mcc134.serial`                            Read the serial number.
    :py:func:`mcc134.calibration_date`                  Read the calibration date.
    :py:func:`mcc134.calibration_coefficient_read`      Read the calibration coefficients for a channel.
    :py:func:`mcc134.calibration_coefficient_write`     Write the calibration coefficients for a channel.
    :py:func:`mcc134.tc_type_write`                     Write the thermocouple type for a channel.
    :py:func:`mcc134.tc_type_read`                      Read the thermocouple type for a channel.
    :py:func:`mcc134.update_interval_write`             Write the temperature update interval.
    :py:func:`mcc134.update_interval_read`              Read the temperature update interval.
    :py:func:`mcc134.t_in_read`                         Read a temperature input channel.
    :py:func:`mcc134.a_in_read`                         Read an analog input channel.
    :py:func:`mcc134.cjc_read`                          Read a CJC temperature
    :py:func:`mcc134.address`                           Read the board's address.
    ==================================================  ===========================================================

Data
----

Thermocouple types
~~~~~~~~~~~~~~~~~~

.. autoclass:: TcTypes
    :members:
