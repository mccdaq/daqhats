Global functions and data
=========================

Functions
---------

===========================  ===================================================
Function                     Description
---------------------------  ---------------------------------------------------
:c:func:`hat_list`           Return a list of detected DAQ HAT boards.
:c:func:`hat_error_message`  Return a text description for a DAQ HAT result.
===========================  ===================================================

.. doxygenfunction:: hat_list
.. doxygenfunction:: hat_error_message

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
