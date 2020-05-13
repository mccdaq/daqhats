/*
    This file contains functions used in the MCC DAQ HAT C examples
    to assist in displaying information, reading user inputs and
    handling errors.
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <daqhats/daqhats.h>

// Macros to handle error checking
#define STOP_ON_ERROR(result)\
{\
    if (result != RESULT_SUCCESS ){\
        print_error(result);\
        goto stop;\
    }\
}

// Channel definitions
#define CHAN0 0x01 << 0
#define CHAN1 0x01 << 1
#define CHAN2 0x01 << 2
#define CHAN3 0x01 << 3
#define CHAN4 0x01 << 4
#define CHAN5 0x01 << 5
#define CHAN6 0x01 << 6
#define CHAN7 0x01 << 7
#define MAX_CHAN_ARRAY_LENGTH 32

// Timeout definitions
#define WAIT_INDEFINITELY   -1
#define RETURN_IMMEDIATELY  0

// Read definitions
#define READ_ALL_AVAILABLE  -1

/****************************************************************************
 * Conversion functions
 ****************************************************************************/

/* This function converts the mask of options defined by the options parameter
   and sets the options_str parameter, which is passed by reference, to a
   comma separated string respresentation of the options. */
void convert_options_to_string(uint32_t options, char* options_str)
{
    if (options_str == NULL)
        return;

    strcpy(options_str, "");

    if (options == OPTS_DEFAULT)
    {
        strcat(options_str, "OPTS_DEFAULT");
    }
    else
    {
        if (options & OPTS_NOSCALEDATA)
        {
            strcat(options_str, "OPTS_NOSCALEDATA, ");
        }
        if (options & OPTS_NOCALIBRATEDATA)
        {
            strcat(options_str, "OPTS_NOCALIBRATEDATA, ");
        }
        if (options & OPTS_EXTCLOCK)
        {
            strcat(options_str, "OPTS_EXTCLOCK, ");
        }
        if (options & OPTS_EXTTRIGGER)
        {
            strcat(options_str, "OPTS_EXTTRIGGER, ");
        }
        if (options & OPTS_CONTINUOUS)
        {
            strcat(options_str, "OPTS_CONTINUOUS, ");
        }
        *strrchr(options_str, ',')= '\0';
    }
}

/* This function converts the trigger mode defined by the trigger_mode
   parameter to a string representation and returns the string
   respresentation of the trigger mode. */
void convert_trigger_mode_to_string(uint8_t trigger_mode,
    char* trigger_mode_str)
{
    if (trigger_mode_str == NULL)
        return;

    switch (trigger_mode)
    {
    case TRIG_FALLING_EDGE:
        strcpy(trigger_mode_str, "TRIG_FALLING_EDGE");
        break;
    case TRIG_ACTIVE_HIGH:
        strcpy(trigger_mode_str, "TRIG_ACTIVE_HIGH");
        break;
    case TRIG_ACTIVE_LOW:
        strcpy(trigger_mode_str, "TRIG_ACTIVE_LOW");
        break;
    case TRIG_RISING_EDGE:
    default:
        strcpy(trigger_mode_str, "TRIG_RISING_EDGE");
        break;
    }
    return;
}

/* This function converts the analog input mode defined by the mode
   parameter to a string representation and returns the string
   respresentation of the input mode. */
void convert_input_mode_to_string(uint8_t mode, char* mode_str)
{
    if (mode_str == NULL)
        return;

    switch (mode)
    {
    case A_IN_MODE_SE:
        strcpy(mode_str, "SINGLE_ENDED");
        break;
    case A_IN_MODE_DIFF:
    default:
        strcpy(mode_str, "DIFFERENTIAL");
        break;
    }
    return;
}

/* This function converts the analog input range defined by the range
   parameter to a string representation and returns the string
   respresentation of the input range. */
void convert_input_range_to_string(uint8_t range, char* range_str)
{
    if (range_str == NULL)
        return;

    switch (range)
    {
    case A_IN_RANGE_BIP_10V:
        strcpy(range_str, "+/- 10 V");
        break;
    case A_IN_RANGE_BIP_5V:
        strcpy(range_str, "+/- 5 V");
        break;
    case A_IN_RANGE_BIP_2V:
        strcpy(range_str, "+/- 2 V");
        break;
    case A_IN_RANGE_BIP_1V:
    default:
        strcpy(range_str, "+/- 1 V");
        break;
    }
    return;
}

/* This function converts the thermocouple type defined by the tc_type
   parameter to a string representation and returns the string
   respresentation. */
void convert_tc_type_to_string(uint8_t tc_type,
    char* tc_type_str)
{
    if (tc_type_str == NULL)
        return;

    switch (tc_type)
    {
    case TC_TYPE_J:
        strcpy(tc_type_str, "J");
        break;
    case TC_TYPE_K:
        strcpy(tc_type_str, "K");
        break;
    case TC_TYPE_T:
        strcpy(tc_type_str, "T");
        break;
    case TC_TYPE_E:
        strcpy(tc_type_str, "E");
        break;
    case TC_TYPE_R:
        strcpy(tc_type_str, "R");
        break;
    case TC_TYPE_S:
        strcpy(tc_type_str, "S");
        break;
    case TC_TYPE_B:
        strcpy(tc_type_str, "B");
        break;
    case TC_TYPE_N:
        strcpy(tc_type_str, "N");
        break;
    case TC_DISABLED:
    default:
        strcpy(tc_type_str, "DISABLED");
        break;
    }
    return;
}


/* This function converts the mask of channels defined by the channel_mask
   parameter and sets the chans_str parameter, which is passed by reference,
   to a comma separated string respresentation of the channel numbers. */
void convert_chan_mask_to_string(uint32_t channel_mask, char* chans_str)
{
    int i = 0;
    char chan_string[8];

    if (chans_str == NULL)
        return;

    strcpy(chans_str, "");

    while (channel_mask > 0)
    {
        if (channel_mask & 0x01)
        {
            sprintf(chan_string, "%d, ", i);
            strcat(chans_str, chan_string);
        }
        i++;
        channel_mask >>= 1;
    }
    *strrchr(chans_str, ',')= '\0';
}

/* This function converts the mask of channels defined by the channel_mask
   parameter and sets the chans parameter, which is passed by reference,
   to an array of channel numbers.
   The return value is an integer representing the number of channels. */
int convert_chan_mask_to_array(uint32_t channel_mask, int chans[])
{
    int i = 0;
    int chan_count = 0;

    if (chans == NULL)
        return 0;

    while (channel_mask > 0)
    {
        if (channel_mask & 0x01)
        {
            chans[chan_count] = i;
            chan_count++;
        }
        i++;
        channel_mask >>= 1;
    }

    return chan_count;
}

/****************************************************************************
 * Display functions
 ****************************************************************************/
/* This function takes a result code as the result parameter and if the
   result code is not RESULT_SUCCESS, the error message is sent to stderr. */
void print_error(int result)
{
    if (result != RESULT_SUCCESS)
    {
        fprintf(stderr, "\nError: %s\n", hat_error_message(result));
    }
}

void resetCursor() {printf("\033[1;1H");}
void clearEOL() {printf("\033[2K");}
void cursorUp() {printf("\033[A");}

/****************************************************************************
 * User input functions
 ****************************************************************************/
void flush_stdin(void)
{
    int c;

    do
    {
        c = getchar();
    } while (c != '\n' && c != EOF);
}

int enter_press()
{
    int stdin_value = 0;
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    stdin_value = FD_ISSET(STDIN_FILENO, &fds);
    if (stdin_value != 0)
    {
        flush_stdin();
    }

    return stdin_value;
}

/* This function displays the available DAQ HAT devices and allows the user
   to select a device to use with the associated example.  The address
   parameter, which is passed by reference, is set to the selected address.
   The return value is 0 for success and -1 for error.*/
int select_hat_device(uint16_t hat_filter_id, uint8_t* address)
{
    struct HatInfo* hats = NULL;
    int hat_count = 0;
    int address_int = 0;
    int return_val = -1;
    int i;

    if (address == NULL)
        return -1;

    // Get the number of HAT devices that are connected that are of the
    // requested type.
    hat_count = hat_list(hat_filter_id , NULL);

    // Verify there are HAT devices connected that are of the requested type.
    if (hat_count > 0)
    {
        // Allocate memory for the list of HAT devices.
        hats = (struct HatInfo*)malloc(hat_count * sizeof(struct HatInfo));

        // Get the list of HAT devices.
        hat_list(hat_filter_id, hats);

        if (hat_count == 1)
        {
            // Get the address of the only HAT device.
            *address = hats[0].address;
            return_val = 0;
        }
        else
        {
            // There is more than 1 HAT device so display the
            // list of devices and let the user choose one.
            for (i = 0; i < hat_count; i++)
            {
                printf("Address %d: %s\n", hats[i].address,
                    hats[i].product_name);
            }

            printf("\nSelect the address of the HAT device to use: ");
            if (scanf("%d", &address_int) == 0)
            {
                fprintf(stderr, "Error: Invalid selection\n");
                free(hats);
                return -1;
            }

            *address = (uint8_t)address_int;

            // Find the HAT device with the specified address in the list.
            for (i = 0; i < hat_count; i++)
            {
                if (hats[i].address == *address)
                {
                    return_val = 0;
                    break;
                }
            }

            if (return_val != 0)
            {
                fprintf(stderr, "Error: Invalid HAT address\n");
            }
            flush_stdin();
        }

        // Release the memory used by the HatInfo list.
        if (hats != NULL)
        {
            free(hats);
        }
    }
    else
    {
        fprintf(stderr, "Error: No HAT devices found\n");
    }

    return return_val;
}

#endif /* UTILITY_H_ */
