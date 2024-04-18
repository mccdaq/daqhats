********************************
Installing and Using the Library
********************************

The project is hosted at https://github.com/mccdaq/daqhats.

Installation
************

1. Power off the Raspberry Pi then attach one or more HAT boards (see :ref:`install`).
2. Power on the Pi and log in.  Open a terminal window if using the graphical interface.
3. Update your package list::

    sudo apt update
    
4. **Optional:** Update your installed packages and reboot::
   
    sudo apt full-upgrade
    sudo reboot
    
5. Install git (if not installed)::

    sudo apt install git
    
6. Download this package to your user folder with git::

    cd ~
    git clone https://github.com/mccdaq/daqhats.git
    
7. Build and install the shared library and tools.  The installer will detect the HAT board EEPROMs and save the contents if needed::

    cd ~/daqhats
    sudo ./install.sh
    
8. To use the daqhats library with Python install the python library. It may be installed system-wide with::

    sudo pip install daqhats
    
   Recent versions of Python discourage system-wide installation, so you may have to append ``--break-system-packages``. 
   To install in a virtual environment (venv), create the venv and install the package (replace ``<path_to_venv>`` with the 
   desired location of the venv)::
    
    python -m venv <path_to_venv>
    <path_to_venv>/bin/pip install daqhats
    
**Note:** If you encounter any errors during steps 5 - 7 then uininstall the daqhats
library (if installed), go back to step 4, update your installed packages and reboot, 
then repeat steps 5 - 7.
    
if ioctl errors are seen when running on a Raspberry Pi 5, update the kernel with::

    sudo rpi-update

You can now run the example programs under ~/daqhats/examples and create your own programs.

If you are using the Raspberry Pi OS desktop interface, the DAQ HAT Manager utility will be
available under the Accessories start menu. This utility will allow you to list the
detected DAQ HATs, update the EEPROM files if you change your board stack, and launch
control applications for each DAQ HAT to perform simple operations. The code for these
programs is in the daqhats/tools/applications directory.

You may display a list of the detected boards at any time with the DAQ HAT Manager or
the command::

    daqhats_list_boards

If you change your board stackup and have more than one HAT board attached you must
update the saved EEPROM images for the library to have the correct board information.
You can use the DAQ HAT Manager or the command::

    sudo daqhats_read_eeproms
    
To display the installed daqhats version use::

    daqhats_version
    
To uninstall the package use::

    cd ~/daqhats
    sudo ./uninstall.sh
    

Firmware Updates
****************

MCC 118
-------
Use the firmware update tool to update the firmware on your MCC 118 board(s).
The "0" in the example below is the board address. Repeat the command for each
MCC 118 address in your board stack. This example demonstrates how to update the
firmware on the MCC 118 that is installed at address 0::

    mcc118_firmware_update 0 ~/daqhats/tools/MCC_118.hex

Creating a C program
********************

- The daqhats headers are installed in /usr/local/include/daqhats.  Add the compiler option :code:`-I/usr/local/include` in order to find the header files when compiling, and the include line :code:`#include <daqhats/daqhats.h>` to your source code.
- The shared library, libdaqhats.so, is installed in /usr/local/lib.  Add the linker option :code:`-ldaqhats` to include this library.
- Study the example programs, example makefile, and library documentation for more information.

Creating a Python program
*************************

- The Python package is named *daqhats*.  Use it in your code with :code:`import daqhats`.
- Study the example programs and library documentation for more information.
