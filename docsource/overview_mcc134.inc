MCC 134
=======

The MCC 134 is a 4-channel thermocouple input board with the following features:

- 24-bit A/D converter
- Onboard sensor for cold junction compensation
- Linearization for J, K, R, S, T, N, E, B type thermocouples
- Open thermocouple detection
- Thermocouple inputs are electrically isolated from the Raspberry Pi for use in harsh environments

.. only:: html

    .. image:: images/mcc134-web.jpg
        :align: center
        :alt: MCC 134 circuit board

.. only:: latex

    .. image:: images/mcc134.jpg
        :align: center
        :alt: MCC 134 circuit board


Board components
----------------
Screw terminals
^^^^^^^^^^^^^^^
- **CH0H/CH0L** to **CH3H/CH3L** (+x-): Differential thermocouple input terminals.

Address jumpers
^^^^^^^^^^^^^^^
- **A0** to **A2**: Used to identify each HAT when multiple boards are connected. The first HAT connected to the Raspberry Pi must be at address 0 (no jumper). Install jumpers on each additional connected board to set the desired address. Refer to the :ref:`multiple` topic for more information about the recommended addressing method.

Status LED
^^^^^^^^^^
The LED turns on when the board is connected to a Raspberry Pi with external power applied. 

Header connector
^^^^^^^^^^^^^^^^
The board header is used to connect with the Raspberry Pi. Refer to :ref:`install` for more information about the header connector.

Functional block diagram
------------------------

.. only:: html

    .. image:: images/mcc134-diag-web.jpg
	:align: center
	:alt: MCC 134 block diagram

.. only:: latex

    .. image:: images/mcc134-diag.jpg
	:align: center
	:alt: MCC 134 block diagram

Specifications
--------------

.. only:: html

    `MCC 134 Electrical Specifications <_static/esmcc134.pdf>`_

.. raw:: latex

    \includepdf[pages=-]{../../specs/esmcc134.pdf}	
