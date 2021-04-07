#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
    MCC 128 Functions Demonstrated:
        mcc128.trigger_mode
        mcc128.a_in_scan_start
        mcc128.a_in_scan_status
        mcc128.a_in_scan_read
        mcc128.a_in_scan_stop
        mcc128_a_in_scan_cleanup
        mcc128.a_in_mode_write
        mcc128.a_in_range_write

    Purpose:
    Get synchronous data from multiple MCC 128 HAT devices.

    Description:
        This example demonstrates acquiring data synchronously from multiple
        MCC 128 HAT devices.  This is done using the external clock and
        external trigger scan options.  The CLK terminals must be connected
        together on all MCC 128 HAT devices being used and an external trigger
        source must be provided to the TRIG terminal on the master MCC 128 HAT
        device.  The EXTCLOCK scan option is set on all of the MCC 128
        HAT devices except the master and the EXTTRIGGER scan option is
        set on the master.
"""
from __future__ import print_function
from sys import stdout
from daqhats import hat_list, mcc128, OptionFlags, HatIDs, TriggerModes, \
    HatError, AnalogInputMode, AnalogInputRange
from daqhats_utils import enum_mask_to_string, chan_list_to_mask, \
    validate_channels, input_mode_to_string, input_range_to_string

# Constants
DEVICE_COUNT = 2
MASTER = 0
CURSOR_SAVE = "\x1b[s"
CURSOR_RESTORE = "\x1b[u"
CURSOR_BACK_2 = '\x1b[2D'
ERASE_TO_END_OF_LINE = '\x1b[0K'


def main():
    """
    This function is executed automatically when the module is run directly.
    """
    hats = []
    # Define the input modes for each MCC 128
    input_modes = [
        AnalogInputMode.SE,
        AnalogInputMode.SE
    ]
    # Define the input ranges for each MCC 128
    input_ranges = [
        AnalogInputRange.BIP_10V,
        AnalogInputRange.BIP_10V
    ]
    # Define the channel list for each HAT device
    chans = [
        {0, 1},
        {0, 1}
    ]
    # Define the options for each HAT device
    options = [
        OptionFlags.EXTTRIGGER,
        OptionFlags.EXTCLOCK
    ]
    samples_per_channel = 10000
    sample_rate = 1000.0  # Samples per second
    trigger_mode = TriggerModes.RISING_EDGE

    try:
        # Get an instance of the selected hat device object.
        hats = select_hat_devices(HatIDs.MCC_128, DEVICE_COUNT)

        # Validate the selected channels, set the modes and ranges.
        for i, hat in enumerate(hats):
            validate_channels(chans[i], hat.info().NUM_AI_CHANNELS[input_modes[i]])
            hat.a_in_mode_write(input_modes[i])
            hat.a_in_range_write(input_ranges[i])

        # Set the trigger mode for the master device.
        hats[MASTER].trigger_mode(trigger_mode)

        # Calculate the actual sample rate.
        actual_rate = hats[MASTER].a_in_scan_actual_rate(len(chans[MASTER]),
                                                         sample_rate)

        print('MCC 128 multiple HAT example using external clock and',
              'external trigger options')
        print('    Functions demonstrated:')
        print('      mcc128.trigger_mode')
        print('      mcc128.a_in_scan_start')
        print('      mcc128.a_in_scan_status')
        print('      mcc128.a_in_scan_read')
        print('      mcc128.a_in_scan_stop')
        print('      mcc128.a_in_scan_cleanup')
        print('      mcc128.a_in_mode_write')
        print('      mcc128.a_in_range_write')
        print('    Samples per channel:', samples_per_channel)
        print('    Requested Sample Rate: {:.3f} Hz'.format(sample_rate))
        print('    Actual Sample Rate: {:.3f} Hz'.format(actual_rate))
        print('    Trigger type:', trigger_mode.name)

        for i, hat in enumerate(hats):
            print('    HAT {}:'.format(i))
            print('      Address:', hat.address())
            print('      Input mode: ', input_mode_to_string(input_modes[i]))
            print('      Input range: ', input_range_to_string(input_ranges[i]))
            print('      Channels: ', end='')
            print(', '.join([str(chan) for chan in chans[i]]))
            options_str = enum_mask_to_string(OptionFlags, options[i])
            print('      Options:', options_str)

        print('\n*NOTE: Connect the CLK terminals together on each MCC 128')
        print('       HAT device being used. Connect a trigger source')
        print('       to the TRIG input terminal on HAT 0.')

        try:
            input("\nPress 'Enter' to continue")
        except (NameError, SyntaxError):
            pass

        # Start the scan.
        for i, hat in enumerate(hats):
            chan_mask = chan_list_to_mask(chans[i])
            hat.a_in_scan_start(chan_mask, samples_per_channel, sample_rate,
                                options[i])

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
        hat (mcc128): The mcc128 HAT device object on which the status will
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


def read_and_display_data(hats, chans):
    """
    Reads data from the specified channels on the specified DAQ HAT devices
    and updates the data on the terminal display.  The reads are executed in a
    loop that continues until either the scan completes or an overrun error
    is detected.

    Args:
        hats (list[mcc128]): A list of mcc128 HAT device objects.
        chans (list[int][int]): A 2D list to specify the channel list for each
            mcc128 HAT device.

    Returns:
        None

    """
    samples_to_read = 500
    timeout = 5  # Seconds
    samples_per_chan_read = [0] * DEVICE_COUNT
    total_samples_per_chan = [0] * DEVICE_COUNT
    is_running = True

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
            for chan_idx in range(len(chans[i])):
                if samples_per_chan_read[i] > 0:
                    sample_idx = ((samples_per_chan_read[i] * len(chans[i]))
                                  - len(chans[i]) + chan_idx)
                    print(' {:>12.5f} V'.format(data[i][sample_idx]), end='')
            print('\n')

        stdout.flush()

        if not is_running:
            break


def select_hat_devices(filter_by_id, number_of_devices):
    """
    This function performs a query of available DAQ HAT devices and determines
    the addresses of the DAQ HAT devices to be used in the example.  If the
    number of HAT devices present matches the requested number of devices,
    a list of all mcc128 objects is returned in order of address, otherwise the
    user is prompted to select addresses from a list of displayed devices.

    Args:
        filter_by_id (int): If this is :py:const:`HatIDs.ANY` return all DAQ
            HATs found.  Otherwise, return only DAQ HATs with ID matching this
            value.
        number_of_devices (int): The number of devices to be selected.

    Returns:
        list[mcc128]: A list of mcc128 objects for the selected devices
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
        error_string = ('Error: This example requires {0} MCC 128 HATs - '
                        'found {1}'.format(number_of_devices, number_of_hats))
        raise HatError(0, error_string)
    elif number_of_hats == number_of_devices:
        for i in range(number_of_devices):
            selected_hats.append(mcc128(hats[i].address))
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
                    selected_hats.append(mcc128(address))

    return selected_hats


if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
