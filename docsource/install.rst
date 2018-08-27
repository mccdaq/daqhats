********************************
Installing and Using the Library
********************************

The project is hosted at https://github.com/mccdaq/daqhats.

Installation
************

1. Power off the Raspberry Pi then attach one or more HAT boards (see :ref:`install`).
2. Power on the Pi and log in.  Open a terminal window if using the graphical interface.
3. Update your package list::

    sudo apt-get update
    
4. **Optional:** If you get errors in the steps that follow, upgrade your installation packages,
   reboot, and try again::
   
    sudo apt-get dist-upgrade
    sudo reboot
    
5. Install git (if not installed)::

    sudo apt-get install git
    
6. Download this package to your user folder with git::

    cd ~
    git clone https://github.com/mccdaq/daqhats.git
    
7. Build and install the shared library and optional Python support.  The installer will ask if you want to install Python 2 and Python 3 support.  It will also detect the HAT board EEPROMs and save the contents if needed::

    cd ~/daqhats
    sudo ./install.sh

8. **Optional:** Use the firmware update tool to update the firmware on your MCC 118
   board(s). The "0" in the example below is the board address. Repeat the command for
   each MCC 118 address in your board stack. This example demonstrates how to update 
   the firmware on the MCC 118 that is installed at address 0::

    mcc118_firmware_update 0 ~/daqhats/tools/MCC_118.hex
    
You can now run the example programs under ~/daqhats/examples and create your own programs.

To uninstall the package use::

    cd ~/daqhats
    sudo ./uninstall.sh
    
If you change your board stackup and have more than one HAT board attached you must update the saved EEPROM images for the library to have the correct board information::

    sudo daqhats_read_eeproms
    
You may display a list of the detected boards at any time with::

    daqhats_list_boards

Creating a C program
********************

- The daqhats headers are installed in /usr/local/include/daqhats.  Add the compiler option :code:`-I/usr/local/include` in order to find the header files when compiling, and the include line :code:`#include <daqhats/daqhats.h>` to your source code.
- The shared library, libdaqhats.so, is installed in /usr/local/lib.  Add the linker option :code:`-ldaqhats` to include this library.
- Study the example programs, example makefile, and library documentation for more information.

Creating a Python program
*************************

- The Python package is named *daqhats*.  Use it in your code with :code:`import daqhats`.
- Study the example programs and library documentation for more information.
