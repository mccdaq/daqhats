# MCC DAQ HAT Library for Raspberry Pi
<table>
    <tr><td>Info</td><td>Contains C and Python Libraries for interacting with
    Measurement Computing DAQ HAT boards.</td></tr>
    <tr><td>Author</td><td>Measurement Computing</td></tr>
    <tr><td>Library Version<td>1.5.0.0</td></tr>
</table>

## About
This is the development repository for Measurement Computing DAQ HAT boards. The
**daqhats** library was created and is supported by Measurement Computing Corporation (MCC).

#### Supported MCC DAQ HAT hardware
Hardware supported by this version of the MCC DAQ HAT Library:
- [MCC 118](https://mccdaq.github.io/daqhats/overview.html#mcc-118)
- [MCC 128](https://mccdaq.github.io/daqhats/overview.html#mcc-128)
- [MCC 134](https://mccdaq.github.io/daqhats/overview.html#mcc-134)
- [MCC 152](https://mccdaq.github.io/daqhats/overview.html#mcc-152)
- [MCC 172](https://mccdaq.github.io/daqhats/overview.html#mcc-172)

#### Hardware Compatibility
The MCC DAQ HATs are compatible with all Raspberry Pi models with the 40-pin
GPIO header (not the original Pi 1 A or B with the 26-pin header.) They are
generally not compatible with any other brand of Raspberry Pi HAT or add-on
board that attaches to the GPIO header, or devices that use the Raspberry Pi
SPI interface.

In particular, LCD displays that use the GPIO header (not HDMI) usually use the
SPI interface and will prevent the DAQ HATs from working. Even if the display is
removed, the driver is probably still loaded by /boot/config.txt and will cause
issues with the DAQ HATs. If you have a problem with your device and have used a
GPIO header display with your Raspberry Pi then consult your display hardware
documentation for how to remove the driver.

The specific pins used by each DAQ HAT are documented in the electrical
specifications for that device.

## Prerequisites
- Raspberry Pi OS or Raspbian image (may work with other Raspberry Pi operating systems)
- Raspberry Pi with 40-pin GPIO header
- C, C++, Python 2.7 or Python 3.4+

## Raspberry Pi Configuration
Follow the instructions at https://www.raspberrypi.org/help/ for setting up a Raspberry Pi.

## Install Instructions
1. Power off the Raspberry Pi and attach one or more DAQ HAT boards, using unique
   address settings for each. Refer to
   [Installing the HAT board](https://mccdaq.github.io/daqhats/hardware.html)
   for detailed information.
   When using a single board, leave it at address 0 (all address jumpers removed.)
   One board must always be at address 0 to ensure that the OS reads a HAT EEPROM
   and initializes the hardware correctly.
2. Power on the Pi, log in, and open a terminal window (if using the graphical interface.)
3. Update your package list:

   ```sh
   sudo apt update
   ```
4. **Optional:** Update your installation packages and reboot:

   ```sh
   sudo apt full-upgrade
   sudo reboot
   ```
5. Install git (if not installed):

   ```sh
   sudo apt install git -y
   ```
6. Download the daqhats library to the root of your home folder:

   ```sh
   cd ~
   git clone https://github.com/mccdaq/daqhats.git
   ```
7. Build and install the shared library and tools. It will also detect the HAT board EEPROMs 
and save the contents, if needed.

   ```sh
   cd ~/daqhats
   sudo ./install.sh
   ```
8. To use the daqhats library with Python install the python library.  It may be 
   installed system-wide with:
   ```sh
   sudo pip install daqhats
   ```
   Recent versions of Python discourage system-wide installation, so you may have
   to append *--break-system-packages*.  To install in a virtual environment (venv),
   create the venv and install the package (replace `<path_to_venv>` with the desired
   location of the venv):
   ```sh
   python -m venv <path_to_venv>
   <path_to_venv>/bin/pip install daqhats
   ```
**Note:** If you encounter any errors during steps 5 - 7 then uininstall the daqhats
library (if installed), go back to step 4, update your installed packages and reboot,
then repeat steps 5 - 7.

If ioctl errors are seen when running on a Raspberry Pi 5, update the kernel with:
```sh
   sudo rpi-update
```

You can now run the example programs under ~/daqhats/examples and create your own
programs. Refer to the [Examples](#examples) section below for more information.
The Python library must be installed to run the Python examples.

If you are using the Raspberry Pi OS desktop interface, the DAQ HAT Manager utility will be
available under the Accessories start menu. This utility will allow you to list the
detected DAQ HATs, update the EEPROM files if you change your board stack, and launch
control applications for each DAQ HAT to perform simple operations. The code for these
programs is in the daqhats/tools/applications directory.

#### List the installed boards
You can use the DAQ HAT Manager or the **daqhats_list_boards** command to display a
list of the detected MCC DAQ HATs.  This list is generated from the EEPROM images, so
it will not be correct if you change the board stack without updating the EEPROM
images (see below.)

#### Update the EEPROM images
If you change your board stack, you must update the saved EEPROM images so that
the library has the correct board information. You can use the DAQ HAT Manager or the
command:

```sh
sudo daqhats_read_eeproms
```

#### Display the installed daqhats version
The command **daqhats_version** may be used to display the installed version number.

#### Uninstall the daqhats library
To uninstall the the daqhats library:

```sh
cd ~/daqhats
sudo ./uninstall.sh
```

To uninstall a system-wide Python library:

```sh
sudo pip uninstall daqhats
```

To uninstall the Python library from a venv (replace `<path_to_venv>` with the path of the venv):

```sh
<path_to_venv>/bin/pip uninstall daqhats
```

## Firmware Updates
The library firmware version for applicable devices can be found in
[tools/README.md](./tools/README.md).

#### MCC 118
Use the firmware update tool to update the firmware on your MCC 118 board(s).
The "0" in the example below is the board address. Repeat the command for each
MCC 118 address in your board stack. This example demonstrates how to update the
firmware on the MCC 118 that is installed at address 0.

```sh
mcc118_firmware_update 0 ~/daqhats/tools/MCC_118.hex
```
#### MCC 128
```sh
mcc128_firmware_update 0 ~/daqhats/tools/MCC_128.fw
```
#### MCC 172
```sh
mcc172_firmware_update 0 ~/daqhats/tools/MCC_172.fw
```
## Examples
The daqhats library includes example programs developed with C/C++ and Python.
The examples are available under ~/daqhats/examples, and are provided in the
following formats:

- console-based (C/C++ and Python)
- User interface
  - Web server (Python)
  - IFTTT (If This Then That) trigger service (Python)
  - Data logger (C/C++)

Refer to the README.md file in each example folder for more information.

To run the Python examples when you have installed the library in a virtual environment
you must explicitly call the venv Python binary in this manner:
```sh
cd ~/daqhats/examples/python/mcc128
<path_to_venv>/bin/python finite_scan.py
```
## Usage
The following is a basic Python example demonstrating how to read MCC 118 voltage
inputs and display channel values.

```python
#!/usr/bin/env python
#
# MCC 118 example program
# Read and display analog input values
#
import sys
from daqhats import hat_list, HatIDs, mcc118

# get hat list of MCC daqhat boards
board_list = hat_list(filter_by_id = HatIDs.ANY)
if not board_list:
    print("No boards found")
    sys.exit()

# Read and display every channel
for entry in board_list:
    if entry.id == HatIDs.MCC_118:
        print("Board {}: MCC 118".format(entry.address))
        board = mcc118(entry.address)
        for channel in range(board.info().NUM_AI_CHANNELS):
            value = board.a_in_read(channel)
            print("Ch {0}: {1:.3f}".format(channel, value))
```

## Support/Feedback
The **daqhats** library is supported by MCC. Obtain technical support through
our [support forum](https://forum.digilent.com/forum/39-measurement-computing-mcc/).

## Documentation
Documentation for the daqhats library is available at
https://mccdaq.github.io/daqhats/index.html.
