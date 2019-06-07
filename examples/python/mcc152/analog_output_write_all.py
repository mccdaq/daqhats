#!/usr/bin/env python
"""
    MCC 152 Methods Demonstrated:
        mcc152.a_out_write_all
        mcc152.info

    Purpose:
        Write values to both analog outputsin a loop.

    Description:
        This example demonstrates writing output data to both outputs
        simultaneously.
"""
from __future__ import print_function
from sys import version_info
from daqhats import mcc152, OptionFlags, HatIDs, HatError
from daqhats_utils import select_hat_device

# Get the min and max voltage values for the analog outputs to validate
# the user input.
MIN_V = mcc152.info().AO_MIN_RANGE
MAX_V = mcc152.info().AO_MAX_RANGE
NUM_CHANNELS = mcc152.info().NUM_AO_CHANNELS

def get_channel_value(channel):
    """
    Get the voltage from the user and validate it.
    """

    if channel not in range(NUM_CHANNELS):
        raise ValueError(
            "Channel must be in range 0 - {}.".format(NUM_CHANNELS - 1))

    while True:
        message = "   Channel {}: ".format(channel)

        if version_info.major > 2:
            str_v = input(message)
        else:
            str_v = raw_input(message)

        try:
            value = float(str_v)
        except ValueError:
            raise
        else:
            if (value < MIN_V) or (value > MAX_V):
                # Out of range, ask again.
                print("Value out of range.")
            else:
                # Valid value.
                return value

def get_input_values():
    """
    Get the voltages for both channels from the user.
    """

    while True:
        print("Enter voltages between {0:.1f} and {1:.1f}, non-numeric "
              "character to exit: ".format(MIN_V, MAX_V))
        try:
            values = [get_channel_value(channel) for channel in
                      range(NUM_CHANNELS)]
        except ValueError:
            raise
        else:
            # Valid values.
            return values

def main():
    """
    This function is executed automatically when the module is run directly.
    """
    options = OptionFlags.DEFAULT

    print('MCC 152 all channel analog output example.')
    print('Writes the specified voltages to the analog outputs.')
    print("   Methods demonstrated:")
    print("      mcc152.a_out_write_all")
    print("      mcc152.info")
    print()

    # Get an instance of the selected hat device object.
    address = select_hat_device(HatIDs.MCC_152)

    print("\nUsing address {}.\n".format(address))

    hat = mcc152(address)

    run_loop = True
    error = False
    while run_loop and not error:
        # Get the values from the user.
        try:
            values = get_input_values()
        except ValueError:
            run_loop = False
        else:
            # Write the values.
            try:
                hat.a_out_write_all(values=values, options=options)
            except (HatError, ValueError):
                error = True


if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
