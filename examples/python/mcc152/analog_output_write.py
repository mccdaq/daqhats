#!/usr/bin/env python
"""
    MCC 152 Methods Demonstrated:
        mcc152.a_out_write
        mcc152.info

    Purpose:
        Write a single data value for channel 0 in a loop.

    Description:
        This example demonstrates writing output data using analog output 0.
"""
from __future__ import print_function
from sys import version_info
from daqhats import mcc152, OptionFlags, HatIDs, HatError
from daqhats_utils import select_hat_device

def get_input_value():
    """
    Get the voltage from the user and validate it.
    """

    # Get the min and max voltage values for the analog outputs to validate
    # the user input.
    min_v = mcc152.info().AO_MIN_RANGE
    max_v = mcc152.info().AO_MAX_RANGE

    while True:
        message = ("Enter a voltage between {0:.1f} and {1:.1f}, "
                   "non-numeric character to exit: ".format(min_v, max_v))

        if version_info.major > 2:
            str_v = input(message)
        else:
            str_v = raw_input(message)

        try:
            value = float(str_v)
        except ValueError:
            raise
        else:
            if (value < min_v) or (value > max_v):
                # Out of range, ask again.
                print("Value out of range.")
            else:
                # Valid value.
                return value

def main():
    """
    This function is executed automatically when the module is run directly.
    """
    options = OptionFlags.DEFAULT
    channel = 0
    num_channels = mcc152.info().NUM_AO_CHANNELS

    # Ensure channel is valid.
    if channel not in range(num_channels):
        error_message = ('Error: Invalid channel selection - must be '
                         '0 - {}'.format(num_channels - 1))
        raise Exception(error_message)

    print('MCC 152 single channel analog output example.')
    print('Writes the specified voltage to the analog output.')
    print("   Methods demonstrated:")
    print("      mcc152.a_out_write")
    print("      mcc152.info")
    print("   Channel: {}\n".format(channel))

    # Get an instance of the selected hat device object.
    address = select_hat_device(HatIDs.MCC_152)

    print("\nUsing address {}.\n".format(address))

    hat = mcc152(address)

    run_loop = True
    error = False
    while run_loop and not error:
        # Get the value from the user.
        try:
            value = get_input_value()
        except ValueError:
            run_loop = False
        else:
            # Write the value to the selected channel.
            try:
                hat.a_out_write(channel=channel,
                                value=value,
                                options=options)
            except (HatError, ValueError):
                error = True

    if error:
        print("Error writing analog output.")

if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
