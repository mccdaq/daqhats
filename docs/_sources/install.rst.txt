********************************
Installing and Using the Library
********************************

The project is hosted at https://github.com/mccdaq/daqhats.

Installation
************

1. Power off the Raspberry Pi then attach one or more HAT boards (see :ref:`install`).
2. Power on the Pi and log in.  Open a terminal window if using the graphical interface.
3. Upgrade your install packages::

    sudo apt-get update
    sudo apt-get dist-upgrade
    
4. If git is not already installed, install it::

    sudo apt-get install git
    
5. Download this package to your user folder with git::

    cd ~
    git clone https://github.com/mccdaq/daqhats
    
6. Build and install the shared library and optional Python support.  The installer will ask if you want to install Python 2 and Python 3 support.  It will also detect the HAT board EEPROMs and save the contents if needed::

    cd ~/daqhats
    sudo ./install.sh

7. [Optional] To update the firmware on your MCC 118 board(s) use the firmware update tool.  The "0" in the example below is the board address.  Repeat the command for each MCC 118 address in your board stack::

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
