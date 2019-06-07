#!/usr/bin/env python
"""
    MCC 152 Methods Demonstrated:
        mcc152.dio_input_read_port
        mcc152.dio_reset

    Purpose:
        Read all digital inputs in a single call until terminated by the user.

    Description:
        This example demonstrates using the digital I/O as inputs and reading
        them in a port read.
"""
from __future__ import print_function
from sys import version_info
from daqhats import mcc152, HatIDs, HatError
from daqhats_utils import select_hat_device

def main():
    """
    This function is executed automatically when the module is run directly.
    """

    print("MCC 152 digital input read example.")
    print("Reads the inputs as a port and displays their state.")
    print("   Methods demonstrated:")
    print("      mcc152.dio_reset")
    print("      mcc152.dio_input_read_port")
    print()

    # Get an instance of the selected hat device object.
    address = select_hat_device(HatIDs.MCC_152)

    print("\nUsing address {}.\n".format(address))

    hat = mcc152(address)

    # Reset the DIO to defaults (all channels input, pull-up resistors
    # enabled).
    hat.dio_reset()

    run_loop = True
    error = False
    # Loop until the user terminates or we get a library error.
    while run_loop and not error:
        # Read and display the inputs
        try:
            value = hat.dio_input_read_port()
        except (HatError, ValueError):
            error = True
            break
        else:
            print("Digital Inputs: 0x{0:02X}".format(value))

        if error:
            print("Error reading the inputs.")
        else:
            print("Enter Q to exit, anything else to read again: ")

            # Wait for the user to enter a response
            if version_info.major > 2:
                response = input("")
            else:
                response = raw_input("")

            if response == "q" or response == "Q":
                # Exit the loop
                run_loop = False

if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
