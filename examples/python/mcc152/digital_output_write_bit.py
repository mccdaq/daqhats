#!/usr/bin/env python
"""
    MCC 152 Methods Demonstrated:
        mcc152.dio_reset
        mcc152.dio_output_write_bit
        mcc152.dio_config_write_bit
        mcc152.info

    Purpose:
        Write individual digital outputs until terminated by the user.

    Description:
        This example demonstrates using the digital I/O as outputs and writing
        them individually.
"""
from __future__ import print_function
import sys
from daqhats import mcc152, HatIDs, HatError, DIOConfigItem
from daqhats_utils import select_hat_device

def get_channel():
    """
    Get a channel number from the user.
    """
    num_channels = mcc152.info().NUM_DIO_CHANNELS

    while True:
        # Wait for the user to enter a response
        message = "Enter a channel between 0 and {},".format(num_channels - 1)
        message += " non-numeric character to exit: "
        if sys.version_info.major > 2:
            response = input(message)
        else:
            response = raw_input(message)

        # Try to convert it to a number.
        try:
            value = int(response)
        except ValueError:
            return None
        else:
            # Compare the number to min and max allowed.
            if (value < 0) or (value >= num_channels):
                # Out of range, ask again.
                print("Value out of range.")
            else:
                # Valid value.
                return value

def get_value():
    """
    Get a value from the user.
    """
    while True:
        # Wait for the user to enter a response
        message = ("Enter the output value, 0 or 1, non-numeric character to "
                   "exit: ")
        if sys.version_info.major > 2:
            response = input(message)
        else:
            response = raw_input(message)

        # Try to convert it to a number.
        try:
            value = int(response)
        except ValueError:
            return None
        else:
            # Compare the number to min and max allowed.
            if (value < 0) or (value > 1):
                # Out of range, ask again.
                print("Value out of range.")
            else:
                # Valid value.
                return value

def get_input():
    """
    Get a channel and value from the user.
    """
    channel = get_channel()
    if channel is None:
        return (None, None)

    value = get_value()
    if value is None:
        return (None, None)

    print()
    return (channel, value)

def main():
    """
    This function is executed automatically when the module is run directly.
    """

    print("MCC 152 digital output write example.")
    print("Sets all digital I/O channels to output then gets channel and")
    print("value input from the user and updates the output.")
    print("   Methods demonstrated:")
    print("      mcc152.dio_reset")
    print("      mcc152.dio_output_write_bit")
    print("      mcc152.dio_config_write_bit")
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
    for channel in range(mcc152.info().NUM_DIO_CHANNELS):
        try:
            hat.dio_config_write_bit(channel, DIOConfigItem.DIRECTION, 0)
        except (HatError, ValueError):
            print("Could not configure the channel as output.")
            sys.exit()

    run_loop = True
    error = False
    # Loop until the user terminates or we get a library error.
    while run_loop and not error:
        channel, value = get_input()

        if channel is None:
            run_loop = False
        else:
            try:
                hat.dio_output_write_bit(channel, value)
            except (HatError, ValueError):
                error = True

        if error:
            print("Error writing the output.")

    # Return the DIO to default settings
    if not error:
        hat.dio_reset()

if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
