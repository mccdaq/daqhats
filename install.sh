#!/bin/bash

if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root, i.e 'sudo ./install.sh'" 1>&2
   exit 1
fi

# Ensure required libraries are installed
apt satisfy gpiod libgpiod-dev libgtk-3-dev -y 2> /dev/null
if [ $? -ne 0 ]; then
   apt install gpiod libgpiod-dev libgtk-3-dev -y
   if [ $? -ne 0 ]; then
      echo "Error installing required libraries"
      exit 1
   fi
fi

# Build / install the C library and headers
echo "Building and installing library"
echo
make -C lib all
if [ $? -ne 0 ]; then
   echo "Library build failed"
   exit 1
fi
make -C lib install
if [ $? -ne 0 ]; then
   echo "Library install failed"
   exit 1
fi
make -C lib clean

echo

# Build / install tools
echo "Building and installing tools"
echo
make -C tools all
if [ $? -ne 0 ]; then
   echo "Tools build failed"
   exit 1
fi
make -C tools install
if [ $? -ne 0 ]; then
   echo "Tools install failed"
   exit 1
fi
make -C tools clean

echo

# Build examples
echo "Building examples"
echo
make -C examples/c all
if [ $? -ne 0 ]; then
   echo "Examples build failed"
   exit 1
fi

echo

# Read HAT EEPROMs to /etc/mcc/hats
echo "Reading DAQ HAT EEPROMs"
echo
daqhats_read_eeproms

echo

# Turn SPI on (needed for some MCC 118s that had incorrectly programmed EEPROMs)
if [ $(raspi-config nonint get_spi) -eq 1 ]; then
   raspi-config nonint do_spi 0
fi

echo "Shared library install complete."
echo "The Python library is not automatically installed. See README.md for instructions to install the Python library."
