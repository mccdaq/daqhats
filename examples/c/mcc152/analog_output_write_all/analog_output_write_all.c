/*****************************************************************************

    MCC 152 Functions Demonstrated:
        mcc152_a_out_write_all
        mcc152_info

    Purpose:
        Write values to both analog outputs in a loop.

    Description:
        This example demonstrates writing output data to both outputs
        simultaneously.

*****************************************************************************/
#include <stdbool.h>
#include "../../daqhats_utils.h"

#define OPTIONS     OPTS_DEFAULT    // default output options (voltage), set to 
                                    // OPTS_NOSCALEDATA to use DAC codes

#define BUFFER_SIZE         32

bool get_channel_value(double* p_value, int channel)
{
    char buffer[BUFFER_SIZE];
    double min;
    double max;
    double value;

    if (p_value == NULL)
    {
        return false;
    }
    
    // Get the min and max voltage/code values for the analog outputs to 
    // validate the user input.
    if ((OPTIONS & OPTS_NOSCALEDATA) == 0)
    {
        min = mcc152_info()->AO_MIN_RANGE;
        max = mcc152_info()->AO_MAX_RANGE;
    }
    else
    {
        min = (double)mcc152_info()->AO_MIN_CODE;
        max = (double)mcc152_info()->AO_MAX_CODE;
    }

    while (1)
    {
        printf("   Ch %d: ", channel);

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
        {
            // Empty string or input error.
            return false;
        }
        else
        {
            if (buffer[0] == '\n')
            {
                // Enter.
                return false;
            }
            
            // Got a string - convert to a number.
            if (sscanf(buffer, "%lf", &value) == 0)
            {
                // Not a number.
                return false;
            }
            
            // Compare the number to min and max allowed.
            if ((value < min) || (value > max))
            {
                // Out of range, ask again.
                printf("Value out of range.\n");
            }
            else
            {
                // Valid value.
                *p_value = value;
                return true;
            }
        }
    }
}

bool get_input_values(double* p_values, int count)
{
    int channel;
    
    if (p_values == NULL)
    {
        return false;
    }
    
    if ((OPTIONS & OPTS_NOSCALEDATA) == 0)
    {
        printf("Enter values between %.1f and %.1f, non-numeric character to "
            "exit:\n", 
            mcc152_info()->AO_MIN_RANGE, 
            mcc152_info()->AO_MAX_RANGE);
    }
    else
    {
        printf("Enter values between %u and %u, non-numeric character to "
            "exit:\n", 
            mcc152_info()->AO_MIN_CODE, 
            mcc152_info()->AO_MAX_CODE);
    }

    for (channel = 0; channel < count; channel++)
    {
        if (!get_channel_value(&p_values[channel], channel))
        {
            return false;
        }
    }
    
    return true;
}

int main()
{
    uint8_t address;
    int result = RESULT_SUCCESS;
    double *values;
    bool error;
    bool run_loop;
    int channel;
    int num_channels;
    char options_str[256];

    printf("\nMCC 152 all channel analog output example.\n");
    printf("Writes the specified voltages to the analog outputs.\n");
    printf("   Functions demonstrated:\n");
    printf("      mcc152_a_out_write_all\n");
    printf("      mcc152_info\n");

    convert_options_to_string(OPTIONS, options_str);
    printf("   Options: %s\n\n", options_str);

    // Select the device to be used.
    if (select_hat_device(HAT_ID_MCC_152, &address) != 0)
    {
        return 1;
    }
    
    printf("\nUsing address %d.\n", address);
    
    // Open a connection to the device.
    result = mcc152_open(address);
    print_error(result);
    if (result != RESULT_SUCCESS)
    {
        // Could not open the device - exit.
        printf("Unable to open device at address %d\n", address);
        return 1;
    }
        
    // Allocate memory for the values.
    num_channels = mcc152_info()->NUM_AO_CHANNELS;
    values = (double*)malloc(num_channels * sizeof(double));
    if (values == NULL)
    {
        mcc152_close(address);
        printf("Could not allocate memory.\n");
        return 1;
    }
    
    error = false;
    run_loop = true;
    // Loop until the user terminates or we get a library error.
    while (run_loop && !error)
    {
        if (get_input_values(values, num_channels))
        {
            result = mcc152_a_out_write_all(address, OPTIONS, values);
            if (result != RESULT_SUCCESS)
            {
                print_error(result);
                error = true;
            }
        }
        else
        {
            run_loop = false;
        }
    }

    // If there was no library error reset the output to 0V.
    if (!error)
    {
        for (channel = 0; channel < mcc152_info()->NUM_AO_CHANNELS; channel++)
        {
            values[channel] = 0.0;
        }
        result = mcc152_a_out_write_all(address, OPTIONS, values);
        print_error(result);
    }
    
    // Close the device.
    result = mcc152_close(address);
    print_error(result);
    
    // Free allocated memory.
    free(values);
    
    return (int)error;
}
