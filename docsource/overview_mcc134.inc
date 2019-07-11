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

Functional details
------------------

Best practices for accurate thermocouple measurements
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The MCC 134 should achieve results within the maximum thermocouple
accuracy specifications when operating within the documented environmental
conditions. Operating in conditions with excessive temperature transients or
airflow may affect results. In most cases, the MCC 134 will achieve the
typical specifications. To achieve the most accurate thermocouple readings,
MCC recommends the following practices:

- *Reduce the load on the Raspberry Pi processor.* Running a program that fully
  loads all 4 cores on the Raspberry Pi processor can raise the temperature
  of the processor above 70 °C. Running a program that only loads 1 core will
  operate approximately 20 °C cooler.
- *Minimize environmental temperature variations.* Place the MCC 134 away from
  heat or cooling sources that cycle on and off. Sudden environmental changes
  may lead to increased errors.
- *Provide a steady airflow, such as from a fan.* A steady airflow can
  dissipate heat and reduce errors.
- *When configuring multiple MCC DAQ Hats in a stack, position the MCC 134
  farthest from the Raspberry Pi board.* Since the Raspberry Pi is a
  significant heat source, placing the MCC 134 farthest from the Pi will
  increase accuracy.

For additional information, refer to the `Measuring Thermocouples with Raspberry Pi
and the MCC 134 <https://www.mccdaq.com/TechTips/TechTip-11.aspx>`_ Tech Tip.

Specifications
--------------

.. only:: html

    `MCC 134 Electrical Specifications <_static/esmcc134.pdf>`_

.. raw:: latex

    \includepdf[pages=-]{../../specs/esmcc134.pdf}	
