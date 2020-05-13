/*****************************************************************************

    MCC 128 Functions Demonstrated:
        mcc128_trigger_mode
        mcc128_a_in_scan_start
        mcc128_a_in_scan_status
        mcc128_a_in_scan_read
        mcc128_a_in_mode_write
        mcc128_a_in_range_write

    Purpose:
        Perform a triggered finite acquisition on 1 or more channels.

    Description:
        Waits for an external trigger to occur and then acquires blocks
        of analog input data for a user-specified group of channels.  The
        last sample of data for each channel is displayed for each block
        of data received from the device.  The acquisition is stopped when
        the specified number of samples is acquired for each channel.

*****************************************************************************/
#include "../../daqhats_utils.h"

int main(void)
{
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char channel_string[512];
    char options_str[512];
    char c;
    char display_header[512];
    char trigger_mode_str[512];
    int i;
    char mode_string[32];
    char range_string[32];

    // Set the channel mask which is used by the library function
    // mcc128_a_in_scan_start to specify the channels to acquire.
    // The functions below, will parse the channel mask into a
    // character string for display purposes.
    uint8_t channel_mask = {CHAN0 | CHAN1 | CHAN2 | CHAN3};
    uint8_t input_mode = A_IN_MODE_SE;
    uint8_t input_range = A_IN_RANGE_BIP_10V;

    int max_channel_array_length = mcc128_info()->NUM_AI_CHANNELS[input_mode];
    int channel_array[max_channel_array_length];
    uint8_t num_channels = convert_chan_mask_to_array(channel_mask,
        channel_array);

    uint32_t samples_per_channel = 10000;
    uint32_t buffer_size = samples_per_channel * num_channels;
    double read_buf[buffer_size];
    int total_samples_read = 0;

    int32_t read_request_size = 500;
    double timeout = 5.0;

    double scan_rate = 1000.0;
    double actual_scan_rate = 0.0;
    mcc128_a_in_scan_actual_rate(num_channels, scan_rate, &actual_scan_rate);

    uint32_t options = OPTS_EXTTRIGGER;

    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;

    uint8_t trigger_mode = TRIG_RISING_EDGE;
    int cancel_trigger = 0;

    // Select an MCC128 HAT device to use.
    if (select_hat_device(HAT_ID_MCC_128, &address))
    {
        // Error getting device.
        return -1;
    }

    printf("\nSelected MCC 128 device at address %d\n", address);

    // Open a connection to the device.
    result = mcc128_open(address);
    STOP_ON_ERROR(result);

    result = mcc128_a_in_mode_write(address, input_mode);
    STOP_ON_ERROR(result);

    result = mcc128_a_in_range_write(address, input_range);
    STOP_ON_ERROR(result);

    convert_options_to_string(options, options_str);
    convert_chan_mask_to_string(channel_mask, channel_string);
    convert_input_mode_to_string(input_mode, mode_string);
    convert_input_range_to_string(input_range, range_string);
    convert_trigger_mode_to_string(trigger_mode, trigger_mode_str);

    printf("\nMCC128 finite scan with trigger  example\n");
    printf("    Functions demonstrated:\n");
    printf("        mcc128_trigger_mode\n");
    printf("        mcc128_a_in_scan_start\n");
    printf("        mcc128_a_in_scan_status\n");
    printf("        mcc128_a_in_scan_read\n");
    printf("        mcc128_a_in_mode_write\n");
    printf("        mcc128_a_in_range_write\n");
    printf("    Input mode: %s\n", mode_string);
    printf("    Input range: %s\n", range_string);
    printf("    Channels: %s\n", channel_string);
    printf("    Samples per channel: %d\n", samples_per_channel);
    printf("    Requested scan rate: %-10.2f\n", scan_rate);
    printf("    Actual scan rate: %-10.2f\n", actual_scan_rate);
    printf("    Options: %s\n", options_str);
    printf("    Trigger mode: %s\n", trigger_mode_str);

    printf("\nPress ENTER to continue\n");
    scanf("%c", &c);

    result = mcc128_trigger_mode(address, trigger_mode);
    STOP_ON_ERROR(result);

    // Configure and start the scan.
    result = mcc128_a_in_scan_start(address, channel_mask, samples_per_channel,
        scan_rate, options);
    STOP_ON_ERROR(result);

    printf("Waiting for trigger ... hit ENTER to cancel the trigger\n");
    do
    {
        result = mcc128_a_in_scan_status(address, &read_status,
            &samples_read_per_channel);
        STOP_ON_ERROR(result);

        cancel_trigger = enter_press();
        if (!cancel_trigger &&
            ((read_status & STATUS_TRIGGERED) != STATUS_TRIGGERED))
        {
            usleep(1000);
        }
    }
    while ((result == RESULT_SUCCESS) &&
           ((read_status & STATUS_TRIGGERED) != STATUS_TRIGGERED) &&
           !cancel_trigger);

    if (cancel_trigger)
    {
        printf("Trigger cancelled by user\n");
        goto stop;
    }

    printf("\nStarting scan ... Press ENTER to stop\n\n");

    // Create the header containing the column names.
    strcpy(display_header, "Samples Read    Scan Count    ");
    for (i = 0; i < num_channels; i++)
    {
        sprintf(channel_string, "Channel %d   ", channel_array[i]);
        strcat(display_header, channel_string);
    }
    strcat(display_header, "\n");

    printf("%s", display_header);

    // Continuously update the display value until enter key is pressed
    // or the number of samples requested has been read.
    do
    {
        // Read the specified number of samples.
        result = mcc128_a_in_scan_read(address, &read_status, read_request_size,
            timeout, read_buf, buffer_size, &samples_read_per_channel);
        STOP_ON_ERROR(result);
        if (read_status & STATUS_HW_OVERRUN)
        {
            printf("\n\nHardware overrun\n");
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            printf("\n\nBuffer overrun\n");
            break;
        }

        total_samples_read += samples_read_per_channel;

        // Display the last sample for each channel.
        printf("\r%12.0d    %10.0d ", samples_read_per_channel,
            total_samples_read);
        if (samples_read_per_channel > 0)
        {
            int index = samples_read_per_channel * num_channels - num_channels;

            for (i = 0; i < num_channels; i++)
            {
                printf("%10.5f V", read_buf[index + i]);
            }
            fflush(stdout);
        }
    }
    while ((result == RESULT_SUCCESS) &&
           ((read_status & STATUS_RUNNING) == STATUS_RUNNING) &&
           !enter_press());

    printf("\n");

stop:
    mcc128_a_in_scan_stop(address);
    mcc128_a_in_scan_cleanup(address);
    mcc128_close(address);

    return 0;
}
