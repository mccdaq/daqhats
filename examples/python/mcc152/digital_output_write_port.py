#!/usr/bin/env python
"""
    MCC 152 Methods Demonstrated:
        mcc152.dio_reset
        mcc152.dio_output_write_port
        mcc152.dio_config_write_port
        mcc152.info

    Purpose:
        Write all digital outputs until terminated by the user.

    Description:
        This example demonstrates using the digital I/O as outputs and writing
        them as an entire port.
"""
from __future__ import print_function
import sys
from daqhats import mcc152, HatIDs, HatError, DIOConfigItem
from daqhats_utils import select_hat_device

def get_input():
    """
    Get a value from the user.
    """
    max_value = (1 << mcc152.info().NUM_DIO_CHANNELS)

    while True:
        # Wait for the user to enter a response
        message = "Enter the output value, non-numeric character to exit: "
        if sys.version_info.major > 2:
            response = input(message)
        else:
            response = raw_input(message)

        # Try to convert it to a number with automatic base conversion
        try:
            value = int(response, 0)
        except ValueError:
            return None
        else:
            # Compare the number to min and max allowed.
            if (value < 0) or (value >= max_value):
                # Out of range, ask again.
                print("Value out of range.")
            else:
                # Valid value.
                return value

def main():
    """
    This function is executed automatically when the module is run directly.
    """

    print("MCC 152 digital output write example.")
    print("Sets all digital I/O channels to outputs then gets values from")
    print("the user and updates the outputs. The value can be specified")
    print("as decimal (0 - 255,) hexadecimal (0x0 - 0xFF,)")
    print("octal (0o0 - 0o377,) or binary (0b0 - 0b11111111.)")
    print("   Methods demonstrated:")
    print("      mcc152.dio_reset")
    print("      mcc152.dio_output_write_port")
    print("      mcc152.dio_config_write_port")
    print("      mcc152.info")
    print()

    # Get an instance of the selected hat device object.
    address = select_hat_device(HatIDs.MCC_152)

    print("\nUsing address {}.\n".format(address))

    hat = mcc152(address)

    # Reset the DIO to defaults (all channels input, pull-up resistors
    # enabled).
    hat.dio_reset()

    # Set all channels as outputs.
    try:
        hat.dio_config_write_port(DIOConfigItem.DIRECTION, 0x00)
    except (HatError, ValueError):
        print("Could not configure the port as outputs.")
        sys.exit()

    run_loop = True
    error = False
    # Loop until the user terminates or we get a library error.
    while run_loop and not error:
        write_value = get_input()

        if write_value is None:
            run_loop = False
        else:
            try:
                hat.dio_output_write_port(write_value)
            except (HatError, ValueError):
                error = True

        if error:
            print("Error writing the outputs.")

    # Return the DIO to default settings
    if not error:
        hat.dio_reset()

if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
