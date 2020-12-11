*****************
Hardware Overview
*****************

The MCC DAQ HATs are Raspberry Pi add-on boards (Hardware Attached on Top). They adhere
to the Raspberry Pi HAT specification, but also extend it to allow stacking up to
8 MCC boards on a single Raspberry Pi.

C and Python libraries, documentation, and examples are provided to allow you to
develop your own applications.

Hardware Compatibility
======================
The MCC DAQ HATs are compatible with all Raspberry Pi models with the 40-pin
GPIO header (not the original Pi 1 A or B with the 26-pin header.) They are
generally not compatible with any other brand of Raspberry Pi HAT or add-on board
that attaches to the GPIO header, or devices that use the Raspberry Pi SPI interface.

In particular, LCD displays that use the GPIO header (not HDMI) usually use the
SPI interface and will prevent the DAQ HATs from working. Even if the display is
removed, the driver is probably still loaded by /boot/config.txt and will cause
issues with the DAQ HATs. If you have a problem with your device and have used a
GPIO header display with your Raspberry Pi then consult your display hardware
documentation for how to remove the driver.

The specific pins used by each DAQ HAT are documented in the electrical
specifications for that device.

.. include:: overview_mcc118.inc
.. include:: overview_mcc128.inc
.. include:: overview_mcc134.inc
.. include:: overview_mcc152.inc
.. include:: overview_mcc172.inc
