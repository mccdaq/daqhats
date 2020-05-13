/*****************************************************************************

    MCC 128 Functions Demonstrated:
        mcc128_a_in_read
        mcc128_a_in_mode_write
        mcc128_a_in_range_write

    Purpose:
        Read a single data value for each channel in a loop.

    Description:
        This example demonstrates acquiring data using a software timed loop
        to read a single value from each selected channel on each iteration
        of the loop.

*****************************************************************************/
#include "../../daqhats_utils.h"

int main()
{
    // mcc128_a_in_read() variables
    uint8_t address;
    uint8_t channel = 0;
    uint32_t options = OPTS_DEFAULT;
    double value;
    char mode_string[32];
    char range_string[32];

    uint8_t low_chan = 0;
    uint8_t high_chan = 3;

    char display_string[256] = "";
    char c;

    int result = RESULT_SUCCESS;
    int samples_per_channel = 0;
    int sample_interval = 500;  // ms
    uint8_t input_mode = A_IN_MODE_SE;
    uint8_t input_range = A_IN_RANGE_BIP_10V;

    int mcc128_num_channels = mcc128_info()->NUM_AI_CHANNELS[input_mode];

    // Ensure low_chan and high_chan are valid
    if ((low_chan >= mcc128_num_channels) ||
        (high_chan >= mcc128_num_channels))
    {
        fprintf(stderr, "Error: Invalid channel - must be 0 - %d.\n",
            mcc128_num_channels - 1);
        return -1;
    }
    if (low_chan > high_chan)
    {
        fprintf(stderr, "Error: Invalid channels - "
            "high_chan must be greater than or equal to low_chan\n");
        return -1;
    }

    // Determine the address of the device to be used
    if (select_hat_device(HAT_ID_MCC_128, &address) != 0)
    {
        return -1;
    }

    // Open a connection to each device
    result = mcc128_open(address);
    STOP_ON_ERROR(result);

    result = mcc128_a_in_mode_write(address, input_mode);
    STOP_ON_ERROR(result);

    result = mcc128_a_in_range_write(address, input_range);
    STOP_ON_ERROR(result);

    convert_input_mode_to_string(input_mode, mode_string);
    convert_input_range_to_string(input_range, range_string);

    printf("\nMCC 128 single data value read example\n");
    printf("    Function demonstrated:\n");
    printf("        mcc128_a_in_read\n");
    printf("        mcc128_a_in_mode_write\n");
    printf("        mcc128_a_in_range_write\n");
    printf("    Input mode: %s\n", mode_string);
    printf("    Input range: %s\n", range_string);
    printf("    Channels: %d - %d\n", low_chan, high_chan);
    convert_options_to_string(options, display_string);
    printf("    Options: %s\n", display_string);

    printf("\nPress 'Enter' to continue\n");

    scanf("%c", &c);

    printf("Acquiring data ... Press 'Enter' to abort\n\n");

    // Display the header row for the data table
    printf("  Samples/Channel");
    for (channel = low_chan; channel <= high_chan; channel++)
    {
        printf("     Channel %d", channel);
    }
    printf("\n");

    while (!enter_press())
    {
        // Display the updated samples per channel
        printf("\r%17d", ++samples_per_channel);
        // Read a single value from each selected channel
        for (channel = low_chan; channel <= high_chan; channel++)
        {
            result = mcc128_a_in_read(address, channel, options, &value);
            STOP_ON_ERROR(result);
            printf("%12.5f V", value);
        }

        fflush(stdout);
        usleep(sample_interval * 1000);
    }

stop:
    result = mcc128_close(address);
    print_error(result);

    return 0;
}
