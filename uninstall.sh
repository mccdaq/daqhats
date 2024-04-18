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

echo "Shared library uninstall complete."
echo "The Python library is not automatically uninstalled. See README.md for instructions to uninstall the Python library."
