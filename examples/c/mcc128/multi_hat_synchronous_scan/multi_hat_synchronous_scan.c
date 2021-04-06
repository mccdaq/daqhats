/*****************************************************************************

    MCC 128 Functions Demonstrated:
        mcc128_trigger_mode
        mcc128_a_in_scan_start
        mcc128_a_in_scan_status
        mcc128_a_in_scan_read
        mcc128_a_in_mode_write
        mcc128_a_in_range_write

    Purpose:
        Get synchronous data from multiple MCC 128 devices.

    Description:
        This example demonstrates acquiring data synchronously from multiple
        MCC 128 devices.  This is done using the external clock and
        external trigger scan options.  The CLK terminals must be connected
        together on all MCC 128 devices being used and an external trigger
        source must be provided to the TRIG terminal on the master MCC 128
        device.  The OPTS_EXTCLOCK scan option is set on all of the MCC 128
        HAT devices except the master and the OPTS_EXTTRIGGER scan option is
        set on the master.

*****************************************************************************/
#include "../../daqhats_utils.h"

#define DEVICE_COUNT 2
#define MASTER 0

#define CURSOR_SAVE "\x1b[s"
#define CURSOR_RESTORE "\x1b[u"

int get_hat_addresses(uint8_t address[]);

int main(void)
{
    // HAT device addresses - determined at runtime
    uint8_t address[DEVICE_COUNT];

    // Input mode and range settings
    uint8_t input_mode[DEVICE_COUNT] = {
        A_IN_MODE_SE,
        A_IN_MODE_SE
    };
    uint8_t input_range[DEVICE_COUNT] = {
        A_IN_RANGE_BIP_10V,
        A_IN_RANGE_BIP_10V
    };

    // mcc128_a_in_scan_start() variables
    uint8_t chan_mask[DEVICE_COUNT] = {
        CHAN0 | CHAN1,
        CHAN0 | CHAN1
    };
    uint32_t options[DEVICE_COUNT] = {
        OPTS_EXTTRIGGER,
        OPTS_EXTCLOCK
    };
    uint32_t samples_per_channel = 10000;
    double sample_rate = 1000; // Samples per second

    // mcc128_trigger_mode() variable
    uint8_t trigger_mode = TRIG_RISING_EDGE;

    int mcc128_num_channels = mcc128_info()->NUM_AI_CHANNELS[A_IN_MODE_SE];

    // mcc128_a_in_scan_status() and mcc128_a_in_scan_read() variables
    uint16_t scan_status[DEVICE_COUNT] = {0};
    int buffer_size = samples_per_channel * mcc128_num_channels;
    double data[DEVICE_COUNT][buffer_size];
    uint32_t samples_read[DEVICE_COUNT] = {0};
    int32_t samples_to_read = 500;
    double timeout = 1;  //seconds
    uint32_t samples_available = 0;

    int result = 0;
    int chan_count[DEVICE_COUNT];
    int chans[DEVICE_COUNT][mcc128_num_channels];
    int device = 0;
    int sample_idx = 0;
    int i = 0;
    int total_samples_read[DEVICE_COUNT] = {0};
    const int data_display_line_count = DEVICE_COUNT * 4;
    uint16_t is_running = 0;
    uint16_t is_triggered = 0;
    uint16_t scan_status_all = 0;

    char chan_display[DEVICE_COUNT][32];
    char options_str[DEVICE_COUNT][256];
    char mode_str[DEVICE_COUNT][32];
    char range_str[DEVICE_COUNT][32];
    char display_string[256] = "";
    char data_display_output[1024] = "";
    char c;

    double actual_sample_rate = 0.0;

    // Determine the addresses of the devices to be used
    if (get_hat_addresses(address) != 0)
    {
        return -1;
    }

    for (device = 0; device < DEVICE_COUNT; device++)
    {
        // Open a connection to each device
        result = mcc128_open(address[device]);
        STOP_ON_ERROR(result);

        result = mcc128_a_in_mode_write(address[device], input_mode[device]);
        STOP_ON_ERROR(result);

        result = mcc128_a_in_range_write(address[device], input_range[device]);
        STOP_ON_ERROR(result);

        // channel_mask is used by the library function mcc128_a_in_scan_start.
        // The functions below parse the mask for display purposes.
        chan_count[device] = convert_chan_mask_to_array(chan_mask[device],
            chans[device]);
        convert_chan_mask_to_string(chan_mask[device], chan_display[device]);
        convert_options_to_string(options[device], options_str[device]);
        convert_input_mode_to_string(input_mode[device], mode_str[device]);
        convert_input_range_to_string(input_range[device], range_str[device]);
    }

    // Set the trigger mode on the master device
    result = mcc128_trigger_mode(address[MASTER], trigger_mode);
    STOP_ON_ERROR(result);

    result = mcc128_a_in_scan_actual_rate(chan_count[MASTER], sample_rate,
        &actual_sample_rate);
    STOP_ON_ERROR(result);

    printf("\nMCC 128 multiple device example using external clock and "
        "external trigger options\n");
    printf("    Functions demonstrated:\n");
    printf("      mcc128_trigger_mode\n");
    printf("      mcc128_a_in_scan_start\n");
    printf("      mcc128_a_in_scan_status\n");
    printf("      mcc128_a_in_scan_read\n");
    printf("      mcc128_a_in_mode_write\n");
    printf("      mcc128_a_in_range_write\n");
    printf("    Samples per channel: %d\n", samples_per_channel);
    printf("    Requested Sample Rate: %.3f Hz\n", sample_rate);
    printf("    Actual Sample Rate: %.3f Hz\n", actual_sample_rate);
    convert_trigger_mode_to_string(trigger_mode, display_string);
    printf("    Trigger type: %s\n", display_string);

    for (device = 0; device < DEVICE_COUNT; device++)
    {
        printf("    MCC 128 %d:\n", device);
        printf("      Address: %d\n", address[device]);
        printf("      Input mode: %s\n", mode_str[device]);
        printf("      Input range: %s\n", range_str[device]);
        printf("      Channels: %s\n", chan_display[device]);
        printf("      Options: %s\n", options_str[device]);
    }

    printf("\n*NOTE: Connect the CLK terminals together on each MCC 128 device "
        "being used.");
    printf("\n       Connect a trigger source to the TRIG input terminal on "
        "device at address %d.\n", address[0]);

    printf("\nPress 'Enter' to continue\n");

    scanf("%c", &c);

    // Start the scan
    for (device = 0; device < DEVICE_COUNT; device++)
    {
        result = mcc128_a_in_scan_start(address[device], chan_mask[device],
            samples_per_channel, sample_rate, options[device]);
        STOP_ON_ERROR(result);
    }

    printf("Waiting for trigger ... Press 'Enter' to abort\n\n");

    do
    {
        usleep(10000);
        // Check the master status in a loop to determine when the trigger occurs
        result = mcc128_a_in_scan_status(address[MASTER], &scan_status[MASTER],
                                         &samples_available);
        STOP_ON_ERROR(result);

        is_running = (scan_status[MASTER] & STATUS_RUNNING);
        is_triggered = (scan_status[MASTER] & STATUS_TRIGGERED);

    }
    while (is_running && !is_triggered && !enter_press());

    if (is_running && is_triggered)
    {
        printf("Acquiring data ... Press 'Enter' to abort\n\n");
        // Create blank lines where the data will be displayed.
        for (i = 0; i <= data_display_line_count; i++)
        {
            printf("\n");
        }
        // Move the cursor up to the start of the data display
        printf("\x1b[%dA", data_display_line_count + 1);
        // Save the cursor position
        printf(CURSOR_SAVE);
    }
    else
    {
        printf("Aborted\n\n");
        is_running = 0;
    }

    while (is_running)
    {
        for (device = 0; device < DEVICE_COUNT; device++)
        {
            // Read data
            result = mcc128_a_in_scan_read(address[device],
                &scan_status[device], samples_to_read, timeout, data[device],
                buffer_size, &samples_read[device]);
            STOP_ON_ERROR(result);
            // Check for overrun on any one device
            scan_status_all |= scan_status[device];
            // Verify the status of all devices is running
            is_running &= (scan_status[device] & STATUS_RUNNING);
        }

        if ((scan_status_all & STATUS_HW_OVERRUN) == STATUS_HW_OVERRUN)
        {
            fprintf(stderr, "\nError: Hardware overrun\n");
            break;
        }
        if ((scan_status_all & STATUS_BUFFER_OVERRUN) == STATUS_BUFFER_OVERRUN)
        {
            fprintf(stderr, "\nError: Buffer overrun\n");
            break;
        }

        // Restore the cursor position to the start of the data display
        printf(CURSOR_RESTORE);

        for (device = 0; device < DEVICE_COUNT; device++)
        {
            strcpy(data_display_output, "");

            sprintf(display_string, "HAT %d:\n", device);
            strcat(data_display_output, display_string);

            // Display the header row for the data table
            strcat(data_display_output, "  Samples Read    Scan Count");
            for (i = 0; i < chan_count[device]; i++)
            {
                sprintf(display_string, "     Channel %d", chans[device][i]);
                strcat(data_display_output, display_string);
            }
            strcat(data_display_output, "\n");

            // Display the sample count information for the device
            total_samples_read[device] += samples_read[device];
            sprintf(display_string, "%14d%14d", samples_read[device],
                total_samples_read[device]);
            strcat(data_display_output, display_string);

            // Display the data for all active channels
            if (samples_read[device] > 0)
            {
                for (i = 0; i < chan_count[device]; i++)
                {
                    sample_idx = (samples_read[device] * chan_count[device]) -
                        chan_count[device] + i;
                    sprintf(display_string, "%12.5f V",
                        data[device][sample_idx]);
                    strcat(data_display_output, display_string);
                }
            }

            strcat(data_display_output, "\n\n");
            printf(data_display_output);
        }

        fflush(stdout);

        if (enter_press())
        {
            printf("Aborted\n\n");
            break;
        }
    }

stop:
    // Stop and cleanup
    for (device = 0; device < DEVICE_COUNT; device++)
    {
        result = mcc128_a_in_scan_stop(address[device]);
        print_error(result);
        result = mcc128_a_in_scan_cleanup(address[device]);
        print_error(result);
        result = mcc128_close(address[device]);
        print_error(result);
    }

    return 0;
}

/* This function gets the addresses of the MCC 128 devices to be used. */
int get_hat_addresses(uint8_t address[])
{
    int return_val = -1;
    int hat_count = 0;
    int device = 0;
    int i = 0;
    int address_int;

    int valid = 0;

    hat_count = hat_list(HAT_ID_MCC_128 , NULL);

    if (hat_count >= DEVICE_COUNT)
    {
        struct HatInfo* hats = (struct HatInfo*)malloc(
            hat_count * sizeof(struct HatInfo));
        hat_list(HAT_ID_MCC_128 , hats);

        if (hat_count == DEVICE_COUNT)
        {
            for (device = 0; device < DEVICE_COUNT; device++)
            {
                address[device] = hats[device].address;
            }
        }
        else
        {
            for (i = 0; i < hat_count; i++)
            {
                printf("Address %d: %s\n", hats[i].address,
                    hats[i].product_name);
            }

            for (device = 0; device < DEVICE_COUNT; device++)
            {
                valid = 0;
                while (!valid)
                {
                    printf("\n Enter address for HAT device %d: ", device);
                    scanf("%d", &address_int);

                    // Check if the address exists in the hats array
                    for (i = 0; i < hat_count; i++)
                    {
                        if (hats[i].address == address_int)
                            valid = 1;
                    }
                    // Make sure the address was not previously selected
                    for (i = 0; i < device; i++)
                    {
                        if (address[i] == address_int)
                            valid = 0;
                    }

                    if (valid)
                    {
                        address[device] = (uint8_t)address_int;
                    }
                    else
                    {
                        printf("Invalid address - try again");
                    }
                }
            }
            flush_stdin();
        }
        free(hats);
        return_val = 0;
    }
    else
    {
        fprintf(stderr, "Error: This example requires %d MCC 128 devices - "
            "found %d\n", DEVICE_COUNT, hat_count);
    }

    return return_val;
}
