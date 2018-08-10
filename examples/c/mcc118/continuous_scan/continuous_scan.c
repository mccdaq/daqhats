/*****************************************************************************

    MCC 118 Functions Demonstrated:
        mcc118_a_in_scan_start
        mcc118_a_in_scan_read
        mcc118_a_in_scan_stop

    Purpose:
        Perform a continuous acquisition on 1 or more channels.

    Description:
        Continuously acquires blocks of analog input data for a
        user-specified group of channels until the acquisition is
        stopped by the user.  The last sample of data for each channel
        is displayed for each block of data received from the device.

*****************************************************************************/
#include "../../daqhats_utils.h"

int main(void)
{
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char c;
    char display_header[512];
    int i;
    char channel_string[512];
    char options_str[512];

    // Set the channel mask which is used by the library function
    // mcc118_a_in_scan_start to specify the channels to acquire.
    // The functions below, will parse the channel mask into a
    // character string for display purposes.
    uint8_t channel_mask = {CHAN0 | CHAN1 | CHAN2 | CHAN3};
    convert_chan_mask_to_string(channel_mask, channel_string);
    
    uint32_t samples_per_channel = 0;

    int max_channel_array_length = mcc118_info()->NUM_AI_CHANNELS;
    int channel_array[max_channel_array_length];
    uint8_t num_channels = convert_chan_mask_to_array(channel_mask, 
        channel_array);

    uint32_t internal_buffer_size_samples = 0;
    uint32_t user_buffer_size = 1000 * num_channels;
    double read_buf[user_buffer_size];
    int total_samples_read = 0;

    int32_t read_request_size = READ_ALL_AVAILABLE;

    // When doing a continuous scan, the timeout value will be ignored in the
    // call to mcc118_a_in_scan_read because we will be requesting that all
    // available samples (up to the default buffer size) be returned.
    double timeout = 5.0;

    double scan_rate = 1000.0;
    double actual_scan_rate = 0.0;
    mcc118_a_in_scan_actual_rate(num_channels, scan_rate, &actual_scan_rate);

    uint32_t options = OPTS_CONTINUOUS;

    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;


    // Select an MCC118 HAT device to use.
    if (select_hat_device(HAT_ID_MCC_118, &address))
    {
        // Error getting device.
        return -1;
    }

    printf ("\nSelected MCC 118 device at address %d\n", address);

    // Open a connection to the device.
    result = mcc118_open(address);
    STOP_ON_ERROR(result);

    convert_options_to_string(options, options_str);
    convert_chan_mask_to_string(channel_mask, channel_string) ;

    printf("\nMCC 118 continuous scan example\n");
    printf("    Functions demonstrated:\n");
    printf("        mcc118_a_in_scan_start\n");
    printf("        mcc118_a_in_scan_read\n");
    printf("        mcc118_a_in_scan_stop\n");
    printf("    Channels: %s\n", channel_string);
    printf("    Requested scan rate: %-10.2f\n", scan_rate);
    printf("    Actual scan rate: %-10.2f\n", actual_scan_rate);
    printf("    Options: %s\n", options_str);

    printf("\nPress ENTER to continue ...\n");
    scanf("%c", &c);

    // Configure and start the scan.
    // Since the continuous option is being used, the samples_per_channel 
    // parameter is ignored if the value is less than the default internal
    // buffer size (10000 * num_channels in this case). If a larger internal
    // buffer size is desired, set the value of this parameter accordingly.
    result = mcc118_a_in_scan_start(address, channel_mask, samples_per_channel,
        scan_rate, options);
    STOP_ON_ERROR(result);

    STOP_ON_ERROR(mcc118_a_in_scan_buffer_size(address,
        &internal_buffer_size_samples));
    printf("Internal data buffer size:  %d\n", internal_buffer_size_samples);


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
    do 
    {
        // Since the read_request_size is set to -1 (READ_ALL_AVAILABLE), this
        // function returns immediately with whatever samples are available (up
        // to user_buffer_size) and the timeout parameter is ignored.
        result = mcc118_a_in_scan_read(address, &read_status, read_request_size,
            timeout, read_buf, user_buffer_size, &samples_read_per_channel);
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

        usleep(500000);

    } 
    while ((result == RESULT_SUCCESS) &&
           ((read_status & STATUS_RUNNING) == STATUS_RUNNING) && 
           !enter_press());

    printf("\n");

stop:
    print_error(mcc118_a_in_scan_stop(address));
    print_error(mcc118_a_in_scan_cleanup(address));
    print_error(mcc118_close(address));

    return 0;
}
