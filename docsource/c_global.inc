Global functions and data
=========================

Functions
---------

========================================  ===============================================
Function                                  Description
----------------------------------------  -----------------------------------------------
:c:func:`hat_list`                        Return a list of detected DAQ HAT boards.
:c:func:`hat_error_message`               Return a text description for a DAQ HAT result.
:c:func:`hat_wait_for_interrupt`          Wait for an interrupt to occur.
:c:func:`hat_interrupt_state`             Read the current interrupt status.
:c:func:`hat_interrupt_callback_enable`   Enable an interrupt callback function.
:c:func:`hat_interrupt_callback_disable`  Disable interrupt callback function.
========================================  ===============================================

.. doxygenfunction:: hat_list
.. doxygenfunction:: hat_error_message
.. doxygenfunction:: hat_wait_for_interrupt
.. doxygenfunction:: hat_interrupt_state
.. doxygenfunction:: hat_interrupt_callback_enable
.. doxygenfunction:: hat_interrupt_callback_disable

Data types and definitions
--------------------------

.. doxygendefine:: MAX_NUMBER_HATS

HAT IDs
~~~~~~~

.. doxygenenum:: HatIDs

Result Codes
~~~~~~~~~~~~

.. doxygenenum:: ResultCode

HatInfo structure
~~~~~~~~~~~~~~~~~

.. doxygenstruct:: HatInfo
    :members:

Analog Input / Scan Option Flags
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See individual function documentation for detailed usage information.

.. doxygendefine:: OPTS_DEFAULT
.. doxygendefine:: OPTS_NOSCALEDATA
.. doxygendefine:: OPTS_NOCALIBRATEDATA
.. doxygendefine:: OPTS_EXTCLOCK
.. doxygendefine:: OPTS_EXTTRIGGER
.. doxygendefine:: OPTS_CONTINUOUS

Scan Status Flags
~~~~~~~~~~~~~~~~~

.. doxygendefine:: STATUS_HW_OVERRUN
.. doxygendefine:: STATUS_BUFFER_OVERRUN
.. doxygendefine:: STATUS_TRIGGERED
.. doxygendefine:: STATUS_RUNNING

Trigger Modes
~~~~~~~~~~~~~

.. doxygenenum:: TriggerMode
