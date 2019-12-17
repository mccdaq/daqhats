MCC 118
=======

The MCC 118 is an 8-channel analog voltage input board with the following features:

- 12-bit, 100 kS/s A/D converter
- ±10 V single-ended analog inputs
- Factory calibration with ±20.8 mV input accuracy
- Bidirectional scan clock
- Onboard sample buffers
- Digital trigger input

.. only:: html

    .. image:: images/mcc118-web.jpg
        :align: center
        :alt: MCC 118 circuit board

.. only:: latex

    .. image:: images/mcc118.jpg
        :align: center
        :alt: MCC 118 circuit board

Board components
----------------
Screw terminals
^^^^^^^^^^^^^^^
- **CH 0 In** to **CH 7 In** (CHx): Single-ended analog input terminals.
- **Clock** (CLK): Bidirectional terminal for scan clock input / output. Set the direction with software. 
  Set for input to clock the scans with an external clock signal, or output to use the internal scan clock.
- **Trigger** (TRIG): External digital trigger input terminal. The trigger mode is software configurable for edge or level sensitive, 
  rising or falling edge, high or low level.
- **AGND** (GND): Common ground for the analog input terminals.
- **DGND** (GND): Common ground for the clock and trigger terminals.

Address jumpers
^^^^^^^^^^^^^^^
- **A0** to **A2**: Used to identify each HAT when multiple boards are connected. The first HAT connected to the Raspberry Pi must be at address 0 (no jumper). Install jumpers on each additional connected board to set the desired address. Refer to the :ref:`multiple` topic for more information about the recommended addressing method.

Status LED
^^^^^^^^^^
The LED turns on when the board is connected to a Raspberry Pi with external power applied and flashes when communicating
with the board. The LED may be blinked by the user.

Header connector
^^^^^^^^^^^^^^^^
The board header is used to connect with the Raspberry Pi. Refer to :ref:`install` for more information about the header connector.

Functional block diagram
------------------------

.. only:: html

    .. image:: images/mcc118-diag-web.jpg
	:align: center
	:alt: MCC 118 block diagram

.. only:: latex

    .. image:: images/mcc118-diag.jpg
	:align: center
	:alt: MCC 118 block diagram

Functional details
------------------

Scan clock
^^^^^^^^^^

The clock input / output (terminal CLK) is used to output the internal scan clock or apply an external scan clock to the device. The clock input signal may be a 3.3V or 5V TTL or CMOS logic signal, and the output will be 3.3V LVCMOS. A scan occurs for each rising edge of the clock, acquiring one sample from each of the selected channels in the scan. For example, when scanning channels 0, 1, and 2 the conversion activity will be:

.. only:: html

    .. image:: images/mcc118-clock-web.png
	:align: center
	:alt: MCC 118 clock mode

.. only:: latex

    .. image:: images/mcc118-clock.png
	:align: center
	:alt: MCC 118 clock mode

Trigger
^^^^^^^

The trigger input (terminal TRIG) is used to hold off the beginning of an analog input scan until the desired condition is met at the trigger input.  The trigger input signal may be a 3.3V or 5V TTL or CMOS logic signal.  The input condition may be rising edge, falling edge, high level, or low level.

Firmware updates
----------------

Use the firmware update tool to update the firmware on your MCC 118 board(s).
The "0" in the example below is the board address. Repeat the command for each
MCC 118 address in your board stack. This example demonstrates how to update the
firmware on the MCC 118 that is installed at address 0::

    mcc118_firmware_update 0 ~/daqhats/tools/MCC_118.hex

MCC 118-OEM
------------
An OEM version is available that is designed with (unpopulated) header connectors 
instead of screw terminals. The board accepts 1x6 and 1x10 0.1” spacing header connectors.
The MCC 118-OEM is functionally equivalent to the standard version. Refer to the 
Electrical Specifications for connector information.

.. only:: html

    .. image:: images/mcc118oem-web.jpg
        :align: center
        :alt: MCC 118-OEM circuit board

.. only:: latex

    .. image:: images/mcc118oem.jpg
        :align: center
        :alt: MCC 118-OEM circuit board

Specifications
--------------

.. only:: html

    `MCC 118 Electrical Specifications <_static/esmcc118.pdf>`_

.. raw:: latex

    \includepdf[pages=-]{../../specs/esmcc118.pdf}
