#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
    MCC 172 Functions Demonstrated:
        mcc172.iepe_config_write
        mcc172.a_in_clock_config_write
        mcc172.a_in_clock_config_read
        mcc172.a_in_sensitivity_write
        mcc172.a_in_scan_start
        mcc172.a_in_scan_read
        mcc172.a_in_scan_stop

    Purpose:
        Perform a finite acquisition on 1 or more channels.

    Description:
        Acquires blocks of analog input data for a user-specified group
        of channels.  The RMS value for each channel is displayed for each
        block of data received from the device.  The acquisition is stopped
        when the specified number of samples is acquired for each channel.

"""
from __future__ import print_function
from time import sleep
from sys import stdout, version_info
from math import sqrt
from daqhats import mcc172, OptionFlags, SourceType, HatIDs, HatError
from daqhats_utils import select_hat_device, enum_mask_to_string, \
chan_list_to_mask

CURSOR_BACK_2 = '\x1b[2D'
ERASE_TO_END_OF_LINE = '\x1b[0K'

def get_iepe():
    """
    Get IEPE enable from the user.
    """

    while True:
        # Wait for the user to enter a response
        message = "IEPE enable [y or n]?  "
        if version_info.major > 2:
            response = input(message)
        else:
            response = raw_input(message)

        # Check for valid response
        if (response == "y") or (response == "Y"):
            return 1
        elif (response == "n") or (response == "N"):
            return 0
        else:
            # Ask again.
            print("Invalid response.")

def main(): # pylint: disable=too-many-locals, too-many-statements
    """
    This function is executed automatically when the module is run directly.
    """

    # Store the channels in a list and convert the list to a channel mask that
    # can be passed as a parameter to the MCC 172 functions.
    channels = [0, 1]
    channel_mask = chan_list_to_mask(channels)
    num_channels = len(channels)

    sensitivity = 1000.0
    samples_per_channel = 10000
    scan_rate = 10240.0
    options = OptionFlags.DEFAULT

    try:
        # Select an MCC 172 HAT device to use.
        address = select_hat_device(HatIDs.MCC_172)
        hat = mcc172(address)

        print('\nSelected MCC 172 HAT device at address', address)

        # Turn on IEPE supply?
        iepe_enable = get_iepe()

        for channel in channels:
            hat.iepe_config_write(channel, iepe_enable)
            hat.a_in_sensitivity_write(channel, sensitivity)

        # Configure the clock and wait for sync to complete.
        hat.a_in_clock_config_write(SourceType.LOCAL, scan_rate)

        synced = False
        while not synced:
            (_source_type, actual_scan_rate, synced) = hat.a_in_clock_config_read()
            if not synced:
                sleep(0.005)

        print('\nMCC 172 continuous scan example')
        print('    Functions demonstrated:')
        print('         mcc172.iepe_config_write')
        print('         mcc172.a_in_clock_config_write')
        print('         mcc172.a_in_clock_config_read')
        print('         mcc172.a_in_sensitivity_write')
        print('         mcc172.a_in_scan_start')
        print('         mcc172.a_in_scan_read')
        print('         mcc172.a_in_scan_stop')
        print('         mcc172.a_in_scan_cleanup')
        print('    IEPE power: ', end='')
        if iepe_enable == 1:
            print('on')
        else:
            print('off')
        print('    Channels: ', end='')
        print(', '.join([str(chan) for chan in channels]))
        print('    Sensitivity: ', sensitivity)
        print('    Requested scan rate: ', scan_rate)
        print('    Actual scan rate: ', actual_scan_rate)
        print('    Samples per channel', samples_per_channel)
        print('    Options: ', enum_mask_to_string(OptionFlags, options))

        try:
            input('\nPress ENTER to continue ...')
        except (NameError, SyntaxError):
            pass

        # Configure and start the scan.
        hat.a_in_scan_start(channel_mask, samples_per_channel, options)

        print('Starting scan ... Press Ctrl-C to stop\n')

        # Display the header row for the data table.
        print('Samples Read    Scan Count', end='')
        for chan in channels:
            print('      Ch ', chan, ' RMS', sep='', end='')
        print('')

        try:
            read_and_display_data(hat, samples_per_channel, num_channels)

        except KeyboardInterrupt:
            # Clear the '^C' from the display.
            print(CURSOR_BACK_2, ERASE_TO_END_OF_LINE, '\n')
            hat.a_in_scan_stop()

        hat.a_in_scan_cleanup()

    except (HatError, ValueError) as err:
        print('\n', err)

def calc_rms(data, channel, num_channels, num_samples_per_channel):
    """ Calculate RMS value from a block of samples. """
    value = 0.0
    index = channel
    for _i in range(num_samples_per_channel):
        value += (data[index] * data[index]) / num_samples_per_channel
        index += num_channels

    return sqrt(value)

def read_and_display_data(hat, samples_per_channel, num_channels):
    """
    Reads data from the specified channels on the specified DAQ HAT devices
    and updates the data on the terminal display.  The reads are executed in a
    loop that continues until either the scan completes or an overrun error
    is detected.

    Args:
        hat (mcc172): The mcc172 HAT device object.
        samples_per_channel: The number of samples to read for each channel.
        num_channels (int): The number of channels to display.

    Returns:
        None

    """
    total_samples_read = 0
    read_request_size = 1000
    timeout = 5.0

    # Since the read_request_size is set to a specific value, a_in_scan_read()
    # will block until that many samples are available or the timeout is
    # exceeded.

    # Continuously update the display value until Ctrl-C is
    # pressed or the number of samples requested has been read.
    while total_samples_read < samples_per_channel:
        read_result = hat.a_in_scan_read(read_request_size, timeout)

        # Check for an overrun error
        if read_result.hardware_overrun:
            print('\n\nHardware overrun\n')
            break
        elif read_result.buffer_overrun:
            print('\n\nBuffer overrun\n')
            break

        samples_read_per_channel = int(len(read_result.data) / num_channels)
        total_samples_read += samples_read_per_channel

        print('\r{:12}'.format(samples_read_per_channel),
              ' {:12}'.format(total_samples_read), end='')

        # Display the RMS voltage for each channel.
        if samples_read_per_channel > 0:
            for i in range(num_channels):
                value = calc_rms(read_result.data, i, num_channels,
                                 samples_read_per_channel)
                print('{:14.5f}'.format(value), end='')
            stdout.flush()

    print('\n')


if __name__ == '__main__':
    main()
