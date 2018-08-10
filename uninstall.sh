#!/bin/bash

if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root, i.e 'sudo ./uninstall.sh'" 1>&2
   exit 1
fi

# Remove the C library and headers
echo "Removing shared library"
echo
make -C lib uninstall
echo

# Removed compiled examples
echo "Removing compiled examples"
echo
make -C examples/c clean
echo

# Remove tools
echo "Removing tools"
echo
make -C tools uninstall
echo

# Remove EEPROM images
echo "Removing EEPROM images"
rm -rf /etc/mcc/hats
echo

# Remove the Python packages

pip2 show daqhats > /dev/null
if [ "$?" -eq 0 ]; then
    echo "Removing Python 2 package"
    pip2 uninstall daqhats -y
    echo
fi

pip3 show daqhats > /dev/null
if [ "$?" -eq 0 ]; then
    echo "Removing Python 3 package"
    pip3 uninstall daqhats -y
    echo
fi

echo "Uninstall complete. Remove this folder to completely remove daqhats."
