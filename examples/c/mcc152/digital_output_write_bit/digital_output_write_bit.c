/*****************************************************************************

    MCC 152 Functions Demonstrated:
        mcc152_dio_reset
        mcc152_dio_output_write_bit
        mcc152_dio_config_write_bit
        mcc152_info

    Purpose:
        Write individual digital outputs until terminated by the user.

    Description:
        This example demonstrates using the digital I/O as outputs and writing
        them individually.

*****************************************************************************/
#include <stdio.h>
#include <stdbool.h>
#include "../../daqhats_utils.h"

#define BUFFER_SIZE 5

bool get_channel(uint8_t* p_channel)
{
    char buffer[BUFFER_SIZE];
    int num_channels;
    int value;

    if (p_channel == NULL)
    {
        return false;
    }
    
    num_channels = mcc152_info()->NUM_DIO_CHANNELS;
    
    while (1)
    {
        printf("Enter a channel between 0 and %d, non-numeric character to"
            " exit: ", num_channels - 1);

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
            if (sscanf(buffer, "%d", &value) == 0)
            {
                // Not a number.
                return false;
            }
            
            // Compare the number to min and max allowed.
            if ((value < 0) || (value >= num_channels))
            {
                // Out of range, ask again.
                printf("Value out of range.\n");
            }
            else
            {
                // Valid value.
                *p_channel = (uint8_t)value;
                return true;
            }
        }
    }
}

bool get_value(uint8_t* p_value)
{
    char buffer[BUFFER_SIZE];
    int value;

    if (p_value == NULL)
    {
        return false;
    }
    
    while (1)
    {
        printf("Enter the output value, 0 or 1, non-numeric character to"
            " exit:  ");

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
            if (sscanf(buffer, "%d", &value) == 0)
            {
                // Not a number.
                return false;
            }
            
            // Compare the number to min and max allowed.
            if ((value < 0) || (value > 1))
            {
                // Out of range, ask again.
                printf("Value out of range.\n");
            }
            else
            {
                // Valid value.
                *p_value = (uint8_t)value;
                return true;
            }
        }
    }
}

bool get_input(uint8_t* p_channel, uint8_t* p_value)
{
    if ((p_value == NULL) || (p_channel == NULL))
    {
        return false;
    }
    
    if (!get_channel(p_channel))
    {
        return false;
    }
    if (!get_value(p_value))
    {
        return false;
    }
    
    printf("\n");
    return true;
}

int main(void)
{
    uint8_t address;
    int result = RESULT_SUCCESS;
    bool run_loop;
    uint8_t value;
    uint8_t channel;

    printf("\nMCC 152 digital output write example.\n");
    printf("Sets all digital I/O channels to output then gets channel and\n");
    printf("value input from the user and updates the output.\n");
    printf("   Functions demonstrated:\n");
    printf("      mcc152_dio_reset\n");
    printf("      mcc152_dio_output_write_bit\n");
    printf("      mcc152_dio_config_write_bit\n");
    printf("      mcc152_info\n\n");

    // Select the device to be used.
    if (select_hat_device(HAT_ID_MCC_152, &address) != 0)
    {
        return 1;
    }
    
    printf("\nUsing address %d.\n\n", address);
    
    // Open a connection to the device.
    result = mcc152_open(address);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        // Could not open the device - exit.
        printf("Unable to open device at address %d\n", address);
        return 1;
    }

    // Reset the DIO to defaults (all channels input, pull-up resistors
    // enabled).
    result = mcc152_dio_reset(address);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        mcc152_close(address);
        return 1;
    }
    
    // Set all channels as outputs.
    for (channel = 0; channel < mcc152_info()->NUM_DIO_CHANNELS; channel++)
    {
        result = mcc152_dio_config_write_bit(address, channel, DIO_DIRECTION, 
            0);
        if (result != RESULT_SUCCESS)
        {
            print_error(result);
            mcc152_close(address);
            return 1;
        }
    }
    
    run_loop = true;
    // Loop until the user terminates or we get a library error.
    while (run_loop)
    {
        if (get_input(&channel, &value))
        {
            // Write the value to the selected channel.
            result = mcc152_dio_output_write_bit(address, channel, value);
            if (result != RESULT_SUCCESS)
            {
                print_error(result);
                mcc152_close(address);
                return 1;
            }
        }
        else
        {
            run_loop = false;
        }
    }

    // Return the digital I/O to default settings.
    result = mcc152_dio_reset(address);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        mcc152_close(address);
        return 1;
    }
    
    // Close the device.
    result = mcc152_close(address);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        return 1;
    }

    return 0;
}
