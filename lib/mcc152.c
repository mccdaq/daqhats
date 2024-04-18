/*
*   mcc152.c
*   author Measurement Computing Corp.
*   brief This file contains functions used with the MCC 152.
*
*   06/29/2018
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "daqhats.h"
#include "util.h"
#include "cJSON.h"
#include "mcc152_dac.h"
#include "mcc152_dio.h"

//*****************************************************************************
// Constants

#define NUM_AO_CHANNELS     2       // The number of analog output channels.
#define NUM_DIO_CHANNELS    8       // The number of digital I/O channels.

#define MAX_CODE            4095
#define MAX_RANGE           (5.0)
#define LSB_SIZE            (MAX_RANGE / (MAX_CODE+1))
#define MIN_VOLTAGE         (0.0)
#define MAX_VOLTAGE         (MAX_CODE * LSB_SIZE)

struct MCC152DeviceInfo mcc152_device_info =
{
    // The number of digital I/O channels.
    8,
    // The number of analog output channels.
    2,
    // The minimum DAC code.
    0,
    // The maximum DAC code (4095.)
    MAX_CODE,
    // The output voltage corresponding to the minimum code (0.0V.)
    MIN_VOLTAGE,
    // The output voltage corresponding to the maximum code (5.0V - 1 LSB.)
    MAX_VOLTAGE,
    // The minimum voltage of the output range (0.0V.)
    MIN_VOLTAGE,
    // The maximum voltage of the output range (5.0V.)
    MAX_RANGE
};

// The maximum size of the serial number string, plus NULL.
#define SERIAL_SIZE         (8+1)

#define MIN(a, b)           ((a < b) ? a : b)
#define MAX(a, b)           ((a > b) ? a : b)

/// \cond
// Contains the device-specific data stored at the factory.
struct mcc152FactoryData
{
    // Serial number
    char serial[SERIAL_SIZE];
};

// Local data for each open MCC 152 board.
struct mcc152Device
{
    uint16_t handle_count;      // the number of handles open to this device
    struct mcc152FactoryData factory_data;   // Factory data
    uint8_t spi_device;         // which SPI device for the DAC (rev 1 boards
                                // used SPI 1, newer boards use SPI 0)
};
/// \endcond

//*****************************************************************************
// Variables

static struct mcc152Device* _devices[MAX_NUMBER_HATS];
static bool _mcc152_lib_initialized = false;

//*****************************************************************************
// Local Functions

/******************************************************************************
  Validate parameters for an address
 *****************************************************************************/
static bool _check_addr(uint8_t address)
{
    if ((address >= MAX_NUMBER_HATS) ||     // Address is invalid
        !_mcc152_lib_initialized ||         // Library is not initialized
        (_devices[address] == NULL))        // Device structure is not allocated
    {
        return false;
    }
    else
    {
        return true;
    }
}


/******************************************************************************
  Sets an mcc152FactoryData to default values.
 *****************************************************************************/
static void _set_defaults(struct mcc152FactoryData* data)
{
    if (data)
    {
        strcpy(data->serial, "00000000");
    }
}

/******************************************************************************
  Parse the factory data JSON structure. Does not recurse so we can support
  multiple processes.

  Expects a JSON structure like:

    {
        "serial": "00000000",
    }

  If it finds all of these keys it will return 1, otherwise will return 0.
 *****************************************************************************/
static int _parse_factory_data(cJSON* root, struct mcc152FactoryData* data)
{
    bool got_serial = false;
    cJSON* child;

    if (!data)
    {
        return 0;
    }

    // root should just have an object type and a child
    if ((root->type != cJSON_Object) ||
        (!root->child))
    {
        return 0;
    }

    child = root->child;

    // parse the structure
    while (child)
    {
        if (!strcmp(child->string, "serial") &&
            (child->type == cJSON_String) &&
            child->valuestring)
        {
            // Found the serial number
            strncpy(data->serial, child->valuestring, SERIAL_SIZE-1);
            got_serial = true;
        }
        child = child->next;
    }

    if (got_serial)
    {
        // Report success if all required items were found
        return 1;
    }
    else
    {
        return 0;
    }
}

/******************************************************************************
  Perform any library initialization.
 *****************************************************************************/
static void _mcc152_lib_init(void)
{
    int i;

    if (!_mcc152_lib_initialized)
    {
        for (i = 0; i < MAX_NUMBER_HATS; i++)
        {
            _devices[i] = NULL;
        }

        _mcc152_lib_initialized = true;
    }
}

//*****************************************************************************
// Global Functions

/******************************************************************************
  Open a connection to the MCC 152 device at the specified address.
 *****************************************************************************/
int mcc152_open(uint8_t address)
{
    struct HatInfo info;
    char* custom_data;
    uint16_t custom_size;
    struct mcc152Device* dev;
    int result;

    custom_data = NULL;

    _mcc152_lib_init();

    // validate the parameters
    if ((address >= MAX_NUMBER_HATS))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address] == NULL)
    {
        // this is either the first time this device is being opened or it is
        // not a 152

        // read the EEPROM file(s), verify that it is an MCC 152, and get the
        // data
        if (_hat_info(address, &info, NULL, &custom_size) == RESULT_SUCCESS)
        {
            if (info.id == HAT_ID_MCC_152)
            {
                custom_data = malloc(custom_size);
                _hat_info(address, &info, custom_data, &custom_size);
            }
            else
            {
                return RESULT_INVALID_DEVICE;
            }
        }
        else
        {
            // no EEPROM info was found - allow opening the board with an
            // uninitialized EEPROM
            custom_size = 0;
        }

        // create a struct to hold device instance data
        _devices[address] = (struct mcc152Device*)calloc(1,
            sizeof(struct mcc152Device));
        dev = _devices[address];

        // initialize the struct elements
        dev->handle_count = 1;
        dev->spi_device = 0;

        if (custom_size > 0)
        {
            // if the EEPROM is initialized then use the version to determine
            // which SPI device is used for the DAC
            if (info.version == 1)
            {
                dev->spi_device = 1;
            }

            // convert the JSON custom data to parameters
            cJSON* root = cJSON_Parse(custom_data);
            if (root == NULL)
            {
                // error parsing the JSON data
                _set_defaults(&dev->factory_data);
                printf("Warning - address %d using factory EEPROM default "
                    "values\n", address);
            }
            else
            {
                if (!_parse_factory_data(root, &dev->factory_data))
                {
                    // invalid custom data, use default values
                    _set_defaults(&dev->factory_data);
                    printf("Warning - address %d using factory EEPROM default "
                        "values\n", address);
                }
                cJSON_Delete(root);
            }

            free(custom_data);
        }
        else
        {
            // use default parameters, board probably has an empty EEPROM.
            _set_defaults(&dev->factory_data);
            printf("Warning - address %d using factory EEPROM default "
                "values\n", address);
        }

        // initialize the DAC
        if ((result = _mcc152_dac_init(dev->spi_device, address)) !=
            RESULT_SUCCESS)
        {
            mcc152_close(address);
            return result;
        }

        // initialize the DIO
        if ((result = _mcc152_dio_init(address)) != RESULT_SUCCESS)
        {
            mcc152_close(address);
            return result;
        }
    }
    else
    {
        // the device has already been opened and initialized, increment
        // reference count
        dev = _devices[address];
        dev->handle_count++;
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Check if an MCC 152is open.
 *****************************************************************************/
int mcc152_is_open(uint8_t address)
{
    if ((address >= MAX_NUMBER_HATS) ||
        (_devices[address] == NULL))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


/******************************************************************************
  Close a connection to an MCC 152 device and free allocated resources.
 *****************************************************************************/
int mcc152_close(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    _devices[address]->handle_count--;
    if (_devices[address]->handle_count == 0)
    {
        free(_devices[address]);
        _devices[address] = NULL;
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Return the device info struct.
 *****************************************************************************/
struct MCC152DeviceInfo* mcc152_info(void)
{
    return &mcc152_device_info;
}

/******************************************************************************
  Read the serial number.
 *****************************************************************************/
int mcc152_serial(uint8_t address, char* buffer)
{
    // validate parameters
    if (!_check_addr(address) ||
        (buffer == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    strcpy(buffer, _devices[address]->factory_data.serial);
    return RESULT_SUCCESS;
}

/******************************************************************************
  Write an analog output channel.
 *****************************************************************************/
int mcc152_a_out_write(uint8_t address, uint8_t channel, uint32_t options,
    double value)
{
    uint16_t code;

    if (!_check_addr(address) ||
        (channel >= NUM_AO_CHANNELS))
    {
        return RESULT_BAD_PARAMETER;
    }

    if ((options & OPTS_NOSCALEDATA) == 0)
    {
        // user passed voltage
        if (value < 0.0)
        {
            value = 0.0;
        }
        else if (value > MAX_VOLTAGE)
        {
            value = MAX_VOLTAGE;
        }
        code = (uint16_t)((value / LSB_SIZE) + 0.5);
    }
    else
    {
        // user passed code
        if (value < 0.0)
        {
            value = 0.0;
        }
        else if (value > MAX_CODE)
        {
            value = MAX_CODE;
        }
        code = (uint16_t)(value + 0.5);
    }

    return _mcc152_dac_write(_devices[address]->spi_device, address, channel,
        code);
}

/******************************************************************************
  Write all analog output channels.
 *****************************************************************************/
int mcc152_a_out_write_all(uint8_t address, uint32_t options, double* values)
{
    uint16_t codes[NUM_AO_CHANNELS];
    int i;

    if (!_check_addr(address) ||
        (values == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    for (i = 0; i < NUM_AO_CHANNELS; i++)
    {
        if ((options & OPTS_NOSCALEDATA) == 0)
        {
            // user passed voltages
            if (values[i] < 0.0)
            {
                values[i] = 0.0;
            }
            else if (values[i] > MAX_VOLTAGE)
            {
                values[i] = MAX_VOLTAGE;
            }
            codes[i] = (uint16_t)((values[i] / LSB_SIZE) + 0.5);
        }
        else
        {
            // user passed codes
            if (values[i] < 0.0)
            {
                values[i] = 0.0;
            }
            else if (values[i] > MAX_CODE)
            {
                values[i] = MAX_CODE;
            }
            codes[i] = (uint16_t)(values[i] + 0.5);
        }
    }

    return _mcc152_dac_write_both(_devices[address]->spi_device, address,
        codes[0], codes[1]);
}

/******************************************************************************
  Reset DIO to default configuration.
 *****************************************************************************/
int mcc152_dio_reset(uint8_t address)
{
    int ret;

    if (!_check_addr(address))                // check address failed
    {
        return RESULT_BAD_PARAMETER;
    }

    // write the register values

    // interrupt mask
    ret = _mcc152_dio_reg_write(address, DIO_REG_INT_MASK, DIO_CHANNEL_ALL,
        0xFF, false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // switch to inputs
    ret = _mcc152_dio_reg_write(address, DIO_REG_CONFIG, DIO_CHANNEL_ALL, 0xFF,
        false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // pull-up setting
    ret = _mcc152_dio_reg_write(address, DIO_REG_PULL_SELECT, DIO_CHANNEL_ALL,
        0xFF, false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // pull-up enable
    ret = _mcc152_dio_reg_write(address, DIO_REG_PULL_ENABLE, DIO_CHANNEL_ALL,
        0xFF, false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // input invert
    ret = _mcc152_dio_reg_write(address, DIO_REG_POLARITY, DIO_CHANNEL_ALL, 0,
        false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // input latch
    ret = _mcc152_dio_reg_write(address, DIO_REG_INPUT_LATCH, DIO_CHANNEL_ALL,
        0, false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // output type
    ret = _mcc152_dio_reg_write(address, DIO_REG_OUTPUT_CONFIG, DIO_CHANNEL_ALL,
        0, false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // output latch
    ret = _mcc152_dio_reg_write(address, DIO_REG_OUTPUT_PORT, DIO_CHANNEL_ALL,
        0xFF, false);
    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Read a single DIO input.
 *****************************************************************************/
int mcc152_dio_input_read_bit(uint8_t address, uint8_t channel, uint8_t* value)
{
    if (!_check_addr(address) ||
        (channel >= NUM_DIO_CHANNELS) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, DIO_REG_INPUT_PORT, channel, value);
}

/******************************************************************************
  Read all DIO inputs.
 *****************************************************************************/
int mcc152_dio_input_read_port(uint8_t address, uint8_t* values)
{
    if (!_check_addr(address) ||
        (values == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, DIO_REG_INPUT_PORT, DIO_CHANNEL_ALL,
        values);
}

/******************************************************************************
  Write a single DIO output.
 *****************************************************************************/
int mcc152_dio_output_write_bit(uint8_t address, uint8_t channel, uint8_t value)
{
    if (!_check_addr(address) ||
        (channel >= NUM_DIO_CHANNELS))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_write(address, DIO_REG_OUTPUT_PORT, channel, value,
        true);
}

/******************************************************************************
  Write all DIO outputs.
 *****************************************************************************/
int mcc152_dio_output_write_port(uint8_t address, uint8_t values)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_write(address, DIO_REG_OUTPUT_PORT, DIO_CHANNEL_ALL,
        values, true);
}


/******************************************************************************
  Read a single DIO output.
 *****************************************************************************/
int mcc152_dio_output_read_bit(uint8_t address, uint8_t channel, uint8_t* value)
{
    if (!_check_addr(address) ||
        (channel >= NUM_DIO_CHANNELS) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, DIO_REG_OUTPUT_PORT, channel, value);
}

/******************************************************************************
  Read all DIO outputs.
 *****************************************************************************/
int mcc152_dio_output_read_port(uint8_t address, uint8_t* values)
{
    if (!_check_addr(address) ||
        (values == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, DIO_REG_OUTPUT_PORT, DIO_CHANNEL_ALL,
        values);
}

/******************************************************************************
  Read the interrupt status for a single channel.
 *****************************************************************************/
int mcc152_dio_int_status_read_bit(uint8_t address, uint8_t channel,
    uint8_t* value)
{
    if (!_check_addr(address) ||
        (channel >= NUM_DIO_CHANNELS) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, DIO_REG_INT_STATUS, channel,
        value);
}

/******************************************************************************
  Read the interrupt status for all channels.
 *****************************************************************************/
int mcc152_dio_int_status_read_port(uint8_t address, uint8_t* value)
{
    if (!_check_addr(address) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, DIO_REG_INT_STATUS, DIO_CHANNEL_ALL,
        value);
}

/******************************************************************************
  Write a DIO configuration setting for a single channel.
 *****************************************************************************/
int mcc152_dio_config_write_bit(uint8_t address, uint8_t channel, uint8_t item,
    uint8_t value)
{
    uint8_t reg;
    bool cache;
    uint8_t chan;

    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    cache = false;
    chan = channel;

    switch (item)
    {
    case DIO_DIRECTION:
        reg = DIO_REG_CONFIG;
        cache = true;
        break;
    case DIO_PULL_CONFIG:
        reg = DIO_REG_PULL_SELECT;
        break;
    case DIO_PULL_ENABLE:
        reg = DIO_REG_PULL_ENABLE;
        break;
    case DIO_INPUT_INVERT:
        reg = DIO_REG_POLARITY;
        break;
    case DIO_INPUT_LATCH:
        reg = DIO_REG_INPUT_LATCH;
        break;
    case DIO_OUTPUT_TYPE:
        reg = DIO_REG_OUTPUT_CONFIG;
        chan = DIO_CHANNEL_ALL;
        // force to 0 or 1
        if (value > 1)
        {
            value = 1;
        }
        break;
    case DIO_INT_MASK:
        reg = DIO_REG_INT_MASK;
        break;
    default:
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_write(address, reg, chan, value, cache);
}

/******************************************************************************
  Write a DIO configuration setting for all channels.
 *****************************************************************************/
int mcc152_dio_config_write_port(uint8_t address, uint8_t item, uint8_t value)
{
    uint8_t reg;

    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    switch (item)
    {
    case DIO_DIRECTION:
        reg = DIO_REG_CONFIG;
        break;
    case DIO_PULL_CONFIG:
        reg = DIO_REG_PULL_SELECT;
        break;
    case DIO_PULL_ENABLE:
        reg = DIO_REG_PULL_ENABLE;
        break;
    case DIO_INPUT_INVERT:
        reg = DIO_REG_POLARITY;
        break;
    case DIO_INPUT_LATCH:
        reg = DIO_REG_INPUT_LATCH;
        break;
    case DIO_OUTPUT_TYPE:
        reg = DIO_REG_OUTPUT_CONFIG;
        // force to 0 or 1
        if (value > 1)
        {
            value = 1;
        }
        break;
    case DIO_INT_MASK:
        reg = DIO_REG_INT_MASK;
        break;
    default:
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_write(address, reg, DIO_CHANNEL_ALL, value, true);
}

/******************************************************************************
  Read a DIO configuration setting for a single channel.
 *****************************************************************************/
int mcc152_dio_config_read_bit(uint8_t address, uint8_t channel, uint8_t item,
    uint8_t* value)
{
    uint8_t reg;
    uint8_t chan;

    chan = channel;

    if (!_check_addr(address) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    switch (item)
    {
    case DIO_DIRECTION:
        reg = DIO_REG_CONFIG;
        break;
    case DIO_PULL_CONFIG:
        reg = DIO_REG_PULL_SELECT;
        break;
    case DIO_PULL_ENABLE:
        reg = DIO_REG_PULL_ENABLE;
        break;
    case DIO_INPUT_INVERT:
        reg = DIO_REG_POLARITY;
        break;
    case DIO_INPUT_LATCH:
        reg = DIO_REG_INPUT_LATCH;
        break;
    case DIO_OUTPUT_TYPE:
        reg = DIO_REG_OUTPUT_CONFIG;
        chan = DIO_CHANNEL_ALL;
        break;
    case DIO_INT_MASK:
        reg = DIO_REG_INT_MASK;
        break;
    default:
        return RESULT_BAD_PARAMETER;
    }

    return _mcc152_dio_reg_read(address, reg, chan, value);
}

/******************************************************************************
  Read a DIO configuration setting for all channels.
 *****************************************************************************/
int mcc152_dio_config_read_port(uint8_t address, uint8_t item, uint8_t* value)
{
    uint8_t reg;
    uint8_t myval;
    int result;

    if (!_check_addr(address) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    switch (item)
    {
    case DIO_DIRECTION:
        reg = DIO_REG_CONFIG;
        break;
    case DIO_PULL_CONFIG:
        reg = DIO_REG_PULL_SELECT;
        break;
    case DIO_PULL_ENABLE:
        reg = DIO_REG_PULL_ENABLE;
        break;
    case DIO_INPUT_INVERT:
        reg = DIO_REG_POLARITY;
        break;
    case DIO_INPUT_LATCH:
        reg = DIO_REG_INPUT_LATCH;
        break;
    case DIO_OUTPUT_TYPE:
        reg = DIO_REG_OUTPUT_CONFIG;
        break;
    case DIO_INT_MASK:
        reg = DIO_REG_INT_MASK;
        break;
    default:
        return RESULT_BAD_PARAMETER;
    }

    result = _mcc152_dio_reg_read(address, reg, DIO_CHANNEL_ALL, &myval);
    if (result != RESULT_SUCCESS)
    {
        return result;
    }

    if (item == DIO_OUTPUT_TYPE)
    {
        if (myval == 0x00)
        {
            *value = 0x00;
        }
        else
        {
            *value = 0xFF;
        }
    }
    else
    {
        *value = myval;
    }
    return RESULT_SUCCESS;
}
