/*
*   mcc134.c
*   author Measurement Computing Corp.
*   brief This file contains functions used with the MCC 134.
*
*   date 2/7/2019
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "daqhats.h"
#include "util.h"
#include "cJSON.h"
#include "mcc134_adc.h"
#include "nist.h"

//#define DEBUG_CJC

//*****************************************************************************
// Constants
#define NUM_TC_CHANNELS     4       // The number of analog input channels.

#define PGA_GAIN            32
#define REFERENCE_VOLTS     2.5
#define MAX_CODE            (8388607L)
#define MIN_CODE            (-8388608L)
#define RANGE_MIN           (-0.078125)
#define RANGE_MAX           (+0.078125)
#define LSB_SIZE            ((RANGE_MAX - RANGE_MIN)/(MAX_CODE+1-MIN_CODE))
#define VOLTAGE_MIN         RANGE_MIN
#define VOLTAGE_MAX         (RANGE_MAX - LSB_SIZE)

#define OPEN_TC_MASK        0x00FFFFFF
#define OPEN_TC_CODE        0x007FFFFF
#define COMMON_MODE_MASK    0x3C000000

#define CJC_MAX_CODE        0x7FFFFF

#define OPEN_TC_VOLTAGE     VOLTAGE_MAX

#define POS_OVERRANGE_VOLTS 0.075
#define NEG_OVERRANGE_VOLTS -0.015

// The maximum size of the serial number string, plus NULL.
#define SERIAL_SIZE         (8+1)
// The maximum size of the calibration date string, plus NULL.
#define CAL_DATE_SIZE       (10+1)

#define MIN(a, b)           ((a < b) ? a : b)
#define MAX(a, b)           ((a > b) ? a : b)

// Constants used by the CJC

#define INTERPOLATE_CJC

#define CJC_REF_R               10000       // CJC reference resistance

#define STEINHART_A             (1.2873851e-3)
#define STEINHART_B             (2.3575235e-4)
#define STEINHART_C             (9.4978060e-8)

// CJC average window in samples
#define CJC_AVERAGE_COUNT       30
// CJC time between reading each sensor and the next
#define CJC_INTER_TIME_MS       1
// CJC conversion time assuming 20sps ADC datarate & global chop enabled
#define CJC_CONVERSION_TIME_MS  114

// TC time between reading each input and the next
#define TC_INTER_TIME_MS        1
// TC conversion time
#define TC_CONVERSION_TIME_MS   114


struct MCC134DeviceInfo mcc134_device_info =
{
    // The number of analog input channels.
    NUM_TC_CHANNELS,
    // The minimum uncalibrated ADC code (-8388608.)
    MIN_CODE,
    // The maximum uncalibrated ADC code (8388607.)
    MAX_CODE,
    // The input voltage corresponding to the minimum code (-0.15625V.)
    VOLTAGE_MIN,
    // The input voltage corresponding to the maximum code (+0.15625V - 1 LSB.)
    VOLTAGE_MAX,
    // The minimum voltage of the input range (-0.15625V.)
    RANGE_MIN,
    // The maximum voltage of the input range (+0.15625V.)
    RANGE_MAX
};


// Map the channel inputs to the ADC pins
const uint8_t TC_CHAN_HI[NUM_TC_CHANNELS] = {7, 5, 3, 1};
const uint8_t TC_CHAN_LO[NUM_TC_CHANNELS] = {6, 4, 2, 0};

#define NUM_CJC_SENSORS    3       // The number of CJC input channels
const uint8_t CJC_CHAN_HI[NUM_CJC_SENSORS] = {8, 8, 8};
const uint8_t CJC_CHAN_LO[NUM_CJC_SENSORS] = {9, 10, 11};

// Map the CJC channels to the associated TC input
const uint8_t CJC_MAP[NUM_TC_CHANNELS] = {0, 1, 1, 2};

/// \cond
// Contains the device-specific data stored at the factory.
struct mcc134FactoryData
{
    // Serial number
    char serial[SERIAL_SIZE];
    // Calibration date in the format 2017-09-19
    char cal_date[CAL_DATE_SIZE];
    // Calibration coefficients - per channel slopes
    double slopes[NUM_TC_CHANNELS];
    // Calibration coefficents - per channel offsets
    double offsets[NUM_TC_CHANNELS];
};

// Local data for each open MCC 134 board.
struct mcc134Device
{
    uint8_t address;
    // the number of handles open to this device
    uint16_t handle_count;
    // the thermocouple types
    uint8_t tc_types[NUM_TC_CHANNELS];

    uint8_t update_interval;
    
    pthread_mutex_t tc_mutex;

    uint32_t tc_codes[NUM_TC_CHANNELS];
    bool tc_reset;
    bool tc_valid;
    int tc_result;
    bool stop_tc_thread;
    pthread_t tc_handle;

    uint32_t cjc_codes[NUM_CJC_SENSORS];

    int cjc_result;
    // true when the CJC sensor readings are valid
    bool cjc_valid;

    // Factory data
    struct mcc134FactoryData factory_data;
};

/// \endcond

//*****************************************************************************
// Variables

static struct mcc134Device* _devices[MAX_NUMBER_HATS];
static bool _mcc134_lib_initialized = false;

//*****************************************************************************
// Local Functions

/******************************************************************************
  Validate parameters for an address
 *****************************************************************************/
static bool _check_addr(uint8_t address)
{
    if ((address >= MAX_NUMBER_HATS) ||     // Address is invalid
        !_mcc134_lib_initialized ||         // Library is not initialized
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
  Sets an mcc134FactoryData to default values.
 *****************************************************************************/
static void _set_defaults(struct mcc134FactoryData* data)
{
    int i;

    if (data)
    {
        strcpy(data->serial, "00000000");
        strcpy(data->cal_date, "1970-01-01");
        for (i = 0; i < NUM_TC_CHANNELS; i++)
        {
            data->slopes[i] = 1.0;
            data->offsets[i] = 0.0;
        }
    }
}

/******************************************************************************
  Parse the factory data JSON structure. Does not recurse so we can support
  multiple processes.

  Expects a JSON structure like:

    {
        "serial": "00000000",
        "calibration":
        {
            "date": "2017-09-19",
            "slopes":
            [
                1.000000,
                1.000000,
                1.000000,
                1.000000,
                1.000000,
                1.000000,
                1.000000,
                1.000000
            ],
            "offsets":
            [
                0.000000,
                0.000000,
                0.000000,
                0.000000,
                0.000000,
                0.000000,
                0.000000,
                0.000000
            ]
        }
    }

  If it finds all of these keys it will return 1, otherwise will return 0.
 *****************************************************************************/
static int _parse_factory_data(cJSON* root, struct mcc134FactoryData* data)
{
    bool got_serial = false;
    bool got_date = false;
    bool got_slopes = false;
    bool got_offsets = false;
    cJSON* child;
    cJSON* calchild;
    cJSON* subchild;
    int index;

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
            strncpy(data->serial, child->valuestring, SERIAL_SIZE);
            got_serial = true;
        }
        else if (!strcmp(child->string, "calibration") &&
                (child->type == cJSON_Object))
        {
            // Found the calibration object, must go down a level
            calchild = child->child;

            while (calchild)
            {
                if (!strcmp(calchild->string, "date") &&
                    (calchild->type == cJSON_String) &&
                    calchild->valuestring)
                {
                    // Found the calibration date
                    strncpy(data->cal_date, calchild->valuestring,
                        CAL_DATE_SIZE-1);
                    got_date = true;
                }
                else if (!strcmp(calchild->string, "slopes") &&
                        (calchild->type == cJSON_Array))
                {
                    // Found the slopes array, must go down a level
                    subchild = calchild->child;
                    index = 0;

                    while (subchild)
                    {
                        // Iterate through the slopes array
                        if ((subchild->type == cJSON_Number) &&
                            (index < NUM_TC_CHANNELS))
                        {
                            data->slopes[index] = subchild->valuedouble;
                            index++;
                        }
                        subchild = subchild->next;
                    }

                    if (index == NUM_TC_CHANNELS)
                    {
                        // Must have all channels to be successful
                        got_slopes = true;
                    }
                }
                else if (!strcmp(calchild->string, "offsets") &&
                        (calchild->type == cJSON_Array))
                {
                    // Found the offsets array, must go down a level
                    subchild = calchild->child;
                    index = 0;

                    while (subchild)
                    {
                        // Iterate through the offsets array
                        if ((subchild->type == cJSON_Number) &&
                            (index < NUM_TC_CHANNELS))
                        {
                            data->offsets[index] = subchild->valuedouble;
                            index++;
                        }
                        subchild = subchild->next;
                    }

                    if (index == NUM_TC_CHANNELS)
                    {
                        // Must have all channels to be successful
                        got_offsets = true;
                    }
                }

                calchild = calchild->next;
            }
        }
        child = child->next;
    }

    if (got_serial && got_date && got_slopes && got_offsets)
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
static void _mcc134_lib_init(void)
{
    int i;

    if (!_mcc134_lib_initialized)
    {
        for (i = 0; i < MAX_NUMBER_HATS; i++)
        {
            _devices[i] = NULL;
        }

        _mcc134_lib_initialized = true;
    }
}

/******************************************************************************
 Convert thermistor resistance to temperature.
 *****************************************************************************/
static double _thermistor_temp(double r)
{
    double temp;
    double lr;

    // Steinhart-Hart equation
    lr = log(r);
    temp = STEINHART_A + (STEINHART_B * lr) +
           (STEINHART_C * lr * lr * lr);

    return (1.0 / temp) - 273.15;
}

/******************************************************************************
  Calculate TC temperature.
 *****************************************************************************/
double _calc_tc_temp(uint8_t tc_type, double tc_voltage, double cjc_temp)
{
    double val;
    double cjc_voltage;

    // calculate the temperature value
    val = tc_voltage * 1000.0;      // convert to mV

    // calculate the CJC offset voltage for the specific TC type
    cjc_voltage = NISTCalcVoltage(tc_type, cjc_temp);
    // add the CJC voltage to the thermocouple voltage
    val += cjc_voltage;
    // calculate the temperature
    val = NISTCalcTemperature(tc_type, val);

    return val;
}

/******************************************************************************
  Constantly read the CJC sensors and enabled TC inputs in the background.
 *****************************************************************************/
static void* _mcc134_thread(void* arg)
{
    struct mcc134Device* dev;
    uint32_t code;
    int tc_index;
    int cjc_index;
    int result;
    int state;
    int channel_count;
    int index;
    uint32_t sleep_time;
    uint32_t sleep_count;
    bool stop;
    uint32_t cjc_codes[NUM_CJC_SENSORS];
    uint32_t tc_codes[NUM_TC_CHANNELS];
    uint32_t update_interval;
    uint32_t cjc_average_buffer[NUM_CJC_SENSORS][CJC_AVERAGE_COUNT];
    uint8_t cjc_average_index;
    uint8_t cjc_average_count;
    double val;
#ifdef DEBUG_CJC    
    char filename[3][256];
    char filename_all[256];
    FILE* f[3];
    FILE* f_all;
#endif    

    if (arg == NULL)
    {
        return NULL;
    }
    dev = (struct mcc134Device*)arg;

#ifdef DEBUG_CJC
    sprintf(filename_all, "cjc_%d.csv", dev->address);
    f_all = fopen(filename_all, "w");
    fprintf(f_all, "CJC 0, CJC 1, CJC 2\n");
    
    for (index = 0; index < 3; index++)
    {
        sprintf(filename[index], "cjc_%d_%d.csv", dev->address, index);
        f[index] = fopen(filename[index], "w");
        fprintf(f[index], "CJC %d\n", index);
    }
#endif

    state = 0;
    tc_index = 0;
    cjc_index = 0;
    update_interval = 0;
    sleep_count = 0;
    sleep_time = 0;
    channel_count = 0;
    cjc_average_index = 0;
    cjc_average_count = 1;
    
    do
    {
        // check for events
        pthread_mutex_lock(&dev->tc_mutex);
        if (dev->tc_reset)
        {
            state = 0;
            tc_index = 0;
            cjc_index = 0;
            dev->tc_valid = false;
            
            channel_count = 0;
            for (index = 0; index < NUM_TC_CHANNELS; index++)
            {
                if (dev->tc_types[index] != TC_DISABLED)
                {
                    channel_count++;
                }
            }
            
            dev->tc_reset = false;
        }

        if (update_interval != dev->update_interval)
        {
            // the interval changed, calculate sleep time in 10ms increments
            update_interval = dev->update_interval;
            sleep_time = (update_interval * 1000 - 
                (channel_count * TC_CONVERSION_TIME_MS +
                NUM_CJC_SENSORS * CJC_CONVERSION_TIME_MS +
                (NUM_CJC_SENSORS - 1) * CJC_INTER_TIME_MS));            
            if (channel_count > 0)
            {
                sleep_time -= (channel_count - 1) * TC_INTER_TIME_MS;
            }
            sleep_time /= 10;

            // reset CJC filter
            cjc_average_index = 0;
            cjc_average_count = 1;
        }
        pthread_mutex_unlock(&dev->tc_mutex);
        
        // process state machine
        switch (state)
        {
        case 0: // perform a CJC reading
            result = _mcc134_adc_read_cjc_code(dev->address,
                CJC_CHAN_HI[cjc_index],
                CJC_CHAN_LO[cjc_index], &code);
            pthread_mutex_lock(&dev->tc_mutex);
            dev->cjc_result = result;
            pthread_mutex_unlock(&dev->tc_mutex);
            
            if (result == RESULT_SUCCESS)
            {
#ifdef DEBUG_CJC            
                fprintf(f[cjc_index], "%u\n", code);
#endif                
                if (update_interval <= 5)
                {
                    // filter the CJCs
                    cjc_average_buffer[cjc_index][cjc_average_index] = code;
                    
                    val = 0.0;
                    for (index = 0; index < cjc_average_count; index++)
                    {
                        val += (double)cjc_average_buffer[cjc_index][index];
                    }
                    val /= cjc_average_count;
                    
                    cjc_codes[cjc_index] = (uint32_t)(val + 0.5);
                }
                else
                {
                    // use unfiltered CJC values
                    cjc_codes[cjc_index] = code;
                }
                state++;
            }
#ifdef DEBUG_CJC            
            else
            {
                printf("%d cjc %d\n", dev->address, result);
            }
#endif
            break;
        case 1: // update variables and loop until CJCs are read
            cjc_index++;
            if (cjc_index >= NUM_CJC_SENSORS)
            {
#ifdef DEBUG_CJC            
                fprintf(f_all, "%u, %u, %u\n",
                    cjc_average_buffer[0][cjc_average_index],
                    cjc_average_buffer[1][cjc_average_index],
                    cjc_average_buffer[2][cjc_average_index]);
#endif                    
                cjc_index = 0;
                
                cjc_average_index++;
                if (cjc_average_index >= CJC_AVERAGE_COUNT)
                {
                    cjc_average_index = 0;
                }
                if (cjc_average_count < CJC_AVERAGE_COUNT)
                {
                    cjc_average_count++;
                }

                state++;
            }
            else
            {
                // read next sensor
                state = 0;
                usleep(CJC_INTER_TIME_MS*1000);
            }
            break;
        case 2: // read enabled TC channels
            if (dev->tc_types[tc_index] != TC_DISABLED)
            {
                // perform TC read
                result = _mcc134_adc_read_tc_code(dev->address,
                    TC_CHAN_HI[tc_index], TC_CHAN_LO[tc_index], &code);
                
                pthread_mutex_lock(&dev->tc_mutex);
                dev->tc_result = result;
                pthread_mutex_unlock(&dev->tc_mutex);
                if (result == RESULT_SUCCESS)
                {
                    tc_codes[tc_index] = code;
                    state++;
                }
            }
            else
            {
                // skip this channel
                state++;
            }
            break;
        case 3: // update variables and loop until TC channels are read
            tc_index++;
            if (tc_index >= NUM_TC_CHANNELS)
            {
                // all channels have been handled
                tc_index = 0;
                
                pthread_mutex_lock(&dev->tc_mutex);

                for (index = 0; index < NUM_CJC_SENSORS; index++)
                {
                    dev->cjc_codes[index] = cjc_codes[index];
                }
                for (index = 0; index < NUM_TC_CHANNELS; index++)
                {
                    dev->tc_codes[index] = tc_codes[index];
                }
                
                if (!dev->tc_reset)
                {
                    dev->cjc_valid = true;
                    dev->tc_valid = true;
                }
                pthread_mutex_unlock(&dev->tc_mutex);
                
                // prepare to sleep until next interval
                sleep_count = 0;
                state++;
            }
            else
            {
                // read next channel
                state = 2;
                usleep(TC_INTER_TIME_MS*1000);
            }
            break;
        case 4: // sleep until next reading
            if (sleep_count < sleep_time)
            {
                sleep_count++;
                usleep(10000);
            }
            else
            {
                state = 0;
            }
            break;
        default:
            state = 0;
            break;
        }
        
        pthread_mutex_lock(&dev->tc_mutex);
        stop = dev->stop_tc_thread;
        pthread_mutex_unlock(&dev->tc_mutex);
    } while (!stop);

#ifdef DEBUG_CJC            
    for (index = 0; index < 3; index++)
    {
        fclose(f[index]);
    }
    fclose(f_all);
#endif
    
    return NULL;
}

//*****************************************************************************
// Global Functions

/******************************************************************************
  Open a connection to the MCC 134 device at the specified address.
 *****************************************************************************/
int mcc134_open(uint8_t address)
{
    struct HatInfo info;
    char* custom_data;
    uint16_t custom_size;
    struct mcc134Device* dev;
    int result;
    int i;

    _mcc134_lib_init();

    // validate the parameters
    if ((address >= MAX_NUMBER_HATS))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address] == NULL)
    {
        // this is either the first time this device is being opened or it is
        // not a 134

        // read the EEPROM file(s), verify that it is an MCC 134, and get the
        // cal data
        if (_hat_info(address, &info, NULL, &custom_size) == RESULT_SUCCESS)
        {
            if (info.id == HAT_ID_MCC_134)
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
            custom_data = NULL;
        }

        // create a struct to hold device instance data
        _devices[address] = (struct mcc134Device*)calloc(1,
            sizeof(struct mcc134Device));
        dev = _devices[address];

        // initialize the struct elements
        dev->address = address;
        dev->handle_count = 1;
        for (i = 0; i < NUM_TC_CHANNELS; i++)
        {
            dev->tc_types[i] = TC_DISABLED;
            dev->tc_codes[i] = 0;
        }

        if (custom_size > 0)
        {
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

        // initialize the ADC
        if ((result = _mcc134_adc_init(address)) != RESULT_SUCCESS)
        {
            mcc134_close(address);
            return result;
        }

        // set up the TC / CJC thread
        dev->update_interval = 1;
        
        pthread_mutex_init(&dev->tc_mutex, NULL);

        dev->tc_handle = 0;
        dev->tc_valid = false;
        dev->tc_result = RESULT_SUCCESS;
        dev->stop_tc_thread = false;

        dev->cjc_valid = false;
        dev->cjc_result = RESULT_SUCCESS;

        pthread_attr_t attr;
        if ((result = pthread_attr_init(&attr)) != 0)
        {
            dev->tc_result = RESULT_RESOURCE_UNAVAIL;
            dev->cjc_result = dev->tc_result;
            return dev->tc_result;
        }

        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        if ((result = pthread_create(&dev->tc_handle, &attr,
            &_mcc134_thread, dev)) != 0)
        {
            pthread_attr_destroy(&attr);
            dev->tc_result = RESULT_RESOURCE_UNAVAIL;
            return dev->tc_result;
        }

        pthread_attr_destroy(&attr);
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
  Check if an MCC 134 is open.
 *****************************************************************************/
int mcc134_is_open(uint8_t address)
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
  Close a connection to an MCC 134 device and free allocated resources.
 *****************************************************************************/
int mcc134_close(uint8_t address)
{
    struct mcc134Device* dev;

    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];

    if (dev->handle_count == 0)
    {
        return RESULT_BAD_PARAMETER;
    }

    dev->handle_count--;
    if (dev->handle_count == 0)
    {
        // stop the TC and CJC thread
        dev->stop_tc_thread = true;
        if (dev->tc_handle)
        {
            pthread_join(dev->tc_handle, NULL);
            dev->tc_handle = 0;
        }

        free(_devices[address]);
        _devices[address] = NULL;
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Return the device info struct.
 *****************************************************************************/
struct MCC134DeviceInfo* mcc134_info(void)
{
    return &mcc134_device_info;
}


/******************************************************************************
  Read the serial number.
 *****************************************************************************/
int mcc134_serial(uint8_t address, char* buffer)
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
  Read the calibration date.
 *****************************************************************************/
int mcc134_calibration_date(uint8_t address, char* buffer)
{
    // validate parameters
    if (!_check_addr(address) ||
        (buffer == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    strcpy(buffer, _devices[address]->factory_data.cal_date);
    return RESULT_SUCCESS;
}

/******************************************************************************
  Read the calibration coefficients.
 *****************************************************************************/
int mcc134_calibration_coefficient_read(uint8_t address, uint8_t channel,
    double* slope, double* offset)
{
    // validate parameters
    if (!_check_addr(address) ||
        (channel >= NUM_TC_CHANNELS) ||
        (slope == NULL) ||
        (offset == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    *slope = _devices[address]->factory_data.slopes[channel];
    *offset = _devices[address]->factory_data.offsets[channel];
    return RESULT_SUCCESS;
}

/******************************************************************************
  Write the calibration coefficients.
 *****************************************************************************/
int mcc134_calibration_coefficient_write(uint8_t address, uint8_t channel,
    double slope, double offset)
{
    // validate parameters
    if (!_check_addr(address) ||
        (channel >= NUM_TC_CHANNELS))
    {
        return RESULT_BAD_PARAMETER;
    }

    _devices[address]->factory_data.slopes[channel] = slope;
    _devices[address]->factory_data.offsets[channel] = offset;
    return RESULT_SUCCESS;
}


/******************************************************************************
  Set the TC type for a channel.
 *****************************************************************************/
int mcc134_tc_type_write(uint8_t address, uint8_t channel, uint8_t type)
{
    struct mcc134Device* dev;

    if (!_check_addr(address) ||
        (channel >= NUM_TC_CHANNELS) ||
        ((type > TC_TYPE_N) && (type != TC_DISABLED)))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];
    
    // did we change enabled channels?
    if (((dev->tc_types[channel] == TC_DISABLED) && (type != TC_DISABLED)) ||
        ((dev->tc_types[channel] != TC_DISABLED) && (type == TC_DISABLED)))
    {
        pthread_mutex_lock(&dev->tc_mutex);
        _devices[address]->tc_types[channel] = type;
        dev->tc_reset = true;
        dev->tc_valid = false;
        pthread_mutex_unlock(&dev->tc_mutex);
    }
    else
    {
        _devices[address]->tc_types[channel] = type;
    }

    
    return RESULT_SUCCESS;
}

/******************************************************************************
  Read the TC type for a channel.
 *****************************************************************************/
int mcc134_tc_type_read(uint8_t address, uint8_t channel, uint8_t* type)
{
    if (!_check_addr(address) ||
        (channel >= NUM_TC_CHANNELS) ||
        (!type))
    {
        return RESULT_BAD_PARAMETER;
    }

    *type = _devices[address]->tc_types[channel];
    return RESULT_SUCCESS;
}

/******************************************************************************
  Write the thread update interval
 *****************************************************************************/
int mcc134_update_interval_write(uint8_t address, uint8_t interval)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (interval == 0)
    {
        interval = 1;
    }
    
    pthread_mutex_lock(&_devices[address]->tc_mutex);
    _devices[address]->update_interval = interval;
    pthread_mutex_unlock(&_devices[address]->tc_mutex);
    
    return RESULT_SUCCESS;
}

/******************************************************************************
  Read the thread update interval
 *****************************************************************************/
int mcc134_update_interval_read(uint8_t address, uint8_t* interval)
{
    if (!_check_addr(address) ||
        (!interval))
    {
        return RESULT_BAD_PARAMETER;
    }

    *interval = _devices[address]->update_interval;
    return RESULT_SUCCESS;
}

/******************************************************************************
  Read an analog input channel.
 *****************************************************************************/
int mcc134_a_in_read(uint8_t address, uint8_t channel, uint32_t options,
    double* value)
{
    int result;
    bool valid;
    uint32_t ucode;
    int32_t code;
    double val;
    struct mcc134Device* dev;

    if (!_check_addr(address) ||
        (channel > NUM_TC_CHANNELS) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];

    if (dev->tc_types[channel] == TC_DISABLED)
    {
        return RESULT_BAD_PARAMETER;
    }

    // wait for reading to be ready or an error occurs
    do
    {
        pthread_mutex_lock(&dev->tc_mutex);
        result = dev->tc_result;
        valid = dev->tc_valid;
        ucode = dev->tc_codes[channel];
        pthread_mutex_unlock(&dev->tc_mutex);

        if (valid)
        {
            if (result == RESULT_SUCCESS)
            {
                // have a value, process the data and options
                if ((ucode & OPEN_TC_MASK) == OPEN_TC_CODE)
                {
                    // don't perform all the calcs if the ADC is railed
                    if ((options & OPTS_NOSCALEDATA) != 0)
                    {
                        val = OPEN_TC_CODE;
                    }
                    else
                    {
                        val = OPEN_TC_VOLTAGE;
                    }
                }
                // check for common-mode error
                else if (((ucode & COMMON_MODE_MASK) != 0) &&
                         (options & OPTS_NOSCALEDATA) == 0)
                {
                    val = COMMON_MODE_TC_VALUE;
                }
                else
                {
                    // convert to signed code
                    if (ucode & 0x00800000)
                    {
                        code = (int32_t)(0xFF000000 | (ucode & 0x00FFFFFF));
                    }
                    else
                    {
                        code = (int32_t)(ucode & 0x00FFFFFF);
                    }

                    // calibrate?
                    if (options & OPTS_NOCALIBRATEDATA)
                    {
                        val = (double)code;
                    }
                    else
                    {
                        val = ((double)code *
                            dev->factory_data.slopes[channel]) +
                            dev->factory_data.offsets[channel];
                    }

                    // calculate voltage?
                    if ((options & OPTS_NOSCALEDATA) == 0)
                    {
                        val = val * ((REFERENCE_VOLTS / PGA_GAIN) /
                            ((double)(MAX_CODE + 1)));
                    }
                }

                *value = val;
            }
        }
        else
        {
            // wait for first valid reading
            usleep(1000);
        }
    } while (!valid && (result == RESULT_SUCCESS));

    return result;
}

/******************************************************************************
  Read a CJC temperature.
 *****************************************************************************/
int mcc134_cjc_read(uint8_t address, uint8_t channel, double* value)
{
    int result;
    bool valid;
#ifdef INTERPOLATE_CJC
    uint32_t codes[NUM_CJC_SENSORS];
    double int_code;
#else
    uint32_t code;
    int cjc_chan;
#endif                
    double r;
    double temp;
    struct mcc134Device* dev;

    if (!_check_addr(address) ||
        (channel >= NUM_TC_CHANNELS*2) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];
    
#ifndef INTERPOLATE_CJC    
    if (channel < NUM_TC_CHANNELS)
    {
        cjc_chan = CJC_MAP[channel];
    }
    else
    {
        cjc_chan = CJC_MAP[channel - NUM_TC_CHANNELS];
    }
#endif

    // wait for reading to be ready or an error occurs
    do
    {
        pthread_mutex_lock(&dev->tc_mutex);
        result = dev->cjc_result;
        valid = dev->cjc_valid;
#ifdef INTERPOLATE_CJC
        codes[0] = dev->cjc_codes[0];
        codes[1] = dev->cjc_codes[1];
        codes[2] = dev->cjc_codes[2];
#else
        code = dev->cjc_codes[cjc_chan];
#endif
        pthread_mutex_unlock(&dev->tc_mutex);

        if (valid)
        {
            if (result == RESULT_SUCCESS)
            {
#ifdef INTERPOLATE_CJC
                switch (channel)
                {
                case 0:
                    int_code = (double)codes[0] + 
                        ((double)codes[1] - (double)codes[0])/3.0;
                    break;
                case 1:
                    int_code = (double)codes[1] + 
                        ((double)codes[0] - (double)codes[1])/3.0;
                    break;
                case 2:
                    int_code = (double)codes[1] + 
                        ((double)codes[2] - (double)codes[1])/3.0;
                    break;
                case 3:
                    int_code = (double)codes[2] + 
                        ((double)codes[1] - (double)codes[1])/3.0;
                    break;
                default:
                    int_code = 0;
                    break;
                }
                r = int_code * CJC_REF_R / ((double)CJC_MAX_CODE - int_code);
                temp = _thermistor_temp(r);
                *value = temp;
#else
                // have a value
                r = (double)code * CJC_REF_R / (double)(CJC_MAX_CODE - code);

                if (channel < NUM_TC_CHANNELS)
                {
                    temp = _thermistor_temp(r);
                    *value = temp;
                }
                else
                {
                    // When the channel is 4 - 7, return the value as resistance
                    // (used for testing.)
                    *value = r;
                }
#endif                
            }
        }
        else
        {
            // wait for first valid reading
            usleep(1000);
        }
    } while (!valid && (result == RESULT_SUCCESS));

    return result;
}

/******************************************************************************
  Read a temperature input channel.
 *****************************************************************************/
int mcc134_t_in_read(uint8_t address, uint8_t channel, double* value)
{
    int result;
    struct mcc134Device* dev;
    double cjc_temp;
    double tc_value;

    if (!_check_addr(address) ||
        (channel > NUM_TC_CHANNELS) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];

    if (dev->tc_types[channel] == TC_DISABLED)
    {
        return RESULT_BAD_PARAMETER;
    }

    result = mcc134_a_in_read(dev->address, channel, OPTS_DEFAULT, &tc_value);
    if (result != RESULT_SUCCESS)
    {
        return result;
    }
    
    if (tc_value == OPEN_TC_VOLTAGE)
    {
        // don't perform all the calcs if the ADC is railed
        tc_value = OPEN_TC_VALUE;
    }
    else if (tc_value == COMMON_MODE_TC_VALUE)
    {
        // report a common mode error
    }
    else
    {
        // We have a valid reading, read the CJC and calculate the temperature.
        result = mcc134_cjc_read(dev->address, channel, &cjc_temp);
        if (result != RESULT_SUCCESS)
        {
            return result;
        }

        tc_value = _calc_tc_temp(dev->tc_types[channel], tc_value, cjc_temp);
    }

    *value = tc_value;
    return result;
}
