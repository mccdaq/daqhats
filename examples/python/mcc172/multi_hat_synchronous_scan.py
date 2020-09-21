#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
    MCC 172 Functions Demonstrated:
        mcc172.trigger_config
        mcc172.iepe_config_write
        mcc172.a_in_clock_config_write
        mcc172.a_in_clock_config_read
        mcc172.a_in_scan_start
        mcc172.a_in_scan_read
        mcc172.a_in_scan_stop

    Purpose:
    Get synchronous data from multiple MCC 172 HAT devices.

    Description:
        This example demonstrates acquiring data synchronously from multiple
        MCC 172 HAT devices.  This is done using the shared clock and
        trigger options.  An external trigger source must be provided to the
        TRIG terminal on the master MCC 172 HAT device.  The EXTTRIGGER scan
        option is set on all devices.
"""
from __future__ import print_function
from sys import stdout, version_info
from math import sqrt
from time import sleep
from daqhats import (hat_list, mcc172, OptionFlags, HatIDs, TriggerModes,
                     HatError, SourceType)
from daqhats_utils import (enum_mask_to_string, chan_list_to_mask,
                           validate_channels)

# Constants
DEVICE_COUNT = 2
MASTER = 0
CURSOR_SAVE = "\x1b[s"
CURSOR_RESTORE = "\x1b[u"
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

def main(): # pylint: disable=too-many-locals, too-many-statements, too-many-branches
    """
    This function is executed automatically when the module is run directly.
    """
    hats = []
    # Define the channel list for each HAT device
    chans = [
        {0, 1},
        {0, 1}
    ]
    # Define the options for each HAT device
    options = [
        OptionFlags.EXTTRIGGER,
        OptionFlags.EXTTRIGGER
    ]
    samples_per_channel = 10000
    sample_rate = 10240.0  # Samples per second
    trigger_mode = TriggerModes.RISING_EDGE

    try:
        # Get an instance of the selected hat device object.
        hats = select_hat_devices(HatIDs.MCC_172, DEVICE_COUNT)

        # Validate the selected channels.
        for i, hat in enumerate(hats):
            validate_channels(chans[i], hat.info().NUM_AI_CHANNELS)

        # Turn on IEPE supply?
        iepe_enable = get_iepe()

        for i, hat in enumerate(hats):
            for channel in chans[i]:
                # Configure IEPE.
                hat.iepe_config_write(channel, iepe_enable)
            if hat.address() != MASTER:
                # Configure the slave clocks.
                hat.a_in_clock_config_write(SourceType.SLAVE, sample_rate)
                # Configure the trigger.
                hat.trigger_config(SourceType.SLAVE, trigger_mode)

        # Configure the master clock and start the sync.
        hats[MASTER].a_in_clock_config_write(SourceType.MASTER, sample_rate)
        synced = False
        while not synced:
            (_source_type, actual_rate, synced) = \
                hats[MASTER].a_in_clock_config_read()
            if not synced:
                sleep(0.005)

        # Configure the master trigger.
        hats[MASTER].trigger_config(SourceType.MASTER, trigger_mode)

        print('MCC 172 multiple HAT example using external clock and',
              'external trigger options')
        print('    Functions demonstrated:')
        print('         mcc172.trigger_mode')
        print('         mcc172.a_in_clock_config_write')
        print('         mcc172.a_in_clock_config_read')
        print('         mcc172.a_in_scan_start')
        print('         mcc172.a_in_scan_read')
        print('         mcc172.a_in_scan_stop')
        print('         mcc172.a_in_scan_cleanup')
        print('    IEPE power: ', end='')
        if iepe_enable == 1:
            print('on')
        else:
            print('off')
        print('    Samples per channel:', samples_per_channel)
        print('    Requested Sample Rate: {:.3f} Hz'.format(sample_rate))
        print('    Actual Sample Rate: {:.3f} Hz'.format(actual_rate))
        print('    Trigger type:', trigger_mode.name)

        for i, hat in enumerate(hats):
            print('    HAT {}:'.format(i))
            print('      Address:', hat.address())
            print('      Channels: ', end='')
            print(', '.join([str(chan) for chan in chans[i]]))
            options_str = enum_mask_to_string(OptionFlags, options[i])
            print('      Options:', options_str)

        print('\n*NOTE: Connect a trigger source to the TRIG input terminal on HAT 0.')

        try:
            input("\nPress 'Enter' to continue")
        except (NameError, SyntaxError):
            pass

        # Start the scan.
        for i, hat in enumerate(hats):
            chan_mask = chan_list_to_mask(chans[i])
            hat.a_in_scan_start(chan_mask, samples_per_channel, options[i])

        print('\nWaiting for trigger ... Press Ctrl-C to stop scan\n')

        try:
            # Monitor the trigger status on the master device.
            wait_for_trigger(hats[MASTER])
            # Read and display data for all devices until scan completes
            # or overrun is detected.
            read_and_display_data(hats, chans)

        except KeyboardInterrupt:
            # Clear the '^C' from the display.
            print(CURSOR_BACK_2, ERASE_TO_END_OF_LINE, '\nAborted\n')

    except (HatError, ValueError) as error:
        print('\n', error)

    finally:
        for hat in hats:
            hat.a_in_scan_stop()
            hat.a_in_scan_cleanup()


def wait_for_trigger(hat):
    """
    Monitor the status of the specified HAT device in a loop until the
    triggered status is True or the running status is False.

    Args:
        hat (mcc172): The mcc172 HAT device object on which the status will
            be monitored.

    Returns:
        None

    """
    # Read the status only to determine when the trigger occurs.
    is_running = True
    is_triggered = False
    while is_running and not is_triggered:
        status = hat.a_in_scan_status()
        is_running = status.running
        is_triggered = status.triggered

def calc_rms(data, channel, num_channels, num_samples_per_channel):
    """ Calculate RMS value from a block of samples. """
    value = 0.0
    index = channel
    for _i in range(num_samples_per_channel):
        value += (data[index] * data[index]) / num_samples_per_channel
        index += num_channels

    return sqrt(value)

def read_and_display_data(hats, chans):
    """
    Reads data from the specified channels on the specified DAQ HAT devices
    and updates the data on the terminal display.  The reads are executed in a
    loop that continues until either the scan completes or an overrun error
    is detected.

    Args:
        hats (list[mcc172]): A list of mcc172 HAT device objects.
        chans (list[int][int]): A 2D list to specify the channel list for each
            mcc172 HAT device.

    Returns:
        None

    """
    samples_to_read = 1000
    timeout = 5  # Seconds
    samples_per_chan_read = [0] * DEVICE_COUNT
    total_samples_per_chan = [0] * DEVICE_COUNT
    is_running = True

    # Since the read_request_size is set to a specific value, a_in_scan_read()
    # will block until that many samples are available or the timeout is
    # exceeded.

    # Create blank lines where the data will be displayed
    for _ in range(DEVICE_COUNT * 4 + 1):
        print('')
    # Move the cursor up to the start of the data display.
    print('\x1b[{0}A'.format(DEVICE_COUNT * 4 + 1), end='')
    print(CURSOR_SAVE, end='')

    while True:
        data = [None] * DEVICE_COUNT
        # Read the data from each HAT device.
        for i, hat in enumerate(hats):
            read_result = hat.a_in_scan_read(samples_to_read, timeout)
            data[i] = read_result.data
            is_running &= read_result.running
            samples_per_chan_read[i] = int(len(data[i]) / len(chans[i]))
            total_samples_per_chan[i] += samples_per_chan_read[i]

            if read_result.buffer_overrun:
                print('\nError: Buffer overrun')
                break
            if read_result.hardware_overrun:
                print('\nError: Hardware overrun')
                break

        print(CURSOR_RESTORE, end='')

        # Display the data for each HAT device
        for i, hat in enumerate(hats):
            print('HAT {0}:'.format(i))

            # Print the header row for the data table.
            print('  Samples Read    Scan Count', end='')
            for chan in chans[i]:
                print('     Channel', chan, end='')
            print('')

            # Display the sample count information.
            print('{0:>14}{1:>14}'.format(samples_per_chan_read[i],
                                          total_samples_per_chan[i]), end='')

            # Display the data for all selected channels
            #for chan_idx in range(len(chans[i])):
            #    sample_idx = ((samples_per_chan_read[i] * len(chans[i]))
            #                  - len(chans[i]) + chan_idx)
            #    print('{:>12.5f} V'.format(data[i][sample_idx]), end='')

            # Display the RMS voltage for each channel.
            if samples_per_chan_read[i] > 0:
                for channel in chans[i]:
                    value = calc_rms(data[i], channel, len(chans[i]),
                                     samples_per_chan_read[i])
                    print('{:10.5f}'.format(value), 'Vrms ',
                          end='')
                stdout.flush()
            print('\n')

        #stdout.flush()

        if not is_running:
            break


def select_hat_devices(filter_by_id, number_of_devices):
    """
    This function performs a query of available DAQ HAT devices and determines
    the addresses of the DAQ HAT devices to be used in the example.  If the
    number of HAT devices present matches the requested number of devices,
    a list of all mcc172 objects is returned in order of address, otherwise the
    user is prompted to select addresses from a list of displayed devices.

    Args:
        filter_by_id (int): If this is :py:const:`HatIDs.ANY` return all DAQ
            HATs found.  Otherwise, return only DAQ HATs with ID matching this
            value.
        number_of_devices (int): The number of devices to be selected.

    Returns:
        list[mcc172]: A list of mcc172 objects for the selected devices
        (Note: The object at index 0 will be used as the master).

    Raises:
        HatError: Not enough HAT devices are present.

    """
    selected_hats = []

    # Get descriptors for all of the available HAT devices.
    hats = hat_list(filter_by_id=filter_by_id)
    number_of_hats = len(hats)

    # Verify at least one HAT device is detected.
    if number_of_hats < number_of_devices:
        error_string = ('Error: This example requires {0} MCC 172 HATs - '
                        'found {1}'.format(number_of_devices, number_of_hats))
        raise HatError(0, error_string)
    elif number_of_hats == number_of_devices:
        for i in range(number_of_devices):
            selected_hats.append(mcc172(hats[i].address))
    else:
        # Display available HAT devices for selection.
        for hat in hats:
            print('Address ', hat.address, ': ', hat.product_name, sep='')
        print('')

        for device in range(number_of_devices):
            valid = False
            while not valid:
                input_str = 'Enter address for HAT device {}: '.format(device)
                address = int(input(input_str))

                # Verify the selected address exists.
                if any(hat.address == address for hat in hats):
                    valid = True
                else:
                    print('Invalid address - try again')

                # Verify the address was not previously selected
                if any(hat.address() == address for hat in selected_hats):
                    print('Address already selected - try again')
                    valid = False

                if valid:
                    selected_hats.append(mcc172(address))

    return selected_hats


if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
