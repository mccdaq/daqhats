#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
    MCC 172 Functions Demonstrated:
        mcc172.iepe_config_write
        mcc172.a_in_clock_config_write
        mcc172.a_in_clock_config_read
        mcc172.a_in_scan_start
        mcc172.a_in_scan_read
        mcc172.a_in_scan_stop

    Purpose:
        Perform a finite acquisition on a channel, calculate the FFT, and
        display peak information.

    Description:
        Acquires blocks of analog input data for a single channel then performs
        an FFT calculation to determine the frequency content. The highest
        frequency peak is detected and displayed, along with harmonics. The 
        time and frequency data are saved to a CSV file.

"""
from __future__ import print_function
from time import sleep
from sys import stdout, version_info
from numpy import fft, real, imag
from math import cos, pi, log10, sqrt, fabs
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
    channels = [0]
    channel_mask = chan_list_to_mask(channels)
    num_channels = len(channels)

    samples_per_channel = 12800
    scan_rate = 51200.0
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

        # Configure the clock and wait for sync to complete.
        hat.a_in_clock_config_write(SourceType.LOCAL, scan_rate)

        synced = False
        while not synced:
            (_source_type, actual_scan_rate, synced) = hat.a_in_clock_config_read()
            if not synced:
                sleep(0.005)

        print('\nMCC 172 FFT example')
        print('    Functions demonstrated:')
        print('         mcc172.iepe_config_write')
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
        print('    Channels: ', end='')
        print(', '.join([str(chan) for chan in channels]))
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

        try:
            read_and_display_data(hat, samples_per_channel, num_channels,
                                  actual_scan_rate)

        except KeyboardInterrupt:
            # Clear the '^C' from the display.
            print(CURSOR_BACK_2, ERASE_TO_END_OF_LINE, '\n')
            hat.a_in_scan_stop()

        hat.a_in_scan_cleanup()

    except (HatError, ValueError) as err:
        print('\n', err)

def window(index, max):
    # Hann window function.
    return 0.5 - 0.5*cos(2*pi*index / max)

def window_compensation():
    # Hann window compensation factor.
    return 2.0;

def calculate_real_fft(data):
    n_samples = len(data)
    in_data = [0.0] * n_samples
    
    # Apply the window and normalize the time data.
    max_v = mcc172.info().AI_MAX_RANGE;
    for i in range(n_samples):
        in_data[i] = window(i, n_samples) * data[i] / max_v;
   
    # Perform the FFT.
    out = fft.rfft(in_data)
    
    # Convert the complex results to real and convert to dBFS.
    spectrum = [0.0] * len(out)
    real_part = real(out)
    imag_part = imag(out)
    for i in range(len(out)):
        if i == 0:
            # Don't multiply DC value times 2.
            spectrum[i] = 20*log10(
                window_compensation() * 
                sqrt(real_part[i] * real_part[i] + imag_part[i] * imag_part[i]) / 
                n_samples)
        else:
            spectrum[i] = 20*log10(
                window_compensation() * 2 *
                sqrt(real_part[i] * real_part[i] + imag_part[i] * imag_part[i]) / 
                n_samples)
    
    return spectrum

def quadratic_interpolate(bin0, bin1, bin2):
    # Interpolate between the bins of an FFT peak to find a more accurate
    # frequency.  bin1 is the FFT value at the detected peak, bin0 and bin2
    # are the values from adjacent bins above and below the peak. Returns
    # the offset value from the index of bin1.
    y1 = fabs(bin0)
    y2 = fabs(bin1)
    y3 = fabs(bin2)

    d = (y3 - y1) / (2 * (2 * y2 - y1 - y3))

    return d

def order_suffix( index):
    if index == 1:
        return "st"
    elif index == 2:
        return "nd"
    elif index == 3:
        return "rd"
    else:
        return "th"

def read_and_display_data(hat, samples_per_channel, num_channels, scan_rate):
    """
    Wait for all of the scan data, perform an FFT, find the peak frequency, 
    and display the frequency information

    Args:
        hat (mcc172): The mcc172 HAT device object.
        samples_per_channel: The number of samples to read for each channel.
        num_channels (int): The number of channels to display.

    Returns:
        None

    """
    total_samples_read = 0
    timeout = 5.0

    # Wait for all the data.
    read_result = hat.a_in_scan_read(samples_per_channel, timeout)
    
    # Check for an overrun error.
    if read_result.hardware_overrun:
        print('\n\nHardware overrun\n')
        return
    elif read_result.buffer_overrun:
        print('\n\nBuffer overrun\n')
        return
        
    # Calculate the FFT.
    spectrum = calculate_real_fft(read_result.data)
    
    # Calculate dBFS and find peak.
    f_i = 0.0
    peak_index = 0
    peak_val = -1000.0

    # Save data to CSV file
    logfile = open("fft_scan.csv", "w")
    logfile.write("Time data (V), Frequency (Hz), Spectrum (dBFS)\n")
    
    for i in range(len(spectrum)):
        # Find the peak value and index.
        if spectrum[i] > peak_val:
            peak_val = spectrum[i]
            peak_index = i
            
        # Save to the CSV file.
        logfile.write("{0:.6f},{1:.3f},{2:.6f}\n".format(read_result.data[i], f_i, 
            spectrum[i]))
            
        f_i += scan_rate / samples_per_channel
    
    logfile.close()
    
    # Interpolate for a more precise peak frequency.
    peak_offset = quadratic_interpolate(spectrum[peak_index - 1], 
        spectrum[peak_index], spectrum[peak_index + 1])
    peak_freq = ((peak_index + peak_offset) * scan_rate / 
        samples_per_channel)
    print("Peak: {0:.1f} dBFS at {1:.1f} Hz".format(peak_val, peak_freq))
    
    # Find and display harmonic levels.
    i = 2
    h_freq = 0
    nyquist = scan_rate / 2.0
    while (i < 8) and (h_freq <= nyquist):
        # Stop when frequency exceeds Nyquist rate or at the 8th harmonic.
        h_freq = peak_freq * i
        if h_freq <= nyquist:
            h_index = int(h_freq * samples_per_channel / scan_rate +
                0.5)
            h_val = spectrum[h_index]
            print("{0:d}{1:s} harmonic: {2:.1f} dBFS at {3:.1f} Hz".format(
                i, order_suffix(i), h_val, h_freq))
        i += 1
    

if __name__ == '__main__':
    main()
