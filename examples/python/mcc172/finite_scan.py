#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
    MCC 172 Functions Demonstrated:
        mcc172.a_in_scan_start
        mcc172.a_in_scan_read

    Purpose:
        Perform a finite acquisition on 1 or more channels.

    Description:
        Acquires blocks of analog input data for a user-specified group
        of channels.  The last sample of data for each channel is
        displayed for each block of data received from the device.  The
        acquisition is stopped when the specified number of samples is
        acquired for each channel.

"""
from __future__ import print_function
from time import sleep
from sys import stdout, version_info
from daqhats import mcc172, SourceType, OptionFlags, HatIDs, HatError
from daqhats_utils import select_hat_device, enum_mask_to_string, \
chan_list_to_mask

CURSOR_BACK_2 = '\x1b[2D'
ERASE_TO_END_OF_LINE = '\x1b[0K'

def main():
    """
    This function is executed automatically when the module is run directly.
    """

    # Store the channels in a list and convert the list to a channel mask that
    # can be passed as a parameter to the MCC 172 functions.
    channels = [0, 1]
    channel_mask = chan_list_to_mask(channels)
    num_channels = len(channels)

    samples_per_channel = 10240
    scan_rate = 10240.0
    options = OptionFlags.DEFAULT

    try:
        # Select an MCC 172 HAT device to use.
        #address = select_hat_device(HatIDs.MCC_172)
        address = 3
        hat = mcc172(address)

        print('\nSelected MCC 172 HAT device at address', address)

        # Turn on IEPE power if desired.
        message = '\nEnable IEPE power [y or n]? '
        if version_info.major > 2:
            str_v = input(message)
        else:
            str_v = raw_input(message)

        if (str_v == 'y') or (str_v == 'Y'):
            iepe_enable = 1
        elif (str_v == 'n') or (str_v == 'N'):
            iepe_enable = 0
        else:
            print('Error: Invalid selection')
            return

        for channel in channels:
            hat.iepe_config_write(channel, iepe_enable)

        # Configure the ADC clock.
        hat.a_in_clock_config_write(SourceType.LOCAL, scan_rate)
    
        # Wait for the ADCs to synchronize.
        synced = False
        while not synced:
            status = hat.a_in_clock_config_read()
            synced = status.synced
            sleep(0.01)

        actual_scan_rate = status.sample_rate_per_channel
        
        print('\nMCC 172 continuous scan example')
        print('    Functions demonstrated:')
        print('         mcc172.a_in_scan_start')
        print('         mcc172.a_in_scan_read')
        if iepe_enable == 1:
            print('    IEPE power: on')
        else:
            print('    IEPE power: off')
        print('    Channels: ', end='')
        print(', '.join([str(chan) for chan in channels]))
        print('    Requested scan rate: ', scan_rate)
        print('    Actual scan rate: ', actual_scan_rate)
        print('    Samples per channel: ', samples_per_channel)
        print('    Options: ', enum_mask_to_string(OptionFlags, options))

        try:
            input('\nPress ENTER to continue ...')
        except (NameError, SyntaxError):
            pass

        # Configure and synchronize the ADC clock.
        
        # Configure and start the scan.
        hat.a_in_scan_start(channel_mask, samples_per_channel, options)

        print('Starting scan ... Press Ctrl-C to stop\n')

        # Display the header row for the data table.
        print('Samples Read    Scan Count', end='')
        for chan in channels:
            print('    Channel ', chan, sep='', end='')
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
    read_request_size = -1
    timeout = 5.0

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

        # Display the last sample for each channel.
        print('\r{:12}'.format(samples_read_per_channel),
              ' {:12} '.format(total_samples_read), end='')

        if samples_read_per_channel > 0:
            index = samples_read_per_channel * num_channels - num_channels

            for i in range(num_channels):
                print('{:10.5f}'.format(read_result.data[index + i]), 'V ',
                      end='')
            stdout.flush()

        sleep(0.001)

    print('\n')


if __name__ == '__main__':
    main()
