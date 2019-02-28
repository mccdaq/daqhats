.. currentmodule:: daqhats

Global methods and data
=======================

Methods
-------

=====================================  =============================================
Method                                 Description
-------------------------------------  ---------------------------------------------
:py:func:`hat_list`                    Return a list of detected DAQ HAT boards.
:py:func:`interrupt_state`             Read the current DAQ HAT interrupt status.
:py:func:`wait_for_interrupt`          Wait for a DAQ HAT  interrupt to occur.
:py:func:`interrupt_callback_enable`   Enable an interrupt callback function.
:py:func:`interrupt_callback_disable`  Disable interrupt callback function.
=====================================  =============================================

.. autofunction:: hat_list
.. autofunction:: interrupt_state
.. autofunction:: wait_for_interrupt
.. autofunction:: interrupt_callback_enable
.. autofunction:: interrupt_callback_disable

Data
----

Hat IDs
~~~~~~~

.. autoclass:: HatIDs
    :members:

Trigger modes
~~~~~~~~~~~~~

.. autoclass:: TriggerModes
    :members:

Scan / read option flags
~~~~~~~~~~~~~~~~~~~~~~~~

.. autoclass:: OptionFlags
    :members:

HatError class
--------------

.. autoexception:: HatError
