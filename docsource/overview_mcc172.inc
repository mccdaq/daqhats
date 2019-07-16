MCC 172
=======

The MCC 172 is a 2-channel analog voltage input board with the following features:

- Two 24-bit, 51.2 kS/s A/D converters (one per channel)
- ±5 V AC coupled differential analog inputs
- IEPE sensor support
- 10-32 and screw terminal connectors for the analog inputs
- ADC conversions can be synchronized between multiple boards
- Onboard sample buffers
- Digital trigger input

.. only:: html

    .. image:: images/mcc172-web.jpg
        :align: center
        :alt: MCC 172 circuit board

.. only:: latex

    .. image:: images/mcc172.jpg
        :align: center
        :alt: MCC 172 circuit board

Board components
----------------
10-32 connectors
^^^^^^^^^^^^^^^^
- **Ch 0** and **Ch 1**: Analog input connectors (do not use both 10-32 and
  screw terminals at the same time for an input channel).

Screw terminals
^^^^^^^^^^^^^^^
- **Ch 0** and **Ch 1**: Analog input terminals  (do not use both 10-32 and
  screw terminals at the same time for an input channel).
- **Trigger** (TRIG): External digital trigger input terminal. The trigger mode 
  is software configurable for edge or level sensitive, rising or falling edge, 
  high or low level.
- **DGND** (GND): Digital ground for the trigger terminal.

Address jumpers
^^^^^^^^^^^^^^^
- **A0** to **A2**: Used to identify each HAT when multiple boards are connected. 
  The first HAT connected to the Raspberry Pi must be at address 0 (no jumper). 
  Install jumpers on each additional connected board to set the desired address. 
  Refer to the :ref:`multiple` topic for more information about the recommended 
  addressing method.

Status LED
^^^^^^^^^^
The LED turns on when the board is connected to a Raspberry Pi with external 
power applied and flashes when communicating with the board. The LED may be
blinked by the user.

Header connector
^^^^^^^^^^^^^^^^
The board header is used to connect with the Raspberry Pi. Refer to 
:ref:`install` for more information about the header connector.

Functional block diagram
------------------------

.. only:: html

    .. image:: images/mcc172-diag-web.jpg
	:align: center
	:alt: MCC 172 block diagram

.. only:: latex

    .. image:: images/mcc172-diag.jpg
	:align: center
	:alt: MCC 172 block diagram

Functional details
------------------

ADC clock
^^^^^^^^^^

The ADCs on a board share the same clock and are synchronized to start
conversions at the same time for synchronous data.  The clock and synchronize
signals may also be shared across the Raspberry Pi GPIO header to synchronize
multiple MCC 172s.  The clock is programmable for various sampling rates between
51.2 kS/s and 200 S/s.

Trigger
^^^^^^^

The trigger input (terminal TRIG) is used to hold off the beginning of an analog
input scan until the desired condition is met at the trigger input.  The trigger
input signal may be a 3.3V or 5V TTL or CMOS logic signal.  The input condition
may be rising edge, falling edge, high level, or low level.  The trigger may
also be shared across the Raspberry Pi GPIO header to synchronize multiple 
MCC 172s.

Firmware updates
----------------

Use the firmware update tool to update the firmware on your MCC 172 board(s).
The "0" in the example below is the board address. Repeat the command for each
MCC 172 address in your board stack. This example demonstrates how to update the
firmware on the MCC 172 that is installed at address 0::

    mcc172_firmware_update 0 ~/daqhats/tools/MCC_172.fw


Specifications
--------------

.. only:: html

    `MCC 172 Electrical Specifications <_static/esmcc118.pdf>`_

.. raw:: latex

    \includepdf[pages=-]{../../specs/esmcc118.pdf}
