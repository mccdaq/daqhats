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
The MCC DAQ HATs are generally not compatible with any other brand of Raspberry
Pi HAT or add-on board that attaches to the GPIO header, or devices that use the
Raspberry Pi SPI interface. In particular, LCD displays that use the GPIO header
usually use SPI and will prevent the DAQ HATs from working. Even if the display
is removed, the driver is probably still loaded by /boot/config.txt and will
prevent the DAQ HATs from working. If you have a problem with your device and
have used a display with your Raspberry Pi then check /boot/config.txt for display
drivers, remove them, and reboot.

.. include:: overview_mcc118.inc
.. include:: overview_mcc134.inc
.. include:: overview_mcc152.inc
