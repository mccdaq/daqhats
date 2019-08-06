/*****************************************************************************

    MCC 172 Functions Demonstrated:
        mcc172_trigger_config
        mcc172_a_in_clock_config_write
        mcc172_a_in_clock_config_read
        mcc172_a_in_scan_start
        mcc172_a_in_scan_status
        mcc172_a_in_scan_read
        mcc172_a_in_scan_stop

    Purpose:
        Get synchronous data from multiple MCC 172 devices.

    Description:
        This example demonstrates acquiring data synchronously from multiple
        MCC 172 devices.  This is done using the shared clock and trigger
        options.  An external trigger source must be provided to the TRIG
        terminal on the master MCC 172 device.  The clock and trigger on the
        master device are configured for SOURCE_MASTER and the remaining devices
        are configured for SOURCE_SLAVE.

*****************************************************************************/
#include "../../daqhats_utils.h"
#include <math.h>

#define DEVICE_COUNT 2
#define MASTER 0

#define CURSOR_SAVE "\x1b[s"
#define CURSOR_RESTORE "\x1b[u"

int get_hat_addresses(uint8_t address[]);

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
    // HAT device addresses - determined at runtime
    uint8_t address[DEVICE_COUNT];

    // mcc172_a_in_scan_start() variables
    uint8_t chan_mask[DEVICE_COUNT] = {
        CHAN0 | CHAN1,
        CHAN0 | CHAN1
    };
    uint32_t options = OPTS_EXTTRIGGER;
    
    uint32_t samples_per_channel = 10240;
    double sample_rate = 10240; // Samples per second

    // mcc172_trigger_config() variable
    uint8_t trigger_mode = TRIG_RISING_EDGE;

    int mcc172_num_channels = mcc172_info()->NUM_AI_CHANNELS;

    // mcc172_a_in_scan_status() and mcc172_a_in_scan_read() variables
    uint16_t scan_status[DEVICE_COUNT] = {0};
    int buffer_size = samples_per_channel * mcc172_num_channels;
    double data[DEVICE_COUNT][buffer_size];
    uint32_t samples_read[DEVICE_COUNT] = {0};
    int32_t samples_to_read = 1000;
    double timeout = 5;  //seconds
    uint32_t samples_available = 0;

    int result = 0;
    int chan_count[DEVICE_COUNT];
    int chans[DEVICE_COUNT][mcc172_num_channels];
    int device = 0;
    int i = 0;
    int total_samples_read[DEVICE_COUNT] = {0};
    const int data_display_line_count = DEVICE_COUNT * 4;
    uint16_t is_running = 0;
    uint16_t is_triggered = 0;
    uint16_t scan_status_all = 0;

    char chan_display[DEVICE_COUNT][32];
    char options_str[256];
    char display_string[256] = "";
    char data_display_output[1024] = "";
    char c;
    uint8_t synced;
    uint8_t clock_source;

    double actual_sample_rate = 0.0;

    // Determine the addresses of the devices to be used
    if (get_hat_addresses(address) != 0)
    {
        return -1;
    }

    convert_options_to_string(options, options_str);
    
    for (device = 0; device < DEVICE_COUNT; device++)
    {
        // Open a connection to each device
        result = mcc172_open(address[device]);
        STOP_ON_ERROR(result);

        // channel_mask is used by the library function mcc172_a_in_scan_start.
        // The functions below parse the mask for display purposes.
        chan_count[device] = convert_chan_mask_to_array(chan_mask[device], 
            chans[device]);
        convert_chan_mask_to_string(chan_mask[device], chan_display[device]);
        
        // Configure the trigger
        result = mcc172_trigger_config(address[device], 
            (device == MASTER) ? SOURCE_MASTER : SOURCE_SLAVE,
            trigger_mode);
        STOP_ON_ERROR(result);
        
        // Configure the clock (slaves only)
        if (device != MASTER)
        {
            result = mcc172_a_in_clock_config_write(address[device], 
                SOURCE_SLAVE, sample_rate);
            STOP_ON_ERROR(result);
        }
    }

    // Configure the master clock last so the clocks are synchronized
    result = mcc172_a_in_clock_config_write(address[MASTER], 
        SOURCE_MASTER, sample_rate);
    STOP_ON_ERROR(result);

    // Wait for sync to complete
    do
    {
        result = mcc172_a_in_clock_config_read(address[MASTER], &clock_source,
            &actual_sample_rate, &synced);
        STOP_ON_ERROR(result);
        usleep(5000);
    } while (synced == 0);
    

    printf("\nMCC 172 multiple device example using shared clock and "
        "trigger options\n");
    printf("    Functions demonstrated:\n");
    printf("      mcc172_trigger_config\n");
    printf("      mcc172_a_in_clock_config_write\n");
    printf("      mcc172_a_in_clock_config_read\n");
    printf("      mcc172_a_in_scan_start\n");
    printf("      mcc172_a_in_scan_status\n");
    printf("      mcc172_a_in_scan_read\n");
    printf("      mcc172_a_in_scan_stop\n");
    printf("    Samples per channel: %d\n", samples_per_channel);
    printf("    Requested Sample Rate: %.3f Hz\n", sample_rate);
    printf("    Actual Sample Rate: %.3f Hz\n", actual_sample_rate);
    convert_trigger_mode_to_string(trigger_mode, display_string);
    printf("    Trigger mode: %s\n", display_string);

    for (device = 0; device < DEVICE_COUNT; device++)
    {
        printf("    MCC 172 %d:\n", device);
        printf("      Address: %d\n", address[device]);
        printf("      Channels: %s\n", chan_display[device]);
    }

    printf("\nConnect a trigger source to the TRIG input terminal on "
        "device at address %d.\n", address[0]);

    printf("\nPress 'Enter' to continue\n");

    scanf("%c", &c);

    // Start the scans
    for (device = 0; device < DEVICE_COUNT; device++)
    {
        result = mcc172_a_in_scan_start(address[device], chan_mask[device],
            samples_per_channel, options);
        STOP_ON_ERROR(result);
    }

    printf("Waiting for trigger ... Press 'Enter' to abort\n\n");

    do
    {
        usleep(10000);
        // Check the master status in a loop to determine when the trigger occurs
        result = mcc172_a_in_scan_status(address[MASTER], &scan_status[MASTER],
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
            result = mcc172_a_in_scan_read(address[device], 
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
                    // Calculate and display RMS voltage of the input data
                    sprintf(display_string, "%9.3f Vrms",
                        calc_rms(data[device], i, chan_count[device],
                            samples_read[device]));
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
        result = mcc172_a_in_scan_stop(address[device]);
        print_error(result);
        result = mcc172_a_in_scan_cleanup(address[device]);
        print_error(result);
        
        // Restore clock and trigger to local source
        result = mcc172_a_in_clock_config_write(address[device], 
            SOURCE_LOCAL, sample_rate);
        print_error(result);
        
        result = mcc172_trigger_config(address[device], 
            SOURCE_LOCAL, trigger_mode);
        print_error(result);

        result = mcc172_close(address[device]);
        print_error(result);
    }

    return 0;
}

/* This function gets the addresses of the MCC 172 devices to be used. */
int get_hat_addresses(uint8_t address[])
{
    int return_val = -1;
    int hat_count = 0;
    int device = 0;
    int i = 0;
    int address_int;

    int valid = 0;

    hat_count = hat_list(HAT_ID_MCC_172 , NULL);

    if (hat_count >= DEVICE_COUNT)
    {
        struct HatInfo* hats = (struct HatInfo*)malloc(
            hat_count * sizeof(struct HatInfo));
        hat_list(HAT_ID_MCC_172 , hats);

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
        fprintf(stderr, "Error: This example requires %d MCC 172 devices - "
            "found %d\n", DEVICE_COUNT, hat_count);
    }

    return return_val;
}
