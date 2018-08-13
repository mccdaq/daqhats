# MCC DAQ HAT Library for Raspberry Pi
<table>
    <tr><td>Info</td><td>Contains C and Python Libraries for interacting with 
    Measurement Computing DAQ HAT boards.</td></tr>
    <tr><td>Author</td><td>Measurement Computing</td></tr>   
    <tr><td>Library Version<td>1.0.0</td></tr>
</table>

## About
This is the repository for Measurement Computing DAQ HAT boards. The
**daqhats** library was created and is supported by Measurement Computing Corporation (MCC).

#### Supported MCC DAQ HAT hardware
Hardware supported by this version of the MCC DAQ HAT Library:
- [MCC 118](https://mccdaq.github.io/daqhats/overview.html#mcc-118)

## Prerequisites
- Raspbian or Raspbian Lite image (may work with other Raspberry Pi operating systems)
- Raspberry Pi A+, B+, 2, or 3
- C, C++, Python 2.7 or Python 3.4

## Raspberry Pi Configuration
Follow the instructions at https://www.raspberrypi.org/help/ for setting up a Raspberry Pi.

## Build Instructions
1. Power off the Raspberry Pi and attach one or more DAQ HAT boards, using unique 
   address settings for each. Refer to 
   [Installing the HAT board](https://mccdaq.github.io/daqhats/hardware.html) 
   for detailed information.
   When using a single board, leave it at address 0 (all address jumpers removed.) 
   One board must always be at address 0 to ensure that the OS reads a HAT EEPROM
   and initializes the hardware correctly.
2. Power on the Pi, log in, and open a terminal window (if using the graphical interface.)
3. Update your installation packages and install git (if not installed):

   ```sh
   sudo apt-get update
   sudo apt-get install git
   ```
4. Download the daqhats library to the root of your home folder:

   ```sh
   cd ~
   git clone https://github.com/mccdaq/daqhats.git
   ```
5. Build and install the shared library, tools, and optional Python support. The 
   installer will ask if you want to install Python 2 and Python 3 support. It 
   will also detect the HAT board EEPROMs and save the contents, if needed.

   ```sh
   cd ~/daqhats
   sudo ./install.sh
   ```   
6. [Optional] Use the firmware update tool to update the firmware on your MCC HAT 
   board. The "0" in the example below is the board address. Repeat the command for
   each MCC 118 address in your board stack. This example demonstrates how to update 
   the firmware on the MCC 118 HAT that is installed at address 0.

   ```sh
   mcc118_firmware_update 0 ~/daqhats/tools/MCC_118.hex
   ```
You can now run the example programs under ~/daqhats/examples and create your own 
programs. Refer to the [Examples](#examples) section below for more information.

#### List the installed boards
You can use the tool **daqhats_list_boards** to display a list of the detected 
MCC DAQ HATs.  This list is generated from the EEPROM images, so it will not be 
correct if you change the board stack without updating the EEPROM images 
(see below.)

#### Update the EEPROM images
If you change your board stack, you must update the saved EEPROM images so that 
the library has the correct board information:

```sh
sudo daqhats_read_eeproms
```
#### Uninstall the daqhats library
If you want to uninstall the the daqhats library, use the following commands:

```sh
cd ~/daqhats
sudo ./uninstall.sh
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
import daqhats as hats

# get hat list of MCC HAT boards
list = hats.hat_list(filter_by_id = hats.HatIDs.ANY)
if not list:
    print("No boards found")
    sys.exit()

# Read and display every channel
for entry in list: 
if entry.id == hats.HatIDs.MCC_118:
    print("Board {}: MCC 118".format(entry.address))
    board = hats.mcc118(entry.address)
    for channel in range(board.info().NUM_AI_CHANNELS):
        value = board.a_in_read(channel)
        print("Ch {0}: {1:.3f}".format(channel, value))	
```
    
## Support/Feedback
The **daqhats** library is supported by MCC. Contact technical support through 
our [support page](https://www.mccdaq.com/support/support_form.aspx).

## Documentation 
Documentation for the daqhats library is available at 
https://mccdaq.github.io/daqhats/index.html.
