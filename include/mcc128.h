/**
*   @file mcc128.h
*   @author Measurement Computing Corp.
*   @brief This file contains definitions for the MCC 128.
*
*   03/17/2020
*/
#ifndef _MCC_128_H
#define _MCC_128_H

#include <stdint.h>

/// Analog input modes.
enum AnalogInputMode
{
    /// Single-ended
    A_IN_MODE_SE      = 0,
    /// Differential
    A_IN_MODE_DIFF    = 1
};

#define A_IN_MODE_SE_FLAG     0x00
#define A_IN_MODE_DIFF_FLAG   0x08
#define A_IN_MODE_BIT_MASK    0x08
#define A_IN_MODE_BIT_POS        3

/// Analog input ranges
enum AnalogInputRange
{
    /// +/- 10 V
    A_IN_RANGE_BIP_10V    = 0,
    /// +/- 5 V
    A_IN_RANGE_BIP_5V     = 1,
    /// +/- 2 V
    A_IN_RANGE_BIP_2V     = 2,
    /// +/- 1 V
    A_IN_RANGE_BIP_1V     = 3
};

#define A_IN_RANGE_BIP_10V_FLAG   0x00
#define A_IN_RANGE_BIP_5V_FLAG    0x10
#define A_IN_RANGE_BIP_2V_FLAG    0x20
#define A_IN_RANGE_BIP_1V_FLAG    0x30
#define A_IN_RANGE_BIT_MASK       0x30
#define A_IN_RANGE_BIT_POS           4

/// MCC 128 constant device information.
struct MCC128DeviceInfo
{
    /// The number of analog input modes (2.)
    const uint8_t NUM_AI_MODES;
    /// The number of analog input channels in each mode (8, 4.)
    const uint8_t NUM_AI_CHANNELS[2];
    /// The minimum ADC code (0.)
    const uint16_t AI_MIN_CODE;
    /// The maximum ADC code (65535.)
    const uint16_t AI_MAX_CODE;
    /// The number of analog input ranges (4.)
    const uint8_t NUM_AI_RANGES;
    /// The input voltage corresponding to the minimum code in each range
    ///   (-10.0V, -5.0V, -2.0V, -1.0V.)
    const double AI_MIN_VOLTAGE[4];
    /// The input voltage corresponding to the maximum code in each range
    ///   (+10.0V - 1 LSB, +5.0V - 1 LSB, +2.0V - 1 LSB, +1.0V - 1 LSB.)
    const double AI_MAX_VOLTAGE[4];
    /// The minimum voltage of the input range in each range
    ///   (-10.0V, -5.0V, -2.0V, -1.0V.)
    const double AI_MIN_RANGE[4];
    /// The maximum voltage of the input range in each range
    ///   (+10.0V, +5.0V, +2.0V, +1.0V.)
    const double AI_MAX_RANGE[4];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
*   @brief Open a connection to the MCC 128 device at the specified address.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_open(uint8_t address);

/**
*   @brief Check if an MCC 128 is open.
*
*   @param address  The board address (0 - 7).
*   @return 1 if open, 0 if not open.
*/
int mcc128_is_open(uint8_t address);

/**
*   @brief Close a connection to an MCC 128 device and free allocated resources.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_close(uint8_t address);

/**
*   @brief Return constant device information for all MCC 128s.
*
*   @return Pointer to struct MCC128DeviceInfo.
*
*/
struct MCC128DeviceInfo* mcc128_info(void);

/**
*   @brief Blink the LED on the MCC 128.
*
*   Passing 0 for count will result in the LED blinking continuously until the
*   board is reset or mcc128_blink_led() is called again with a non-zero value
*   for count.
*
*   @param address  The board address (0 - 7).
*   @param count    The number of times to blink (0 - 255).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_blink_led(uint8_t address, uint8_t count);

/**
*   @brief Return the board firmware version.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param version  Receives the firmware version.  The version will be in BCD
*       hexadecimal with the high byte as the major version and low byte as
*       minor, i.e. 0x0103 is version 1.03.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_firmware_version(uint8_t address, uint16_t* version);

/**
*   @brief Read the MCC 128 serial number
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the serial
*       number as a string.  The buffer must be at least 9 characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_serial(uint8_t address, char* buffer);

/**
*   @brief Read the MCC 128 calibration date
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the date as
*       a string (format "YYYY-MM-DD").  The buffer must be at least 11
*       characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_calibration_date(uint8_t address, char* buffer);

/**
*   @brief Read the MCC 128 calibration coefficients for a specified input
*       range.
*
*   The coefficients are applied in the library as:
*
*       calibrated_ADC_code = (raw_ADC_code * slope) + offset
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param range    The input range, one of the
*                   [input range](@ref AnalogInputRange) values.
*   @param slope    Receives the slope.
*   @param offset   Receives the offset.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_calibration_coefficient_read(uint8_t address, uint8_t range,
    double* slope, double* offset);

/**
*   @brief Temporarily write the MCC 128 calibration coefficients for a
*       specified input range.
*
*   The user can apply their own calibration coefficients by writing to these
*   values. The values will reset to the factory values from the EEPROM whenever
*   mcc128_open() is called. This function will fail and return
*   [RESULT_BUSY](@ref RESULT_BUSY) if a scan is active when it is called.
*
*   The coefficients are applied in the library as:
*
*       calibrated_ADC_code = (raw_ADC_code * slope) + offset
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param range    The input range, one of the
*                   [input range](@ref AnalogInputRange) values.
*   @param slope    The new slope value.
*   @param offset   The new offset value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_calibration_coefficient_write(uint8_t address, uint8_t range,
    double slope, double offset);

/**
*   @brief Set the analog input mode.
*
*   This sets the analog inputs to one of the valid values:
*       - [A_IN_MODE_SE](@ref A_IN_MODE_SE): Single-ended (8 inputs relative to
*           ground.)
*       - [A_IN_MODE_DIFF](@ref A_IN_MODE_DIFF): Differential (4 channels with
*           positive and negative inputs.)
*
*   This function will fail and return [RESULT_BUSY](@ref RESULT_BUSY) if a scan
*   is active when it is called.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param mode     One of the [input mode](@ref AnalogInputMode) values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_mode_write(uint8_t address, uint8_t mode);

/**
*   @brief Read the analog input mode.
*
*   Reads the current analog [input mode](@ref AnalogInputMode).
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param mode     Receives the [input mode](@ref AnalogInputMode).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_mode_read(uint8_t address, uint8_t* mode);

/**
*   @brief Set the analog input range.
*
*   This sets the analog input range to one of the valid ranges:
*       - [A_IN_RANGE_BIP_10V](@ref A_IN_RANGE_BIP_10V): +/- 10V
*       - [A_IN_RANGE_BIP_5V](@ref A_IN_RANGE_BIP_5V): +/- 5V
*       - [A_IN_RANGE_BIP_2V](@ref A_IN_RANGE_BIP_2V): +/- 2V
*       - [A_IN_RANGE_BIP_1V](@ref A_IN_RANGE_BIP_1V): +/- 1V
*
*   This function will fail and return [RESULT_BUSY](@ref RESULT_BUSY) if a scan
*   is active when it is called.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param range    One of the [input range](@ref AnalogInputRange) values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_range_write(uint8_t address, uint8_t range);

/**
*   @brief Read the analog input range.
*
*   Returns the current analog [input range](@ref AnalogInputRange).
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param range    Receives the [input range](@ref AnalogInputRange).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_range_read(uint8_t address, uint8_t* range);

/**
*   @brief Perform a single reading of an analog input channel and return the
*       value.
*
*   The valid options are:
*       - [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA): Return ADC code (a value
*           between 0 and 65535) rather than voltage.
*       - [OPTS_NOCALIBRATEDATA](@ref OPTS_NOCALIBRATEDATA): Return data without
*           the calibration factors applied.
*
*   The options parameter is set to 0 or [OPTS_DEFAULT](@ref OPTS_DEFAULT) for
*   default operation, which is scaled and calibrated data.
*
*   Multiple options may be specified by ORing the flags. For instance,
*   specifying [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA) |
*   [OPTS_NOCALIBRATEDATA](@ref OPTS_NOCALIBRATEDATA) will return the value read
*   from the ADC without calibration or converting to voltage.
*
*   The function will return [RESULT_BUSY](@ref RESULT_BUSY) if called while a
*   scan is running.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog input channel number, 0 - 7.
*   @param options  Options bitmask.
*   @param value    Receives the analog input value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_read(uint8_t address, uint8_t channel, uint32_t options,
    double* value);

/**
*   @brief Set the trigger input mode.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param mode     One of the [trigger mode](@ref TriggerMode) values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_trigger_mode(uint8_t address, uint8_t mode);

/**
*   @brief Read the actual sample rate per channel for a requested sample rate.
*
*   The internal scan clock is generated from a 16 MHz clock source so only
*   discrete frequency steps can be achieved.  This function will return the
*   actual rate for a requested channel count and rate. This function does not
*   perform any actions with a board, it simply calculates the rate.
*
*   @param channel_count    The number of channels in the scan.
*   @param sample_rate_per_channel   The desired sampling rate in samples per
*       second per channel, max 100,000.
*   @param actual_sample_rate_per_channel   The actual sample rate that would
*       occur when requesting this rate on an MCC 128, or 0 if there is an
*       error.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful,
*       [RESULT_BAD_PARAMETER](@ref RESULT_BAD_PARAMETER) if the scan parameters
*       are not achievable on an MCC 128.
*/
int mcc128_a_in_scan_actual_rate(uint8_t channel_count,
    double sample_rate_per_channel, double* actual_sample_rate_per_channel);

int mcc128_a_in_scan_queue_start(uint8_t address, uint8_t queue_count,
    uint8_t* queue, uint32_t samples_per_channel,
    double sample_rate_per_channel, uint32_t options);

/**
*   @brief Start a hardware-paced analog input scan.
*
*   The scan runs as a separate thread from the user's code.  The function will
*   allocate a scan buffer and read data from the device into that buffer.  The
*   user reads the data from this buffer and the scan status using the
*   mcc128_a_in_scan_read() function. mcc128_a_in_scan_stop() is used to stop a
*   continuous scan, or to stop a finite scan before it completes.  The user
*   must call mcc128_a_in_scan_cleanup() after the scan has finished and all
*   desired data has been read; this frees all resources from the scan and
*   allows additional scans to be performed.
*
*   The scan state has defined terminology:
*   - \b Active: mcc128_a_in_scan_start() has been called and the device may be
*       acquiring data or finished with the acquisition. The scan has not been
*       cleaned up by calling mcc128_a_in_scan_cleanup(), so another scan may
*       not be started.
*   - \b Running: The scan is active and the device is still acquiring data.
*       Certain functions like mcc128_a_in_read() will return an error because
*       the device is busy.
*
*   The valid options are:
*       - [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA): Returns ADC code (a value
*           between 0 and 65535) rather than voltage.
*       - [OPTS_NOCALIBRATEDATA](@ref OPTS_NOCALIBRATEDATA): Return data without
*           the calibration factors applied.
*       - [OPTS_EXTCLOCK](@ref OPTS_EXTCLOCK): Use an external 3.3V or 5V logic
*           signal at the CLK input as the scan clock. Multiple devices can be
*           synchronized by connecting the CLK pins together and using this
*           option on all but one device so they will be clocked by the
*           single device using its internal clock. \b sample_rate_per_channel
*           is only used for buffer sizing.
*       - [OPTS_EXTTRIGGER](@ref OPTS_EXTTRIGGER): Hold off the scan (after
*           calling mcc128_a_in_scan_start()) until the trigger condition is
*           met.  The trigger is a 3.3V or 5V logic signal applied to the TRIG
*           pin.
*       - [OPTS_CONTINUOUS](@ref OPTS_CONTINUOUS): Scans continuously until
*           stopped by the user by calling mcc128_a_in_scan_stop() and writes
*           data to a circular buffer. The data must be read before being
*           overwritten to avoid a buffer overrun error. \b samples_per_channel
*           is only used for buffer sizing.
*
*   The options parameter is set to 0 or [OPTS_DEFAULT](@ref OPTS_DEFAULT) for
*   default operation, which is scaled and calibrated data, internal scan clock,
*   no trigger, and finite operation.
*
*   Multiple options may be specified by ORing the flags. For instance,
*   specifying [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA) |
*   [OPTS_NOCALIBRATEDATA](@ref OPTS_NOCALIBRATEDATA) will return the values
*   read from the ADC without calibration or converting to voltage.
*
*   The buffer size will be allocated as follows:
*
*   \b Finite mode: Total number of samples in the scan
*
*   \b Continuous mode (buffer size is per channel): Either
*   \b samples_per_channel or the value in the following table, whichever is
*   greater
*
*   \verbatim embed:rst:leading-asterisk
*   ==============      =========================
*   Sample Rate         Buffer Size (per channel)
*   ==============      =========================
*   Not specified       10 kS
*   0-100 S/s           1 kS
*   100-10k S/s         10 kS
*   10k-100k S/s        100 kS
*   ==============      =========================
*   \endverbatim
*
*   Specifying a very large value for \b samples_per_channel could use too much
*   of the Raspberry Pi memory. If the memory allocation fails, the function
*   will return [RESULT_RESOURCE_UNAVAIL](@ref RESULT_RESOURCE_UNAVAIL). The
*   allocation could succeed, but the lack of free memory could cause other
*   problems in the Raspberry Pi. If you need to acquire a high number of
*   samples then it is better to run the scan in continuous mode and stop it
*   when you have acquired the desired amount of data.  If a scan is already
*   active this function will return [RESULT_BUSY](@ref RESULT_BUSY).
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel_mask  A bit mask of the channels to be scanned. Set each bit
*       to enable the associated channel (0x01 - 0xFF.)
*   @param samples_per_channel  The number of samples to acquire for each
*       channel in the scan (finite mode,) or can be used to set a larger scan
*       buffer size than the default value (continuous mode.)
*   @param sample_rate_per_channel   The sampling rate in samples per second per
*       channel, max 100,000. When using an external sample clock set this value
*       to the maximum expected rate of the clock.
*   @param options  The options bitmask.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful,
*       [RESULT_BUSY](@ref RESULT_BUSY) if a scan is already active.
*/
int mcc128_a_in_scan_start(uint8_t address, uint8_t channel_mask,
    uint32_t samples_per_channel, double sample_rate_per_channel,
    uint32_t options);

/**
*   @brief Returns the size of the internal scan data buffer.
*
*   An internal data buffer is allocated for the scan when
*   mcc128_a_in_scan_start() is called. This function returns the total size of
*   that buffer in samples.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer_size_samples  Receives the size of the buffer in samples. Each
*       sample is a \b double.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful,
*       [RESULT_RESOURCE_UNAVAIL](@ref RESULT_RESOURCE_UNAVAIL) if a scan is not
*       currently active,
*       [RESULT_BAD_PARAMETER](@ref RESULT_BAD_PARAMETER) if the address is
*       invalid or buffer_size_samples is NULL.
*/
int mcc128_a_in_scan_buffer_size(uint8_t address,
    uint32_t* buffer_size_samples);

/**
*   @brief Reads status and number of available samples from an analog input
*   scan.
*
*   The scan is started with mcc128_a_in_scan_start() and runs in a background
*   thread that reads the data from the board into an internal scan buffer.
*   This function reads the status of the scan and amount of data in the scan
*   buffer.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param status   Receives the scan status, an ORed combination of the flags:
*       - [STATUS_HW_OVERRUN](@ref STATUS_HW_OVERRUN): The device scan buffer
*           was not read fast enough and data was lost.
*       - [STATUS_BUFFER_OVERRUN](@ref STATUS_BUFFER_OVERRUN): The thread scan
*           buffer was not read by the user fast enough and data was lost.
*       - [STATUS_TRIGGERED](@ref STATUS_TRIGGERED): The trigger conditions have
*           been met.
*       - [STATUS_RUNNING](@ref STATUS_RUNNING): The scan is running.
*   @param samples_per_channel  Receives the number of samples per channel
*       available in the scan thread buffer.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful,
*       [RESULT_RESOURCE_UNAVAIL](@ref RESULT_RESOURCE_UNAVAIL) if a scan has
*       not been started under this instance of the device.
*/
int mcc128_a_in_scan_status(uint8_t address, uint16_t* status,
    uint32_t* samples_per_channel);

/**
*   @brief Reads status and multiple samples from an analog input scan.
*
*   The scan is started with mcc128_a_in_scan_start() and runs in a background
*   thread that reads the data from the board into an internal scan buffer.
*   This function reads the data from the scan buffer, and returns the current
*   scan status.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param status   Receives the scan status, an ORed combination of the flags:
*       - [STATUS_HW_OVERRUN](@ref STATUS_HW_OVERRUN): The device scan buffer
*           was not read fast enough and data was lost.
*       - [STATUS_BUFFER_OVERRUN](@ref STATUS_BUFFER_OVERRUN): The thread scan
*           buffer was not read by the user fast enough and data was lost.
*       - [STATUS_TRIGGERED](@ref STATUS_TRIGGERED): The trigger conditions have
*           been met.
*       - [STATUS_RUNNING](@ref STATUS_RUNNING): The scan is running.
*   @param samples_per_channel  The number of samples per channel to read.
*       Specify \b -1 to read all available samples in the scan thread buffer,
*       ignoring \b timeout. If \b buffer does not contain enough space then the
*       function will read as many samples per channel as will fit in \b buffer.
*   @param timeout  The amount of time in seconds to wait for the samples to be
*       read. Specify a negative number to wait indefinitely or \b 0 to return
*       immediately with whatever samples are available (up to the value of
*       \b samples_per_channel or \b buffer_size_samples.)
*   @param buffer   The user data buffer that receives the samples.
*   @param buffer_size_samples  The size of the buffer in samples. Each sample
*       is a \b double.
*   @param samples_read_per_channel Returns the actual number of samples read
*       from each channel.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful,
*       [RESULT_RESOURCE_UNAVAIL](@ref RESULT_RESOURCE_UNAVAIL) if a scan is not
*           active.
*/
int mcc128_a_in_scan_read(uint8_t address, uint16_t* status,
    int32_t samples_per_channel, double timeout, double* buffer,
    uint32_t buffer_size_samples, uint32_t* samples_read_per_channel);

/**
*   @brief Stops an analog input scan.
*
*   The scan is stopped immediately.  The scan data that has been read into the
*   scan buffer is available until mcc128_a_in_scan_cleanup() is called.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_scan_stop(uint8_t address);

/**
*   @brief Free analog input scan resources after the scan is complete.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_a_in_scan_cleanup(uint8_t address);

/**
*   @brief Return the number of channels in the current analog input scan.
*
*   This function returns 0 if no scan is active.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return The number of channels, 0 - 8.
*/
int mcc128_a_in_scan_channel_count(uint8_t address);

/**
*   @brief Test the CLK pin.
*
*   This function will return RESULT_BUSY if called while a scan is running.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param mode The CLK pin mode
*       - 0 = input
*       - 1 = output low
*       - 2 = output high
*       - 3 = output 1 kHz square wave
*   @param value   Receives the value at the CLK pin after setting the mode.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_test_clock(uint8_t address, uint8_t mode, uint8_t* value);

/**
*   @brief Test the TRIG pin by returning the current state.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param state    Receives the TRIG pin state (0 or 1.)
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc128_test_trigger(uint8_t address, uint8_t* state);

#ifdef __cplusplus
}
#endif

#endif
