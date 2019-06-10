/*****************************************************************************

    MCC 134 Functions Demonstrated:
        mcc134_t_in_read
        mcc134_tc_type_write
        mcc134_info

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
    uint8_t address;
    uint8_t channel;
    double value;
    uint8_t low_chan;
    uint8_t high_chan;
    uint8_t tc_type = TC_TYPE_J;    // change this to desired thermocouple type
    char c;
    int result = RESULT_SUCCESS;
    int samples_per_channel = 0;
    int delay_between_reads = 1000;  // ms
    int num_channels = mcc134_info()->NUM_AI_CHANNELS;
    char tc_type_str[10];
    
    low_chan = 0;
    high_chan = num_channels - 1;

    // Determine the address of the device to be used
    if (select_hat_device(HAT_ID_MCC_134, &address) != 0)
    {
        return -1;
    }

    // Open a connection to the device
    result = mcc134_open(address);
    STOP_ON_ERROR(result);

    for (channel = low_chan; channel <= high_chan; channel++)
    {
        result = mcc134_tc_type_write(address, channel, tc_type);
        STOP_ON_ERROR(result);
    }

    convert_tc_type_to_string(tc_type, tc_type_str);
    
    printf("\nMCC 134 single data value read example\n");
    printf("    Function demonstrated: mcc134_t_in_read\n");
    printf("    Channels: %d - %d\n", low_chan, high_chan);
    printf("    Thermocouple type: %s\n", tc_type_str);

    printf("\nPress 'Enter' to continue\n");

    scanf("%c", &c);

    printf("Acquiring data ... Press 'Enter' to abort\n\n");

    // Display the header row for the data table
    printf("  Sample");
    for (channel = low_chan; channel <= high_chan; channel++)
    {
        printf("     Channel %d", channel);
    }
    printf("\n");

    while (!enter_press())
    {
        // Display the updated samples per channel
        printf("\r%8d", ++samples_per_channel);
        // Read a single value from each selected channel
        for (channel = low_chan; channel <= high_chan; channel++)
        {
            result = mcc134_t_in_read(address, channel, &value);
            STOP_ON_ERROR(result);
            if (value == OPEN_TC_VALUE)
            {
                printf("     Open     ");
            }
            else if (value == OVERRANGE_TC_VALUE)
            {
                printf("     OverRange");
            }
            else if (value == COMMON_MODE_TC_VALUE)
            {
                printf("   Common Mode");
            }
            else
            {
                printf("%12.2f C", value);
            }
        }

        fflush(stdout);
        usleep(delay_between_reads * 1000);
    }

stop:
    result = mcc134_close(address);
    print_error(result);

    return 0;
}
