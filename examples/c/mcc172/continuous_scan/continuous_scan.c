/*****************************************************************************

    MCC 172 Functions Demonstrated:
        mcc172_iepe_config_write
        mcc172_a_in_clock_config_read
        mcc172_a_in_clock_config_write
        mcc172_a_in_scan_start
        mcc172_a_in_scan_read
        mcc172_a_in_scan_stop

    Purpose:
        Perform a continuous acquisition on 1 or more channels.

    Description:
        Continuously acquires blocks of analog input data for a
        user-specified group of channels until the acquisition is
        stopped by the user.  The RMS voltage for each channel
        is displayed for each block of data received from the device.

*****************************************************************************/
#include <math.h>
#include "../../daqhats_utils.h"

double calc_rms(double* data, uint8_t channel, uint8_t num_channels,
    uint32_t num_samples_per_channel)
{
    double value;
    uint32_t i;
    uint32_t index;

    value = 0.0;
    for (i = 0; i < num_samples_per_channel; i++)
    {
        index = (i * num_channels) + channel;
        value += (data[index] * data[index]) / num_samples_per_channel;
    }

    return sqrt(value);
}

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
    // mcc172_a_in_scan_start to specify the channels to acquire.
    // The functions below, will parse the channel mask into a
    // character string for display purposes.
    uint8_t channel_mask = {CHAN0 | CHAN1};
    convert_chan_mask_to_string(channel_mask, channel_string);


    int max_channel_array_length = mcc172_info()->NUM_AI_CHANNELS;
    int channel_array[max_channel_array_length];
    uint8_t num_channels = convert_chan_mask_to_array(channel_mask,
        channel_array);

    // When doing a continuous scan, the timeout value will be ignored in the
    // call to mcc172_a_in_scan_read because we will be requesting that all
    // available samples (up to the default buffer size) be returned.
    double timeout = 5.0;
    double scan_rate = 51200.0;
    double actual_scan_rate = 0.0;
    uint32_t options = OPTS_CONTINUOUS;
    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;
    uint8_t synced;
    uint8_t clock_source;
    uint32_t user_buffer_size = 2 * scan_rate * num_channels;
    uint32_t samples_per_channel = 2 * scan_rate;
    double read_buf[user_buffer_size];
    int total_samples_read = 0;
    uint8_t iepe_enable;
    int32_t read_request_size = READ_ALL_AVAILABLE;


    // Select an MCC 172 HAT device to use.
    if (select_hat_device(HAT_ID_MCC_172, &address))
    {
        // Error getting device.
        return -1;
    }

    printf ("\nSelected MCC 172 device at address %d\n", address);

    // Open a connection to the device.
    result = mcc172_open(address);
    STOP_ON_ERROR(result);

    // Turn on IEPE supply?
    printf("Enable IEPE power [y or n]?  ");
    scanf("%c", &c);
    if ((c == 'y') || (c == 'Y'))
    {
        iepe_enable = 1;
    }
    else if ((c == 'n') || (c == 'N'))
    {
        iepe_enable = 0;
    }
    else
    {
        printf("Error: Invalid selection\n");
        mcc172_close(address);
        return 1;
    }
    flush_stdin();

    for (i = 0; i < num_channels; i++)
    {
        result = mcc172_iepe_config_write(address, channel_array[i],
            iepe_enable);
        STOP_ON_ERROR(result);
    }

    // Set the ADC clock to the desired rate.
    result = mcc172_a_in_clock_config_write(address, SOURCE_LOCAL, scan_rate);
    STOP_ON_ERROR(result);

    // Wait for the ADCs to synchronize.
    do
    {
        result = mcc172_a_in_clock_config_read(address, &clock_source,
            &actual_scan_rate, &synced);
        STOP_ON_ERROR(result);
        usleep(5000);
    } while (synced == 0);

    convert_options_to_string(options, options_str);
    convert_chan_mask_to_string(channel_mask, channel_string) ;

    printf("\nMCC 172 continuous scan example\n");
    printf("    Functions demonstrated:\n");
    printf("        mcc172_iepe_config_write\n");
    printf("        mcc172_a_in_clock_config_read\n");
    printf("        mcc172_a_in_clock_config_write\n");
    printf("        mcc172_a_in_scan_start\n");
    printf("        mcc172_a_in_scan_read\n");
    printf("        mcc172_a_in_scan_stop\n");
    printf("    Channels: %s\n", channel_string);
    printf("    Requested scan rate: %-10.2f\n", scan_rate);
    printf("    Actual scan rate: %-10.2f\n", actual_scan_rate);
    printf("    Options: %s\n", options_str);

    printf("\nPress ENTER to continue\n");
    scanf("%c", &c);

    // Configure and start the scan.
    // Since the continuous option is being used, the samples_per_channel
    // parameter is ignored if the value is less than the default internal
    // buffer size (10000 * num_channels in this case). If a larger internal
    // buffer size is desired, set the value of this parameter accordingly.
    result = mcc172_a_in_scan_start(address, channel_mask, samples_per_channel,
        options);
    STOP_ON_ERROR(result);

    printf("Starting scan ... Press ENTER to stop\n\n");

    // Create the header containing the column names.
    strcpy(display_header, "Samples Read    Scan Count    ");
    for (i = 0; i < num_channels; i++)
    {
        sprintf(channel_string, "Ch %d RMS  ", channel_array[i]);
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
        result = mcc172_a_in_scan_read(address, &read_status, read_request_size,
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

        if (samples_read_per_channel > 0)
        {
            // Display the samples read and total samples
            printf("\r%12.0d    %10.0d  ", samples_read_per_channel,
                total_samples_read);

            // Calculate and display RMS voltage of the input data
            for (i = 0; i < num_channels; i++)
            {
                printf("%10.4f",
                    calc_rms(read_buf, i, num_channels,
                        samples_read_per_channel));
            }
            fflush(stdout);
        }

        usleep(100000);
    }
    while ((result == RESULT_SUCCESS) &&
           ((read_status & STATUS_RUNNING) == STATUS_RUNNING) &&
           !enter_press());

    printf("\n");

stop:
    print_error(mcc172_a_in_scan_stop(address));
    print_error(mcc172_a_in_scan_cleanup(address));

    // Turn off IEPE supply
    for (i = 0; i < num_channels; i++)
    {
        result = mcc172_iepe_config_write(address, channel_array[i], 0);
        STOP_ON_ERROR(result);
    }

    print_error(mcc172_close(address));

    return 0;
}
