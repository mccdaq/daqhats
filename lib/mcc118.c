/*
*   mcc118.c
*   Measurement Computing Corp.
*   This file contains functions used with the MCC 118.
*
*   1 Feb 2018
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "daqhats.h"
#include "util.h"
#include "cJSON.h"

// *****************************************************************************
// Constants

#define MAX_CODE                (4095)
#define RANGE_MIN               (-10.0)
#define RANGE_MAX               (+10.0)
#define LSB_SIZE                ((RANGE_MAX - RANGE_MIN)/(MAX_CODE+1))
#define VOLTAGE_MIN             RANGE_MIN
#define VOLTAGE_MAX             (RANGE_MAX - LSB_SIZE)

struct MCC118DeviceInfo mcc118_device_info =
{
    // The number of analog input channels.
    8,
    // The minimum uncalibrated ADC code.
    0,
    // The maximum uncalibrated ADC code (4095.)
    MAX_CODE,
    // The input voltage corresponding to the minimum code (-10.0V.)
    VOLTAGE_MIN,
    // The input voltage corresponding to the maximum code (+10.0V - 1 LSB.)
    VOLTAGE_MAX,
    // The minimum voltage of the input range (-10.0V.)
    RANGE_MIN,
    // The maximum voltage of the input range (+10.0V.)
    RANGE_MAX
};

#define CLOCK_TIMEBASE          16e6
#define MAX_ADC_RATE            100000.0
#define MIN_SCAN_RATE           (CLOCK_TIMEBASE / 0xFFFFFFFF)

// Delay / timeout constants
#define SEND_RETRY_TIME         10*MSEC     // 10 milliseconds

// MCC 118 command codes
#define CMD_AIN                 0x10
#define CMD_AINSCANSTART        0x11
#define CMD_AINSCANSTATUS       0x12
#define CMD_AINSCANDATA         0x13
#define CMD_AINSCANSTOP         0x14
#define CMD_AINSCANSTATUSDATA   0x15

#define CMD_BLINK               0x40
#define CMD_ID                  0x41
#define CMD_RESET               0x42
#define CMD_TESTCLOCK           0x43
#define CMD_TESTTRIGGER         0x44

#define CMD_BOOTMEM_READ        0x52
#define CMD_BOOTMEM_WRITE       0x53

#define CMD_BL_ENTER            0x54
#define CMD_BL_ERASE            0x55
#define CMD_BL_WRITE            0x56
#define CMD_BL_READ_CRC         0x57
#define CMD_BL_JUMP             0x58

#define CMD_READ_REPLY          0x7F

#define MAX_TX_DATA_SIZE        (256)    // size of transmit / receive SPI
                                         // buffer in device

#define MSG_START               (0xDB)

// Tx definitions
#define MSG_TX_INDEX_START      0
#define MSG_TX_INDEX_COMMAND    1
#define MSG_TX_INDEX_COUNT_LOW  2
#define MSG_TX_INDEX_COUNT_HIGH 3
#define MSG_TX_INDEX_DATA       4

#define MSG_TX_HEADER_SIZE      4

// Rx definitions
#define MSG_RX_INDEX_START      0
#define MSG_RX_INDEX_COMMAND    1
#define MSG_RX_INDEX_STATUS     2
#define MSG_RX_INDEX_COUNT_LOW  3
#define MSG_RX_INDEX_COUNT_HIGH 4
#define MSG_RX_INDEX_DATA       5

#define MSG_RX_HEADER_SIZE      5

#define TX_BUFFER_SIZE          (MAX_TX_DATA_SIZE + MSG_TX_HEADER_SIZE)

#define MAX_SAMPLES_READ        512

// MCC 118 command response codes
#define FW_RES_SUCCESS          0x00
#define FW_RES_BAD_PROTOCOL     0x01
#define FW_RES_BAD_PARAMETER    0x02
#define FW_RES_BUSY             0x03
#define FW_RES_NOT_READY        0x04
#define FW_RES_TIMEOUT          0x05
#define FW_RES_OTHER_ERROR      0x06


#define SERIAL_SIZE     (8+1)   ///< The maximum size of the serial number
                                // string, plus NULL.
#define CAL_DATE_SIZE   (10+1)  ///< The maximum size of the calibration date
                                // string, plus NULL.
#define NUM_CHANNELS    8       ///< The number of analog input channels.

#define MAX_SCAN_BUFFER_SIZE_SAMPLES    (16ul*1024ul*1024ul)    // 16 MS

#define COUNT_NORMALIZE(x, c)  ((x / c) * c)

#define MIN(a, b)   ((a < b) ? a : b)
#define MAX(a, b)   ((a > b) ? a : b)

/// \cond
// Contains the device-specific data stored at the factory.
struct mcc118FactoryData
{
    // Serial number
    char serial[SERIAL_SIZE];
    // Calibration date in the format 2017-09-19
    char cal_date[CAL_DATE_SIZE];
    // Calibration coefficients - per channel slopes
    double slopes[NUM_CHANNELS];
    // Calibration coefficents - per channel offsets
    double offsets[NUM_CHANNELS];
};

// Local data for analog input scans
struct mcc118ScanThreadInfo
{
    pthread_t handle;
    double* scan_buffer;
    uint32_t buffer_size;
    uint32_t write_index;
    uint32_t read_index;
    uint32_t samples_transferred;
    uint32_t buffer_depth;

    uint16_t read_threshold;
    uint16_t options;
    bool hw_overrun;
    bool buffer_overrun;
    bool thread_started;
    bool thread_running;
    bool stop_thread;
    bool triggered;
    bool scan_running;
    uint8_t channel_count;
    uint8_t channel_index;
    uint8_t channels[NUM_CHANNELS];
    double slopes[NUM_CHANNELS];
    double offsets[NUM_CHANNELS];
};

// Local data for each open MCC 118 board.
struct mcc118Device
{
    uint16_t handle_count;      // the number of handles open to this device
    uint8_t mode;               // firmware mode; 0=normal, 1=bootloader
    uint16_t fw_version;        // firmware version
    uint16_t boot_version;      // bootloader version
    int spi_fd;                 // SPI file descriptor
    uint8_t trigger_mode;       // Trigger mode
    struct mcc118FactoryData factory_data;   // Factory data
    struct mcc118ScanThreadInfo* scan_info; // Scan info
    pthread_mutex_t scan_mutex;
};

/// \endcond

// *****************************************************************************
// Variables

static struct mcc118Device* _devices[MAX_NUMBER_HATS];
static bool _mcc118_lib_initialized = false;

static const char* const spi_device = SPI_DEVICE_0; // the spidev device
static const uint8_t spi_mode = SPI_MODE_1;         // use mode 1 (CPOL=0,
                                                    // CPHA=1)
static const uint8_t spi_bits = 8;                  // 8 bits per transfer
static const uint32_t spi_speed = 9600000;          // maximum SPI clock
                                                    // frequency
static const uint16_t spi_delay = 0;                // delay in us before
                                                    // removing CS

// *****************************************************************************
// Local Functions

/******************************************************************************
  Validate parameters for an address
 *****************************************************************************/
static bool _check_addr(uint8_t address)
{
    if ((address >= MAX_NUMBER_HATS) ||     // Address is invalid
        !_mcc118_lib_initialized ||         // Library is not initialized
        (_devices[address] == NULL) ||      // Device structure is not allocated
        (_devices[address]->spi_fd < 0))    // SPI file descriptor is invalid
    {
        return false;
    }
    else
    {
        return true;
    }
}

/******************************************************************************
  Parse a buffer and look for a valid message
 *****************************************************************************/
static bool _parse_buffer(uint8_t* buffer, uint16_t length,
    uint16_t* frame_start, uint16_t* frame_length, uint16_t* remaining)
{
    uint8_t* ptr = buffer;
    uint16_t index;
    bool found_frame;
    int parse_state = 0;
    uint16_t data_count;
    uint16_t data_index;
    uint16_t _remaining;
    uint16_t _frame_length;
    uint16_t _frame_start;

    found_frame = false;
    _remaining = 0;
    _frame_length = 0;
    _frame_start = 0;
    data_index = 0;
    data_count = 0;

    for (index = 0; (index < length) && !found_frame; index++)
    {
        switch (parse_state)
        {
        case 0: // looking for start
            if (MSG_START == ptr[index])
            {
                _frame_start = index;
                data_count = 0;
                data_index = 0;
                parse_state++;
            }
            break;
        case 1: // command
            parse_state++;
            break;
        case 2: // status
            parse_state++;
            break;
        case 3: // count low
            data_count = ptr[index];
            parse_state++;
            break;
        case 4: // count high
            data_count |= (uint16_t)ptr[index] << 8;
            if (data_count == 0)
            {
                _remaining = 0;
                found_frame = true;
                _frame_length = MSG_RX_HEADER_SIZE;
                parse_state = 6;
            }
            else
            {
                _remaining = data_count;// + 1;
                parse_state++;
            }
            break;
        case 5: // data
            _remaining--;
            if (++data_index >= data_count)
            {
                parse_state++;
                found_frame = true;
                _frame_length = data_count + MSG_RX_HEADER_SIZE;
            }
            break;
        case 6: // message is complete
            break;
        default:
            parse_state = 0;
            break;
        }
    }

    *remaining = _remaining;
    *frame_length = _frame_length;
    *frame_start = _frame_start;
    return found_frame;
}

/******************************************************************************
  Create a message frame for sending to the device
 *****************************************************************************/
static int _create_frame(uint8_t* buffer, uint8_t command, uint16_t count,
    void* data)
{
    if (count > MAX_TX_DATA_SIZE)
    {
        return 0;
    }

    buffer[MSG_TX_INDEX_START] = MSG_START;
    buffer[MSG_TX_INDEX_COMMAND] = command;
    buffer[MSG_TX_INDEX_COUNT_LOW] = (uint8_t)count;
    buffer[MSG_TX_INDEX_COUNT_HIGH] = (uint8_t)(count >> 8);

    if (count > 0)
    {
        memcpy(&buffer[MSG_TX_INDEX_DATA], data, count);
    }

    return MSG_TX_HEADER_SIZE + count;
}

/******************************************************************************
  Perform command / response SPI transfers to an MCC 118.

  address: board address
  command: firmware API command code
  tx_data: optional transmit data buffer
  tx_data_count: count of transmit data bytes
  rx_data: optional receive data buffer
  rx_data_count: count of receive data bytes
  reply_timeout_us: Time to wait for a reply in microseconds
  retry_us: delay between read retries in microseconds

  Return: RESULT_SUCCESS if successful
 *****************************************************************************/
static int _spi_transfer(uint8_t address, uint8_t command, void* tx_data,
    uint16_t tx_data_count, void* rx_data, uint16_t rx_data_count,
    uint32_t reply_timeout_us, uint32_t retry_us)
{
    struct timespec start_time;
    struct timespec current_time;
    uint32_t diff;
    bool got_reply;
    int lock_fd;
    int ret;
    uint8_t temp;
    bool timeout;

    uint16_t tx_count;
    uint8_t* tx_buffer;
    uint8_t* rx_buffer;
    uint8_t* temp_buffer;
    struct mcc118Device* dev = _devices[address];

    if (!_check_addr(address) ||                // check address failed
        (tx_data_count && (tx_data == NULL)) || // no tx buffer when count != 0
        (rx_data_count && (rx_data == NULL)))   // no rx buffer when count != 0
    {
        return RESULT_BAD_PARAMETER;
    }

    // allocate buffers
    uint16_t tx_buffer_size = MSG_TX_HEADER_SIZE + tx_data_count;
    tx_buffer = (uint8_t*)calloc(1, tx_buffer_size);
    uint16_t rx_buffer_size = MSG_RX_HEADER_SIZE + rx_data_count + 5;
    rx_buffer = (uint8_t*)calloc(1, rx_buffer_size);
    uint16_t temp_buffer_size = MAX(rx_buffer_size, tx_buffer_size);
    temp_buffer = (uint8_t*)calloc(1, temp_buffer_size);

    if ((tx_buffer == NULL) ||
        (rx_buffer == NULL) ||
        (temp_buffer == NULL))
    {
        free(tx_buffer);
        free(rx_buffer);
        free(temp_buffer);

        return RESULT_RESOURCE_UNAVAIL;
    }

    // create a tx frame
    tx_count = _create_frame(tx_buffer, command, tx_data_count, tx_data);

    // Obtain a spi lock
    if ((lock_fd = _obtain_lock()) < 0)
    {
        // could not get a lock within 5 seconds, report as a timeout
        free(tx_buffer);
        free(rx_buffer);
        free(temp_buffer);
        return RESULT_LOCK_TIMEOUT;
    }

    _set_address(address);

    // check spi mode and change if necessary
    ret = ioctl(dev->spi_fd, SPI_IOC_RD_MODE, &temp);
    if (ret == -1)
    {
        _free_address();
        _release_lock(lock_fd);
        free(tx_buffer);
        free(rx_buffer);
        free(temp_buffer);
        return RESULT_UNDEFINED;
    }
    if (temp != spi_mode)
    {
        ret = ioctl(dev->spi_fd, SPI_IOC_WR_MODE, &spi_mode);
        if (ret == -1)
        {
            _free_address();
            _release_lock(lock_fd);
            free(tx_buffer);
            free(rx_buffer);
            free(temp_buffer);
            return RESULT_UNDEFINED;
        }
    }

    // Init the spi ioctl structure, using temp_buffer for the intermediate
    // reply.
    struct spi_ioc_transfer tr = {
        .tx_buf = (uintptr_t)tx_buffer,
        .rx_buf = (uintptr_t)temp_buffer,
        .len = tx_count,
        .delay_usecs = spi_delay,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };

    // send the command
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    if ((ret = ioctl(dev->spi_fd, SPI_IOC_MESSAGE(1), &tr)) < 1)
    {
        _free_address();
        _release_lock(lock_fd);
        free(tx_buffer);
        free(rx_buffer);
        free(temp_buffer);
        return RESULT_UNDEFINED;
    }

    if (retry_us)
        usleep(retry_us);

    // read the reply
    memset(temp_buffer, 0xFF, rx_buffer_size);
    uint16_t frame_start = 0;
    uint16_t frame_length;
    uint16_t remaining = 0;
    uint16_t read_amount = rx_data_count + MSG_RX_HEADER_SIZE;

    // only read the first byte of the reply in order to test for the device
    // readiness
    struct spi_ioc_transfer tr1 = {
        .tx_buf = (uintptr_t)temp_buffer,
        .rx_buf = (uintptr_t)rx_buffer,
        .len = 1,
        .delay_usecs = spi_delay,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };
    got_reply = false;

    do
    {
        // loop until a reply is ready
        if ((ret = ioctl(dev->spi_fd, SPI_IOC_MESSAGE(1), &tr1)) >= 1)
        {
            if (rx_buffer[0] != 0)
            {
                got_reply = true;
            }
            else
            {
                if (retry_us)
                {
                    usleep(retry_us);
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        diff = _difftime_us(&start_time, &current_time);
        timeout = (diff > reply_timeout_us);
    } while (!got_reply && !timeout);

    if (got_reply)
    {
        // read the rest of the reply
        struct spi_ioc_transfer tr2 = {
            .tx_buf = (uintptr_t)temp_buffer,
            .rx_buf = (uintptr_t)&rx_buffer[1],
            .len = read_amount,
            .delay_usecs = spi_delay,
            .speed_hz = spi_speed,
            .bits_per_word = spi_bits,
        };

        got_reply = false;
        do
        {
            if ((ret = ioctl(dev->spi_fd, SPI_IOC_MESSAGE(1), &tr2)) >= 1)
            {
                // parse the reply
                got_reply = _parse_buffer(rx_buffer, read_amount+1,
                    &frame_start, &frame_length, &remaining);
            }
            else
            {
                printf("ioctl failed %d %d\n", errno, tr2.len);
                usleep(300);
            }

            clock_gettime(CLOCK_MONOTONIC, &current_time);
            diff = _difftime_us(&start_time, &current_time);
            timeout = (diff > reply_timeout_us);
        } while (!got_reply && !timeout);
    }

    if (!got_reply)
    {
        _free_address();
        // clear the SPI lock
        _release_lock(lock_fd);
        free(tx_buffer);
        free(rx_buffer);
        free(temp_buffer);
        return RESULT_TIMEOUT;
    }

    if (rx_buffer[frame_start+MSG_RX_INDEX_COMMAND] ==
        tx_buffer[MSG_TX_INDEX_COMMAND])
    {
        switch (rx_buffer[frame_start+MSG_RX_INDEX_STATUS])
        {
        case FW_RES_SUCCESS:
            if (rx_data_count > 0)
            {
                memcpy(rx_data, &rx_buffer[frame_start+MSG_RX_INDEX_DATA],
                    rx_data_count);
            }
            ret = RESULT_SUCCESS;
            break;
        case FW_RES_BAD_PARAMETER:
            ret = RESULT_BAD_PARAMETER;
            break;
        case FW_RES_TIMEOUT:
            ret = RESULT_TIMEOUT;
            break;
        case FW_RES_BUSY:
            ret = RESULT_BUSY;
            break;
        /*
        case FW_RES_BAD_PROTOCOL:
        case FW_RES_OTHER_ERROR:
        */
        default:
            ret = RESULT_UNDEFINED;
            break;
        }
    }
    else
    {
        ret = RESULT_BAD_PARAMETER;
    }

    _free_address();

    // clear the SPI lock
    _release_lock(lock_fd);

    free(tx_buffer);
    free(rx_buffer);
    free(temp_buffer);

    return ret;
}

/******************************************************************************
  Sets an mcc118FactoryData to default values.
 *****************************************************************************/
static void _set_defaults(struct mcc118FactoryData* data)
{
    int i;

    if (data)
    {
        strcpy(data->serial, "00000000");
        strcpy(data->cal_date, "1970-01-01");
        for (i = 0; i < NUM_CHANNELS; i++)
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
static int _parse_factory_data(cJSON* root, struct mcc118FactoryData* data)
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
            strncpy(data->serial, child->valuestring, SERIAL_SIZE-1);
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
                            (index < NUM_CHANNELS))
                        {
                            data->slopes[index] = subchild->valuedouble;
                            index++;
                        }
                        subchild = subchild->next;
                    }

                    if (index == NUM_CHANNELS)
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
                            (index < NUM_CHANNELS))
                        {
                            data->offsets[index] = subchild->valuedouble;
                            index++;
                        }
                        subchild = subchild->next;
                    }

                    if (index == NUM_CHANNELS)
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
static void _mcc118_lib_init(void)
{
    int i;

    if (!_mcc118_lib_initialized)
    {
        for (i = 0; i < MAX_NUMBER_HATS; i++)
        {
            _devices[i] = NULL;
        }

        _mcc118_lib_initialized = true;
    }
}

/******************************************************************************
  Read the specified number of samples of scan data as double precision.
 *****************************************************************************/
static int _a_in_read_scan_data(uint8_t address, uint16_t sample_count,
    bool scaled, bool calibrated, double* buffer)
{
    uint16_t count;
    int ret;
    struct mcc118Device* dev;
    uint16_t* rx_data;

    if (!_check_addr(address) ||
        (buffer == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];

    rx_data = (uint16_t*)calloc(1, sample_count * sizeof(uint16_t));
    if (rx_data == NULL)
    {
        return RESULT_RESOURCE_UNAVAIL;
    }

    // send the read scan data command
    ret = _spi_transfer(address, CMD_AINSCANDATA, &sample_count, 2, rx_data,
        sample_count*sizeof(uint16_t), 40*MSEC, 1);

    if (ret != RESULT_SUCCESS)
    {
        free(rx_data);
        return ret;
    }

    for (count = 0; count < sample_count; count++)
    {
        // convert raw values to double
        buffer[count] = (double)rx_data[count];

        if (calibrated)
        {
            // apply the appropriate cal factor to each sample in the list
            buffer[count] *= dev->scan_info->slopes[
                dev->scan_info->channel_index];
            buffer[count] += dev->scan_info->offsets[
                dev->scan_info->channel_index];
        }

        // convert to volts if desired
        if (scaled)
        {
            buffer[count] *= LSB_SIZE;
            buffer[count] += VOLTAGE_MIN;
        }

        dev->scan_info->channel_index++;
        if (dev->scan_info->channel_index >= dev->scan_info->channel_count)
        {
            dev->scan_info->channel_index = 0;
        }
    }

    free(rx_data);
    return RESULT_SUCCESS;
}

/******************************************************************************
 Reads the scan status and data until the scan ends.
 *****************************************************************************/
static void* _scan_thread(void* arg)
{
    bool done;
    uint16_t available_samples;
    uint16_t max_read_now;
    uint16_t read_count;
    int error;
    uint32_t sleep_us;
    uint32_t status_count;
    uint8_t address = *(uint8_t*)arg;
    struct mcc118ScanThreadInfo* info = _devices[address]->scan_info;
    bool calibrated;
    bool scaled;
    uint8_t rx_buffer[5];
    bool scan_running;
    bool stop_thread;

    free(arg);

    if (!_check_addr(address) ||
        (info == NULL))
    {
        return NULL;
    }

    pthread_mutex_lock(&_devices[address]->scan_mutex);
    info->thread_started = true;
    info->thread_running = true;
    info->hw_overrun = false;
    pthread_mutex_unlock(&_devices[address]->scan_mutex);

    status_count = 0;

    if (info->options & OPTS_NOSCALEDATA)
    {
        scaled = false;
    }
    else
    {
        scaled = true;
    }

    if (info->options & OPTS_NOCALIBRATEDATA)
    {
        calibrated = false;
    }
    else
    {
        calibrated = true;
    }

#define MIN_SLEEP_US	200
#define TRIG_SLEEP_US	1000

    done = false;
    sleep_us = MIN_SLEEP_US;
    do
    {
        // read the scan status
        if (_spi_transfer(address, CMD_AINSCANSTATUS, NULL, 0, rx_buffer, 5,
            1*MSEC, 20) == RESULT_SUCCESS)
        {
            available_samples = ((uint16_t)rx_buffer[2] << 8) + rx_buffer[1];
            max_read_now = ((uint16_t)rx_buffer[4] << 8) + rx_buffer[3];
            scan_running = (rx_buffer[0] & 0x01) == 0x01;

            pthread_mutex_lock(&_devices[address]->scan_mutex);
            info->hw_overrun = (rx_buffer[0] & 0x02) == 0x02;
            info->triggered = (rx_buffer[0] & 0x04) == 0x04;
            pthread_mutex_unlock(&_devices[address]->scan_mutex);

            status_count++;

            if (info->hw_overrun)
            {
                done = true;
                pthread_mutex_lock(&_devices[address]->scan_mutex);
                info->scan_running = false;
                pthread_mutex_unlock(&_devices[address]->scan_mutex);
            }
            else if (info->triggered == 0)
            {
                // waiting for trigger, use a longer sleep time
                sleep_us = TRIG_SLEEP_US;
            }
            else
            {
                // determine how much data to read
                if (!scan_running ||
                    (available_samples >= info->read_threshold) ||
                    (available_samples > max_read_now))
                {
                    read_count = available_samples;
                    if (max_read_now < read_count)
                    {
                        read_count = max_read_now;
                    }
                    if (read_count > MAX_SAMPLES_READ)
                    {
                        read_count = MAX_SAMPLES_READ;
                    }
                }
                else
                {
                    read_count = 0;
                }

                if (read_count > 0)
                {
                    // handle wrap at end of buffer
                    if ((info->buffer_size - info->write_index) < read_count)
                    {
                        read_count = (info->buffer_size - info->write_index);
                    }

                    if ((error = _a_in_read_scan_data(address, read_count,
                        scaled, calibrated,
                        &info->scan_buffer[info->write_index])) ==
                        RESULT_SUCCESS)
                    {
                        info->write_index += read_count;
                        if (info->write_index >= info->buffer_size)
                        {
                            info->write_index = 0;
                        }

                        pthread_mutex_lock(&_devices[address]->scan_mutex);
                        info->buffer_depth += read_count;
                        pthread_mutex_unlock(&_devices[address]->scan_mutex);

                        if (info->buffer_depth > info->buffer_size)
                        {
                            pthread_mutex_lock(&_devices[address]->scan_mutex);
                            info->buffer_overrun = true;
                            info->scan_running = false;
                            pthread_mutex_unlock(
                                &_devices[address]->scan_mutex);
                            done = true;
                        }
                        info->samples_transferred += read_count;
                    }

                    // adaptive sleep time to minimize processor usage
                    if (status_count > 4)
                    {
                        sleep_us *= 2;
                    }
                    else if (status_count < 1)
                    {
                        sleep_us /= 2;
                        if (sleep_us < MIN_SLEEP_US)
                        {
                            sleep_us = MIN_SLEEP_US;
                        }
                    }

                    status_count = 0;
                }

                if (!scan_running && (available_samples == read_count))
                {
                    done = true;
                    pthread_mutex_lock(&_devices[address]->scan_mutex);
                    info->scan_running = false;
                    pthread_mutex_unlock(&_devices[address]->scan_mutex);
                }
            }
        }

        usleep(sleep_us);

        pthread_mutex_lock(&_devices[address]->scan_mutex);
        stop_thread = info->stop_thread;
        pthread_mutex_unlock(&_devices[address]->scan_mutex);
    }
    while (!stop_thread && !done);

    if (info->scan_running)
    {
        // if we are stopped while the device is still running a scan then
        // send the stop scan command
        mcc118_a_in_scan_stop(address);
    }

    pthread_mutex_lock(&_devices[address]->scan_mutex);
    info->thread_running = false;
    pthread_mutex_unlock(&_devices[address]->scan_mutex);
    return NULL;
}


//*****************************************************************************
// Global Functions

/******************************************************************************
  Open a connection to the MCC 118 device at the specified address.
 *****************************************************************************/
int mcc118_open(uint8_t address)
{
    int ret;
    struct HatInfo info;
    char* custom_data;
    uint16_t custom_size;
    struct mcc118Device* dev;
    uint16_t id_data[3];

    _mcc118_lib_init();

    // validate the parameters
    if (address >= MAX_NUMBER_HATS)
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address] == NULL)
    {
        // this is either the first time this device is being opened or it is
        // not a 118

        // read the EEPROM file(s), verify that it is an MCC 118, and get the
        // cal data
        if (_hat_info(address, &info, NULL, &custom_size) == RESULT_SUCCESS)
        {
            if (info.id == HAT_ID_MCC_118)
            {
                custom_data = calloc(1, custom_size);
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
        _devices[address] = (struct mcc118Device*)calloc(
            1, sizeof(struct mcc118Device));
        dev = _devices[address];

        // initialize the struct elements
        dev->scan_info = NULL;
        dev->handle_count = 1;

        pthread_mutex_init(&dev->scan_mutex, NULL);

        // open the SPI device handle
        dev->spi_fd = open(spi_device, O_RDWR);
        if (dev->spi_fd < 0)
        {
            pthread_mutex_destroy(&dev->scan_mutex);
            free(custom_data);
            free(dev);
            _devices[address] = NULL;
            return RESULT_RESOURCE_UNAVAIL;
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

    }
    else
    {
        // the device has already been opened and initialized, increment
        // reference count
        dev = _devices[address];
        dev->handle_count++;
    }

    int attempts = 0;

    do
    {
        // Try to communicate with the device and verify that it is an MCC 118
        ret = _spi_transfer(address, CMD_ID, NULL, 0, id_data,
            3*sizeof(uint16_t), 20*MSEC, 10);

        if (ret == RESULT_SUCCESS)
        {
            // the ID command returns the product ID, compare it with the MCC
            // 118
            if ((id_data[0] == HAT_ID_MCC_118) ||
                (id_data[0] == HAT_ID_MCC_118_BOOTLOADER))
            {
                // save the firmware version
                dev->fw_version = id_data[1];
                dev->boot_version = id_data[2];
                dev->mode = (id_data[0] == HAT_ID_MCC_118_BOOTLOADER);
                return RESULT_SUCCESS;
            }
            else
            {
                pthread_mutex_destroy(&dev->scan_mutex);
                free(dev);
                _devices[address] = NULL;
                return RESULT_INVALID_DEVICE;
            }
        }

        attempts++;
    } while ((ret != RESULT_SUCCESS) && (attempts < 2));

    return RESULT_SUCCESS;
}

/******************************************************************************
  Check if an MCC 118 is open.
 *****************************************************************************/
int mcc118_is_open(uint8_t address)
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
  Close a connection to an MCC 118 device and free allocated resources.
 *****************************************************************************/
int mcc118_close(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    mcc118_a_in_scan_cleanup(address);

    _devices[address]->handle_count--;
    if (_devices[address]->handle_count == 0)
    {
        pthread_mutex_destroy(&_devices[address]->scan_mutex);
        close(_devices[address]->spi_fd);
        free(_devices[address]);
        _devices[address] = NULL;
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Blink the board LED.
 *****************************************************************************/
int mcc118_blink_led(uint8_t address, uint8_t count)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    int ret = _spi_transfer(address, CMD_BLINK, &count, 1, NULL, 0, 20*MSEC,
        10);
    return ret;
}


/******************************************************************************
  Return the board firmware and bootloader versions
 *****************************************************************************/
int mcc118_firmware_version(uint8_t address, uint16_t* version,
    uint16_t* boot_version)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (version)
        *version = _devices[address]->fw_version;
    if (boot_version)
        *boot_version = _devices[address]->boot_version;

    return RESULT_SUCCESS;
}

/******************************************************************************
  Send a reset command to the HAT board micro.
 *****************************************************************************/
int mcc118_reset(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send reset command
    int ret = _spi_transfer(address, CMD_RESET, NULL, 0, NULL, 0, 20*MSEC, 10);
    return ret;
}

/******************************************************************************
  Return the device info struct.
 *****************************************************************************/
struct MCC118DeviceInfo* mcc118_info(void)
{
    return &mcc118_device_info;
}

/******************************************************************************
  Read the serial number.
 *****************************************************************************/
int mcc118_serial(uint8_t address, char* buffer)
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
int mcc118_calibration_date(uint8_t address, char* buffer)
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
int mcc118_calibration_coefficient_read(uint8_t address, uint8_t channel,
    double* slope, double* offset)
{
    // validate parameters
    if (!_check_addr(address) ||
        (channel >= NUM_CHANNELS) ||
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
int mcc118_calibration_coefficient_write(uint8_t address, uint8_t channel,
    double slope, double offset)
{
    // validate parameters
    if (!_check_addr(address) ||
        (channel >= NUM_CHANNELS))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address]->scan_info)
    {
        return RESULT_BUSY;
    }

    _devices[address]->factory_data.slopes[channel] = slope;
    _devices[address]->factory_data.offsets[channel] = offset;
    return RESULT_SUCCESS;
}


/******************************************************************************
  Perform a single reading of an analog input channel and return the value.
 *****************************************************************************/
int mcc118_a_in_read(uint8_t address, uint8_t channel, uint32_t options,
    double* value)
{
    int ret;
    uint16_t code;
    double val;

    if (!_check_addr(address) ||
        (channel >= NUM_CHANNELS) ||
        (value == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send the AIn command
    ret = _spi_transfer(address, CMD_AIN, &channel, 1, &code, 2, 20*MSEC, 10);

    if (ret != RESULT_SUCCESS)
    {
        return ret;
    }

    // calibrate?
    if (options & OPTS_NOCALIBRATEDATA)
    {
        val = (double)code;
    }
    else
    {
        val = ((double)code * _devices[address]->factory_data.slopes[channel]) +
            _devices[address]->factory_data.offsets[channel];
    }

    // calculate voltage?
    if ((options & OPTS_NOSCALEDATA) == 0)
    {
        val = (val * LSB_SIZE) + VOLTAGE_MIN;
    }
    *value = val;
    return RESULT_SUCCESS;
}

/******************************************************************************
  Set the scan trigger mode.
 *****************************************************************************/
int mcc118_trigger_mode(uint8_t address, uint8_t mode)
{
    if (!_check_addr(address) ||
        (mode > TRIG_ACTIVE_LOW))
    {
        return RESULT_BAD_PARAMETER;
    }

    // don't allow changing while scan is running
    if (_devices[address]->scan_info != NULL)
    {
        return RESULT_BUSY;
    }

    _devices[address]->trigger_mode = mode;
    return RESULT_SUCCESS;
}

/******************************************************************************
  Read the actual scan rate for a set of scan parameters.
 *****************************************************************************/
int mcc118_a_in_scan_actual_rate(uint8_t channel_count,
    double sample_rate_per_channel, double* actual_sample_rate_per_channel)
{
    double adc_rate;
    uint32_t period;

    if ((channel_count == 0) || (channel_count > 8) ||
        (actual_sample_rate_per_channel == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    // Make sure the rate is within the board specs
    adc_rate = channel_count * sample_rate_per_channel;
    if (adc_rate > MAX_ADC_RATE)
    {
        *actual_sample_rate_per_channel = 0.0;
        return RESULT_BAD_PARAMETER;
    }

    // calculate period value
    if (sample_rate_per_channel <= ((double)CLOCK_TIMEBASE / 0xFFFFFFFF))
    {
        period = 0xFFFFFFFF;
    }
    else
    {
        period = (uint32_t)(CLOCK_TIMEBASE / sample_rate_per_channel + 0.5) - 1;
    }

    *actual_sample_rate_per_channel = CLOCK_TIMEBASE / (period + 1);

    return RESULT_SUCCESS;
}

/******************************************************************************
  Start an analog input scan.  This function will allocate a scan thread info
  structure and scan buffer, send the start command to the device, then start a
  scan data thread that constantly reads the scan status and data.
 *****************************************************************************/
int mcc118_a_in_scan_start(uint8_t address, uint8_t channel_mask,
    uint32_t samples_per_channel, double sample_rate_per_channel,
    uint32_t options)
{
    int result;
    uint8_t num_channels;
    uint8_t channel;
    double adc_rate;
    struct mcc118Device* dev;
    struct mcc118ScanThreadInfo* info;
    uint8_t buffer[10];
    uint32_t period;
    uint32_t scan_count;
    uint8_t scan_options;


    if (!_check_addr(address) ||
        (channel_mask == 0) ||
        ((samples_per_channel == 0) && ((options & OPTS_CONTINUOUS) == 0)))
    {
        return RESULT_BAD_PARAMETER;
    }

    dev = _devices[address];

    if (dev->scan_info != NULL)
    {
        // scan already running?
        return RESULT_BUSY;
    }

    dev->scan_info = (struct mcc118ScanThreadInfo*)calloc(
        sizeof(struct mcc118ScanThreadInfo), 1);
    if (dev->scan_info == NULL)
    {
        return RESULT_RESOURCE_UNAVAIL;
    }

    info = dev->scan_info;
    info->options = (uint16_t)options;

    num_channels = 0;
    for (channel = 0; channel < NUM_CHANNELS; channel++)
    {
        if (channel_mask & (1 << channel))
        {
            // save the channel list and coefficients for calibrating the
            // incoming data
            info->channels[num_channels] = channel;
            info->slopes[num_channels] = dev->factory_data.slopes[channel];
            info->offsets[num_channels] = dev->factory_data.offsets[channel];

            num_channels++;
        }
    }
    info->channel_count = num_channels;
    info->channel_index = 0;

    // Make sure the rate is within the board specs
    adc_rate = 0.0;
    if ((options & OPTS_EXTCLOCK) == 0)
    {
        // internal clock - we know our capabilities
        adc_rate = num_channels * sample_rate_per_channel;
        if (adc_rate > MAX_ADC_RATE)
        {
            free(info);
            dev->scan_info = NULL;
            return RESULT_BAD_PARAMETER;
        }
    }
    else
    {
        // external clock - this is just a suggestion, so use it for read rate
        adc_rate = num_channels * sample_rate_per_channel;
    }

    // Calculate the buffer size
    if (options & OPTS_CONTINUOUS)
    {
        // Continuous scan - buffer size is set to the (samples_per_channel
        // * number of channels) unless that value is less than:
        //
        // Rate         Buffer size
        // ----         -----------
        // None         10 kS per channel
        // < 100 S/s    1 kS per channel
        // < 10 kS/s    10 kS per channel
        // < 100 kS/s   100 kS per channel
        if (sample_rate_per_channel <= 100.0)
        {
            info->buffer_size = 1000;
        }
        else if (sample_rate_per_channel <= 10000.0)
        {
            info->buffer_size = 10000;
        }
        else if (sample_rate_per_channel <= 100000.0)
        {
            info->buffer_size = 100000;
        }
        else
        {
            info->buffer_size = 10000;
        }

        if (info->buffer_size < samples_per_channel)
        {
            info->buffer_size = samples_per_channel;
        }
    }
    else
    {
        // Finite scan - buffer size is the number of channels *
        // samples_per_channel,
        info->buffer_size = samples_per_channel;
    }

    info->buffer_size *= num_channels;

    // allocate the buffer
    info->scan_buffer = (double*)calloc(1, info->buffer_size * sizeof(double));
    if (info->scan_buffer == NULL)
    {
        // can't allocate memory
        free(info);
        dev->scan_info = NULL;
        return RESULT_RESOURCE_UNAVAIL;
    }

    // Set the device read threshold based on the scan rate - read data
    // every 100ms or faster.
    if ((adc_rate == 0.0) ||    // rate not specified
        (adc_rate > 2560.0))
    {
        info->read_threshold = COUNT_NORMALIZE(256, info->channel_count);
    }
    else
    {
        info->read_threshold = (uint16_t)(adc_rate / 10);
        info->read_threshold = COUNT_NORMALIZE(info->read_threshold,
            info->channel_count);
        if (info->read_threshold == 0)
        {
            info->read_threshold = info->channel_count;
        }
    }

    pthread_attr_t attr;
    if ((result = pthread_attr_init(&attr)) != 0)
    {
        free(info->scan_buffer);
        free(info);
        dev->scan_info = NULL;
        return RESULT_RESOURCE_UNAVAIL;
    }

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Start the scan
    scan_options = 0;
    if (options & OPTS_EXTTRIGGER)
    {
        scan_options |= (0x01 | (dev->trigger_mode << 1));
    }

    if (options & OPTS_EXTCLOCK)
    {
        // ignore the specified frequency and use the external clock
        period = 0;
    }
    else
    {
        // calculate period value
        if (sample_rate_per_channel <= ((double)CLOCK_TIMEBASE / 0xFFFFFFFF))
        {
            period = 0xFFFFFFFF;
        }
        else
        {
            period = (uint32_t)(CLOCK_TIMEBASE / sample_rate_per_channel + 0.5)
                - 1;
        }
    }

    if (options & OPTS_CONTINUOUS)
    {
        // set to 0 for continuous
        scan_count = 0;
    }
    else
    {
        scan_count = samples_per_channel;
    }


    buffer[0] = (uint8_t)scan_count;
    buffer[1] = (uint8_t)(scan_count >> 8);
    buffer[2] = (uint8_t)(scan_count >> 16);
    buffer[3] = (uint8_t)(scan_count >> 24);
    buffer[4] = (uint8_t)period;
    buffer[5] = (uint8_t)(period >> 8);
    buffer[6] = (uint8_t)(period >> 16);
    buffer[7] = (uint8_t)(period >> 24);
    buffer[8] = channel_mask;
    buffer[9] = scan_options;

    result = _spi_transfer(address, CMD_AINSCANSTART, buffer, 10, NULL, 0,
        20*MSEC, 0);

    if (result != RESULT_SUCCESS)
    {
        pthread_attr_destroy(&attr);
        free(info->scan_buffer);
        free(info);
        dev->scan_info = NULL;
        return result;
    }

    info->thread_started = false;

    // create the scan data thread
    uint8_t* temp_address = (uint8_t*)malloc(sizeof(uint8_t));
    *temp_address = address;
    if ((result = pthread_create(&info->handle, &attr, &_scan_thread,
        temp_address)) != 0)
    {
        free(temp_address);
        mcc118_a_in_scan_stop(address);
        pthread_attr_destroy(&attr);
        free(info->scan_buffer);
        free(info);
        dev->scan_info = NULL;
        return RESULT_RESOURCE_UNAVAIL;
    }

    pthread_attr_destroy(&attr);

    // Wait for thread to start to avoid race conditions reading thread status
    bool running;
    do
    {
        usleep(1);
        pthread_mutex_lock(&dev->scan_mutex);
        running = info->thread_started;
        pthread_mutex_unlock(&dev->scan_mutex);
    } while (!running);

    dev->scan_info->scan_running = true;

    return RESULT_SUCCESS;
}

/******************************************************************************
  Return the size of the internal scan buffer in samples (0 if scan is not
  running).
 *****************************************************************************/
int mcc118_a_in_scan_buffer_size(uint8_t address, uint32_t* buffer_size_samples)
{
    if (!_check_addr(address) ||
        (buffer_size_samples == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address]->scan_info == NULL)
    {
        return RESULT_RESOURCE_UNAVAIL;
    }

    *buffer_size_samples = _devices[address]->scan_info->buffer_size;
    return RESULT_SUCCESS;
}

/******************************************************************************
  Return the number of channels in the current scan (0 if scan is not running).
 *****************************************************************************/
int mcc118_a_in_scan_channel_count(uint8_t address)
{
    if (!_check_addr(address) ||
        (_devices[address]->scan_info == NULL))
    {
        return 0;
    }

    return _devices[address]->scan_info->channel_count;
}

/******************************************************************************
  Read the scan status and amount of data in the scan buffer.
 *****************************************************************************/
int mcc118_a_in_scan_status(uint8_t address, uint16_t* status,
    uint32_t* samples_per_channel)
{
    struct mcc118ScanThreadInfo* info;
    uint16_t stat;
    uint32_t buffer_depth;
    bool hw_overrun;
    bool buffer_overrun;
    bool triggered;
    bool scan_running;

    if (!_check_addr(address) ||
        (status == NULL))
    {
        return RESULT_BAD_PARAMETER;
    }

    stat = 0;

    if ((info = _devices[address]->scan_info) == NULL)
    {
        // scan not running?
        *status = 0;
        if (samples_per_channel)
        {
            *samples_per_channel = 0;
        }
        return RESULT_RESOURCE_UNAVAIL;
    }

    // get thread values
    pthread_mutex_lock(&_devices[address]->scan_mutex);
    buffer_depth = info->buffer_depth;
    hw_overrun = info->hw_overrun;
    buffer_overrun = info->buffer_overrun;
    triggered = info->triggered;
    scan_running = info->scan_running;
    pthread_mutex_unlock(&_devices[address]->scan_mutex);

    if (samples_per_channel)
    {
        *samples_per_channel = buffer_depth / info->channel_count;
    }

    if (hw_overrun)
    {
        stat |= STATUS_HW_OVERRUN;
    }
    if (buffer_overrun)
    {
        stat |= STATUS_BUFFER_OVERRUN;
    }
    if (triggered)
    {
        stat |= STATUS_TRIGGERED;
    }
    if (scan_running)
    {
        stat |= STATUS_RUNNING;
    }

    *status = stat;
    return RESULT_SUCCESS;
}

/******************************************************************************
  Read the specified amount of data from the scan buffer.  If
  samples_per_channel == -1, return all available samples.  If timeout is
  negative, wait indefinitely.  If it is 0,  return immediately with the
  available data.
 *****************************************************************************/
int mcc118_a_in_scan_read(uint8_t address, uint16_t* status,
    int32_t samples_per_channel, double timeout, double* buffer,
    uint32_t buffer_size_samples, uint32_t* samples_read_per_channel)
{
    uint32_t samples_to_read;
    uint32_t samples_read;
    uint32_t current_read;
    uint32_t max_read;
    bool no_timeout;
    bool timed_out;
    bool error;
    uint32_t timeout_us;
    struct mcc118ScanThreadInfo* info;
    struct timespec start_time;
    struct timespec current_time;
    uint16_t stat;
    uint32_t buffer_depth;
    bool hw_overrun;
    bool buffer_overrun;
    bool triggered;
    bool scan_running;
    bool thread_running;

    if (!_check_addr(address) ||
        (status == NULL) ||
        ((samples_per_channel > 0) &&
            ((buffer == NULL) || (buffer_size_samples == 0))))
    {
        return RESULT_BAD_PARAMETER;
    }

    stat = 0;
    samples_read = 0;
    error = false;
    timed_out = false;

    if (timeout < 0.0)
    {
        no_timeout = true;
        timeout_us = 0;
    }
    else
    {
        no_timeout = false;
        timeout_us = (uint32_t)(timeout * 1e6);
    }

    if ((info = _devices[address]->scan_info) == NULL)
    {
        // scan not running?
        *status = 0;
        if (samples_read_per_channel)
        {
            *samples_read_per_channel = 0;
        }
        return RESULT_RESOURCE_UNAVAIL;
    }

    // get thread values
    pthread_mutex_lock(&_devices[address]->scan_mutex);
    buffer_depth = info->buffer_depth;
    hw_overrun = info->hw_overrun;
    buffer_overrun = info->buffer_overrun;
    triggered = info->triggered;
    scan_running = info->scan_running;
    thread_running = info->thread_running;
    pthread_mutex_unlock(&_devices[address]->scan_mutex);

    // Determine how many samples to read
    if (samples_per_channel == -1)
    {
        // return all available, ignore timeout
        samples_to_read = buffer_depth;
    }
    else
    {
        // return the specified number of samples, depending on the timeout
        samples_to_read = samples_per_channel * info->channel_count;
    }

    if (buffer_size_samples < samples_to_read)
    {
        // buffer is not large enough, so read the amount of samples that will
        // fit
        samples_to_read = COUNT_NORMALIZE(buffer_size_samples,
            info->channel_count);
    }

    if (samples_to_read)
    {
        // Wait for the all of the data to be read or a timeout
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        timed_out = false;
        do
        {
            // update thread values
            pthread_mutex_lock(&_devices[address]->scan_mutex);
            buffer_depth = info->buffer_depth;
            hw_overrun = info->hw_overrun;
            buffer_overrun = info->buffer_overrun;
            triggered = info->triggered;
            scan_running = info->scan_running;
            thread_running = info->thread_running;
            pthread_mutex_unlock(&_devices[address]->scan_mutex);

            if (buffer_depth >= info->channel_count)
            {
                // read in increments of the number of channels in the scan
                current_read = MIN(buffer_depth, samples_to_read);
                current_read = COUNT_NORMALIZE(current_read,
                    info->channel_count);

                // check for a wrap at the end of the scan buffer
                max_read = info->buffer_size - info->read_index;
                if (max_read < current_read)
                {
                    // when wrapping, perform two copies
                    memcpy(&buffer[samples_read],
                        &info->scan_buffer[info->read_index],
                        max_read*sizeof(double));

                    samples_read += max_read;
                    memcpy(&buffer[samples_read], &info->scan_buffer[0],
                        (current_read - max_read)*sizeof(double));

                    samples_read += (current_read - max_read);
                    info->read_index = (current_read - max_read);
                }
                else
                {
                    memcpy(&buffer[samples_read],
                        &info->scan_buffer[info->read_index],
                        current_read*sizeof(double));
                    samples_read += current_read;
                    info->read_index += current_read;
                    if (info->read_index >= info->buffer_size)
                    {
                        info->read_index = 0;
                    }
                }

                samples_to_read -= current_read;
                buffer_depth -= current_read;
                pthread_mutex_lock(&_devices[address]->scan_mutex);
                info->buffer_depth -= current_read;
                pthread_mutex_unlock(&_devices[address]->scan_mutex);
            }
            usleep(100);

            if (!no_timeout)
            {
                clock_gettime(CLOCK_MONOTONIC, &current_time);
                timed_out = (_difftime_us(&start_time, &current_time) >=
                    timeout_us);
            }

            if (hw_overrun)
            {
                stat |= STATUS_HW_OVERRUN;
                error = true;
            }
            if (buffer_overrun)
            {
                stat |= STATUS_BUFFER_OVERRUN;
                error = true;
            }
        } while ((samples_to_read > 0) && !error &&
            (thread_running == false ? buffer_depth > 0 : true) &&
            !timed_out);

        if (samples_read_per_channel)
        {
            *samples_read_per_channel = samples_read / info->channel_count;
        }
    }
    else
    {
        // just update status
        if (hw_overrun)
        {
            stat |= STATUS_HW_OVERRUN;
        }
        if (buffer_overrun)
        {
            stat |= STATUS_BUFFER_OVERRUN;
        }

        if (samples_read_per_channel)
        {
            *samples_read_per_channel = 0;
        }
    }

    if (triggered)
    {
        stat |= STATUS_TRIGGERED;
    }
    if (scan_running)
    {
        stat |= STATUS_RUNNING;
    }

    *status = stat;

    if (!no_timeout && (timeout > 0.0) && timed_out && (samples_to_read > 0))
    {
        return RESULT_TIMEOUT;
    }
    else
    {
        return RESULT_SUCCESS;
    }
}

/******************************************************************************
  Stop a running scan by sending the scan stop command to the device.  The
  thread will  detect that the scan has stopped and terminate gracefully.
 *****************************************************************************/
int mcc118_a_in_scan_stop(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send scan stop command
    int ret = _spi_transfer(address, CMD_AINSCANSTOP, NULL, 0, NULL, 0, 20*MSEC,
        10);
    return ret;
}

/******************************************************************************
  Free the resources used by a scan.  If the scan thread is still running it
  will terminate the thread first.
 *****************************************************************************/
int mcc118_a_in_scan_cleanup(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address]->scan_info != NULL)
    {
        if (_devices[address]->scan_info->handle != 0)
        {
            // If the thread is running then tell it to stop and wait for it.
            // It will send the a_in_stop_scan command.
            pthread_mutex_lock(&_devices[address]->scan_mutex);
            _devices[address]->scan_info->stop_thread = true;
            pthread_mutex_unlock(&_devices[address]->scan_mutex);

            pthread_join(_devices[address]->scan_info->handle, NULL);
            _devices[address]->scan_info->handle = 0;
        }

        free(_devices[address]->scan_info->scan_buffer);
        free(_devices[address]->scan_info);
        _devices[address]->scan_info = NULL;

        pthread_mutex_unlock(&_devices[address]->scan_mutex);
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Test the CLK pin.  Can output different values, and will return the state
  of the pin for input testing.
 *****************************************************************************/
int mcc118_test_clock(uint8_t address, uint8_t mode, uint8_t* pValue)
{
    if (!_check_addr(address) ||
        (mode > 3))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send test clock command
    int ret = _spi_transfer(address, CMD_TESTCLOCK, &mode, 1, pValue,
        pValue ? 1 : 0, 20*MSEC, 0);

    return ret;
}

/******************************************************************************
  Test the TRIG pin by returning the current state.
 *****************************************************************************/
int mcc118_test_trigger(uint8_t address, uint8_t* pState)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send test trigger command
    int ret = _spi_transfer(address, CMD_TESTTRIGGER, NULL, 0, pState,
        pState ? 1 : 0, 20*MSEC, 0);

    return ret;
}

/******************************************************************************
  Read program memory from the bootloader region.
 *****************************************************************************/
int mcc118_bootmem_read(uint8_t address, uint16_t mem_address, uint16_t count,
    uint8_t* buffer)
{
    uint8_t temp[4];

    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    temp[0] = (uint8_t)mem_address;
    temp[1] = (uint8_t)(mem_address >> 8);
    temp[2] = (uint8_t)count;
    temp[3] = (uint8_t)(count >> 8);

    int ret = _spi_transfer(address, CMD_BOOTMEM_READ, temp, 4, buffer, count,
        20*MSEC, 100);

    return ret;
}

/******************************************************************************
  Write program memory in the bootloader region.
 *****************************************************************************/
int mcc118_bootmem_write(uint8_t address, uint16_t mem_address, uint16_t count,
    uint8_t* buffer)
{
    uint8_t* temp;

    if (!_check_addr(address) ||
        (count > (MAX_TX_DATA_SIZE-2)))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    temp = (uint8_t*)malloc(count + 2);
    temp[0] = (uint8_t)mem_address;
    temp[1] = (uint8_t)(mem_address >> 8);
    memcpy(&temp[2], buffer, count);

    int ret = _spi_transfer(address, CMD_BOOTMEM_WRITE, temp, count + 2, NULL,
        0, 500*MSEC, 100);

    free(temp);
    return ret;
}

/******************************************************************************
  Enter the bootloader for a firmware update.
 *****************************************************************************/
int mcc118_enter_bootloader(uint8_t address)
{
    int result;
    uint16_t id_data[3];

    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    if (_devices[address]->mode == 1)
    {
        // already in bootloader
        return RESULT_SUCCESS;
    }

    // reset the device
    if ((result = mcc118_reset(address)) != RESULT_SUCCESS)
    {
        return result;
    }

    // issue a bootloader command within 500ms to remain in bootloader
    usleep(200000);

    if ((result = _spi_transfer(address, CMD_BL_ENTER, NULL, 0, NULL, 0,
        20*MSEC, 10)) != RESULT_SUCCESS)
    {
        return result;
    }

    // Try to communicate with the device and verify that it is an MCC 118
    if ((result = _spi_transfer(address, CMD_ID, NULL, 0, id_data,
        3*sizeof(uint16_t), 20*MSEC, 10)) != RESULT_SUCCESS)
    {
        return result;
    }

    // save the firmware version
    _devices[address]->fw_version = id_data[1];
    _devices[address]->boot_version = id_data[2];
    _devices[address]->mode = (id_data[0] == (HAT_ID_MCC_118_BOOTLOADER));

    return RESULT_SUCCESS;
}

/******************************************************************************
  Erase the firmware program memory region.
 *****************************************************************************/
int mcc118_bl_erase(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    int ret = _spi_transfer(address, CMD_BL_ERASE, NULL, 0, NULL, 0, 1000*MSEC,
        1000);

    return ret;
}

/******************************************************************************
  Write firmware program memory.
 *****************************************************************************/
int mcc118_bl_write(uint8_t address, uint8_t* hex_record, uint8_t length)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    int ret = _spi_transfer(address, CMD_BL_WRITE, hex_record, length, NULL, 0,
        100*MSEC, 100);

    return ret;
}

/******************************************************************************
  Read the CRC of firmware program memory.
 *****************************************************************************/
int mcc118_bl_read_crc(uint8_t address, uint32_t mem_address, uint32_t count,
    uint16_t* pCRC)
{
    uint32_t args[2];

    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    args[0] = mem_address;
    args[1] = count;

    int ret = _spi_transfer(address, CMD_BL_READ_CRC, args, 2*sizeof(uint32_t),
        pCRC, pCRC ? sizeof(uint16_t) : 0, 500*MSEC, 1000);

    return ret;
}

/******************************************************************************
  Jump to the firmware from the bootloader.
 *****************************************************************************/
int mcc118_bl_jump(uint8_t address)
{
    if (!_check_addr(address))
    {
        return RESULT_BAD_PARAMETER;
    }

    // send command
    int ret = _spi_transfer(address, CMD_BL_JUMP, NULL, 0, NULL, 0, 20*MSEC,
        10);

    return ret;
}
