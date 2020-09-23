/**
*   @file mcc172.h
*   @author Measurement Computing Corp.
*   @brief This file contains definitions for the MCC 172.
*
*   10/02/2019
*/
#ifndef _MCC_172_H
#define _MCC_172_H

#include <stdint.h>

/// MCC 172 constant device information.
struct MCC172DeviceInfo
{
    /// The number of analog input channels (2.)
    const uint8_t NUM_AI_CHANNELS;
    /// The minimum ADC code (-8,388,608.)
    const int32_t AI_MIN_CODE;
    /// The maximum ADC code (8,388,607.)
    const int32_t AI_MAX_CODE;
    /// The input voltage corresponding to the minimum code (-5.0V.)
    const double AI_MIN_VOLTAGE;
    /// The input voltage corresponding to the maximum code (+5.0V - 1 LSB.)
    const double AI_MAX_VOLTAGE;
    /// The minimum voltage of the input range (-5.0V.)
    const double AI_MIN_RANGE;
    /// The maximum voltage of the input range (+5.0V.)
    const double AI_MAX_RANGE;
};

/// Clock / trigger source definitions.
enum SourceType
{
    /// Use a local-only source.
    SOURCE_LOCAL    = 0,
    /// Use a local source and set it as master.
    SOURCE_MASTER   = 1,
    /// Use a separate master source.
    SOURCE_SLAVE    = 2
};

#ifdef __cplusplus
extern "C" {
#endif

//int mcc172_test(uint8_t address, double* value);

/**
*   @brief Open a connection to the MCC 172 device at the specified address.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_open(uint8_t address);

/**
*   @brief Check if an MCC 172 is open.
*
*   @param address  The board address (0 - 7).
*   @return 1 if open, 0 if not open.
*/
int mcc172_is_open(uint8_t address);

/**
*   @brief Close a connection to an MCC 172 device and free allocated resources.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_close(uint8_t address);

/**
*   @brief Return constant device information for all MCC 172s.
*
*   @return Pointer to struct MCC172DeviceInfo.
*
*/
struct MCC172DeviceInfo* mcc172_info(void);

/**
*   @brief Blink the LED on the MCC 172.
*
*   Passing 0 for count will result in the LED blinking continuously until the
*   board is reset or mcc172_blink_led() is called again with a non-zero value
*   for count.
*
*   @param address  The board address (0 - 7).
*   @param count    The number of times to blink (0 - 255).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_blink_led(uint8_t address, uint8_t count);

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
int mcc172_firmware_version(uint8_t address, uint16_t* version);

/**
*   @brief Read the MCC 172 serial number
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the serial
*       number as a string.  The buffer must be at least 9 characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_serial(uint8_t address, char* buffer);

/**
*   @brief Read the MCC 172 calibration date
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the date as
*       a string (format "YYYY-MM-DD").  The buffer must be at least 11
*       characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_calibration_date(uint8_t address, char* buffer);

/**
*   @brief Read the MCC 172 calibration coefficients for a single channel.
*
*   The coefficients are applied in the library as:
*
*       calibrated_ADC_code = (raw_ADC_code - offset) * slope
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 1).
*   @param slope    Receives the slope.
*   @param offset   Receives the offset.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_calibration_coefficient_read(uint8_t address, uint8_t channel,
    double* slope, double* offset);

/**
*   @brief Temporarily write the MCC 172 calibration coefficients for a single
*       channel.
*
*   The user can apply their own calibration coefficients by writing to these
*   values. The values will reset to the factory values from the EEPROM whenever
*   mcc172_open() is called. This function will fail and return
*   [RESULT_BUSY](@ref RESULT_BUSY) if a scan is active when it is called.
*
*   The coefficients are applied in the library as:
*
*       calibrated_ADC_code = (raw_ADC_code - offset) * slope
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 1).
*   @param slope    The new slope value.
*   @param offset   The new offset value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_calibration_coefficient_write(uint8_t address, uint8_t channel,
    double slope, double offset);

/**
*   @brief Read the MCC 172 IEPE configuration for a single channel.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 1).
*   @param config   Receives the configuration for the specified channel:
*       - 0: IEPE power off
*       - 1: IEPE power on
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_iepe_config_read(uint8_t address, uint8_t channel, uint8_t* config);

/**
*   @brief Write the MCC 172 IEPE configuration for a single channel.
*
*   Writes the new IEPE configuration for a channel. This function will fail and
*   return [RESULT_BUSY](@ref RESULT_BUSY) if a scan is active when it is
*   called.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 1).
*   @param config   The IEPE configuration for the specified channel:
*       - 0: IEPE power off
*       - 1: IEPE power on
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_iepe_config_write(uint8_t address, uint8_t channel, uint8_t config);

/**
*   @brief Read the MCC 172 analog input sensitivity scaling factor for a single
*       channel.
*
*   The sensitivity is specified in mV / mechanical unit. The default value
*   when opening the library is 1000, resulting in no scaling of the input
*   voltage.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 1).
*   @param value    Receives the sensitivity for the specified channel
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_a_in_sensitivity_read(uint8_t address, uint8_t channel,
    double* value);

/**
*   @brief Write the MCC 172 analog input sensitivity scaling factor for a
*       single channel.
*
*   This applies a scaling factor to the analog input data so it returns values
*   that are meaningful for the connected sensor.
*
*   The sensitivity is specified in mV / mechanical unit. The default value
*   when opening the library is 1000, resulting in no scaling of the input
*   voltage.  Changing this value will not change the values reported by
*   mcc172_info() since it is simply sensor scaling applied to the data before
*   returning it.
*
*   Examples:
*
*   - A seismic sensor with a sensitivity of 10 V/g. Set the sensitivity to
*     10,000 and the returned data will be in units of g.
*   - A vibration sensor with a sensitivity of 100 mV/g.  Set the sensitivity to
*     100 and the returned data will be in units of g.
*
*   This function will fail and return [RESULT_BUSY](@ref RESULT_BUSY) if a scan
*   is active when it is called.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 1).
*   @param value    The sensitivity for the specified channel.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_a_in_sensitivity_write(uint8_t address, uint8_t channel,
    double value);

/**
*   @brief Read the sampling clock configuration.
*
*   This function will return the sample clock configuration and rate. If the
*   clock is configured for local or master source, then the rate will be the
*   internally adjusted rate set by the user.  If the clock is configured for
*   slave source, then the rate will be measured from the master clock after the
*   synchronization period has ended. The synchronization status is also
*   returned.
*
*   The clock source will be one of the following values:
*
*   - [SOURCE_LOCAL](@ref SOURCE_LOCAL): The clock is generated on this MCC
*     172 and not shared with other MCC 172s.
*   - [SOURCE_MASTER](@ref SOURCE_MASTER): The clock is generated on this
*     MCC 172 and is shared as the master clock for other MCC 172s.
*   - [SOURCE_SLAVE](@ref SOURCE_SLAVE): No clock is generated on this MCC
*     172, it receives its clock from the master MCC 172.
*
*   The data rate will not be valid in slave mode if synced is equal to 0.
*   The device will not detect a loss of the master clock when in slave mode;
*   it only monitors the clock when a sync is initiated.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param clock_source Receives the ADC clock source, one of the
*       [source type](@ref SourceType) values.
*   @param sample_rate_per_channel   Receives the sample rate in samples per
*       second per channel
*   @param synced   Receives the syncronization status (0: sync in progress,
*       1: sync complete)
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful
*/
int mcc172_a_in_clock_config_read(uint8_t address, uint8_t* clock_source,
    double* sample_rate_per_channel, uint8_t* synced);

/**
*   @brief Write the sampling clock configuration.
*
*   This function will configure the ADC sampling clock. The default
*   configuration after opening the device is local mode, 51.2 KHz data rate.
*
*   The clock_source must be one of:
*
*   - [SOURCE_LOCAL](@ref SOURCE_LOCAL): The clock is generated on this MCC
*     172 and not shared with other MCC 172s.
*   - [SOURCE_MASTER](@ref SOURCE_MASTER): The clock is generated on this
*     MCC 172 and is shared as the master clock for other MCC 172s. All
*     other MCC 172s must be configured for local or slave clock.
*   - [SOURCE_SLAVE](@ref SOURCE_SLAVE): No clock is generated on this MCC
*     172, it receives its clock from the master MCC 172.
*
*   The ADCs will be synchronized so they sample the inputs at the same time.
*   This requires 128 clock cycles before the first sample is available. When
*   using a master - slave clock configuration for multiple MCC 172s there are
*   additional considerations:
*
*   - There should be only one master device; otherwise, you will be
*     connecting multiple outputs together and could damage a device.
*   - Configure the clock on the slave device(s) first, master last. The
*     synchronization will occur when the master clock is configured, causing
*     the ADCs on all the devices to be in sync.
*   - If you change the clock configuration on one device after configuring
*     the master, then the data will no longer be in sync.  The devices cannot
*     detect this and will still report that they are synchronized.  Always
*     write the clock configuration to all devices when modifying the
*     configuration.
*   - Slave devices must have a master clock source or scans will never
*     complete.
*   - A trigger must be used for the data streams from all devices to start on
*     the same sample.
*
*   The MCC 172 can generate an ADC sampling clock equal to 51.2 kHz divided by
*   an integer between 1 and 256. The data_rate_per_channel will be internally
*   converted to the nearest valid rate. The actual rate can be read back
*   using mcc172_a_in_clock_config_read(). When used in slave clock
*   configuration, the device will measure the frequency of the incoming master
*   clock after the synchronization period is complete. Calling
*   mcc172_a_in_clock_config_read() after this will return the measured data
*   rate.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param clock_source The ADC clock source, one of the
*       [source type](@ref SourceType) values.
*   @param sample_rate_per_channel   The requested sample rate in samples per
*       second per channel
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful
*/
int mcc172_a_in_clock_config_write(uint8_t address, uint8_t clock_source,
    double sample_rate_per_channel);

/**
*   @brief Configure the digital trigger.
*
*   The analog input scan may be configured to start saving the acquired data
*   when the digital trigger is in the desired state. A single device trigger
*   may also be shared with multiple boards. This command sets the trigger
*   source and mode.
*
*   The trigger source must be one of:
*
*   - [SOURCE_LOCAL](@ref SOURCE_LOCAL): The trigger terminal on this MCC
*     172 is used and not shared with other MCC 172s.
*   - [SOURCE_MASTER](@ref SOURCE_MASTER): The trigger terminal on this MCC
*     172 is used and is shared as the master trigger for other MCC 172s.
*   - [SOURCE_SLAVE](@ref SOURCE_SLAVE): The trigger terminal on this MCC
*     172 is not used, it receives its trigger from the master MCC 172.
*
*   The trigger mode must be one of:
*
*   - [TRIG_RISING_EDGE](@ref TRIG_RISING_EDGE): Start the scan on a rising edge
*     of TRIG.
*   - [TRIG_FALLING_EDGE](@ref TRIG_FALLING_EDGE): Start the scan on a falling
*     edge of TRIG.
*   - [TRIG_ACTIVE_HIGH](@ref TRIG_ACTIVE_HIGH): Start the scan any time TRIG is
*     high.
*   - [TRIG_ACTIVE_LOW](@ref TRIG_ACTIVE_LOW): Start the scan any time TRIG is
*     low.
*
*   Due to the nature of the filtering in the A/D converters there is an input
*   delay of 39 samples, so the data coming from the converters at any time is
*   delayed by 39 samples from the current time.  This is most noticeable when
*   using a trigger - there will be approximately 39 samples prior to the
*   trigger event in the captured data.
*
*   Care must be taken when using master / slave triggering; the input trigger
*   signal on the master will be passed through to the slave(s), but the mode
*   is set independently on each device.  For example, it is possible for the
*   master to trigger on the rising edge of the signal and the slave to trigger
*   on the falling edge.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param source   The trigger source, one of the
*       [source type](@ref SourceType) values.
*   @param mode     The trigger mode, one of the
*       [trigger mode](@ref TriggerMode) values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_trigger_config(uint8_t address, uint8_t source, uint8_t mode);

/**
*   @brief Start capturing analog input data from the specified channels
*
*   The scan runs as a separate thread from the user's code.  The function will
*   allocate a scan buffer and read data from the device into that buffer.  The
*   user reads the data from this buffer and the scan status using the
*   mcc172_a_in_scan_read() function. mcc172_a_in_scan_stop() is used to stop a
*   continuous scan, or to stop a finite scan before it completes.  The user
*   must call mcc172_a_in_scan_cleanup() after the scan has finished and all
*   desired data has been read; this frees all resources from the scan and
*   allows additional scans to be performed.
*
*   The scan cannot be started until the ADCs are synchronized, so this function
*   will not return until that has completed. It is best to wait for sync using
*   mcc172_a_in_clock_config_read() before starting the scan.
*
*   The scan state has defined terminology:
*   - \b Active: mcc172_a_in_scan_start() has been called and the device may be
*       acquiring data or finished with the acquisition. The scan has not been
*       cleaned up by calling mcc172_a_in_scan_cleanup(), so another scan may
*       not be started.
*   - \b Running: The scan is active and the device is still acquiring data.
*       Certain functions will return an error because the device is busy.
*
*   The valid options are:
*       - [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA): Returns ADC code (a value
*           between AI_MIN_CODE and AI_MAX_CODE) rather than voltage.
*       - [OPTS_NOCALIBRATEDATA](@ref OPTS_NOCALIBRATEDATA): Return data without
*           the calibration factors applied.
*       - [OPTS_EXTTRIGGER](@ref OPTS_EXTTRIGGER): Hold off the scan (after
*           calling mcc172_a_in_scan_start()) until the trigger condition is
*           met.
*       - [OPTS_CONTINUOUS](@ref OPTS_CONTINUOUS): Scans continuously until
*           stopped by the user by calling mcc172_a_in_scan_stop() and writes
*           data to a circular buffer. The data must be read before being
*           overwritten to avoid a buffer overrun error. \b samples_per_channel
*           is only used for buffer sizing.
*
*   The [OPTS_EXTCLOCK](@ref OPTS_EXTCLOCK) option is not supported for this
*   device and will return an error.
*
*   The options parameter is set to 0 or [OPTS_DEFAULT](@ref OPTS_DEFAULT) for
*   default operation, which is scaled and calibrated data, no trigger, and
*   finite operation.
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
*   =====================      =========================
*   Sample Rate                Buffer Size (per channel)
*   =====================      =========================
*   200-1024 S/s                 1 kS
*   1280-10.24 kS/s             10 kS
*   12.8, 25.6, 51.2 kS/s      100 kS
*   =====================      =========================
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
*       to enable the associated channel (0x01 - 0x03.)
*   @param samples_per_channel  The number of samples to acquire for each
*       channel in the scan (finite mode,) or can be used to set a larger scan
*       buffer size than the default value (continuous mode.)
*   @param options  The options bitmask.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful,
*       [RESULT_BUSY](@ref RESULT_BUSY) if a scan is already active.
*/
int mcc172_a_in_scan_start(uint8_t address, uint8_t channel_mask,
    uint32_t samples_per_channel, uint32_t options);

/**
*   @brief Returns the size of the internal scan data buffer.
*
*   An internal data buffer is allocated for the scan when
*   mcc172_a_in_scan_start() is called. This function returns the total size of
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
int mcc172_a_in_scan_buffer_size(uint8_t address,
    uint32_t* buffer_size_samples);

/**
*   @brief Reads status and number of available samples from an analog input
*   scan.
*
*   The scan is started with mcc172_a_in_scan_start() and runs in a background
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
int mcc172_a_in_scan_status(uint8_t address, uint16_t* status,
    uint32_t* samples_per_channel);

/**
*   @brief Reads status and multiple samples from an analog input scan.
*
*   The scan is started with mcc172_a_in_scan_start() and runs in a background
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
int mcc172_a_in_scan_read(uint8_t address, uint16_t* status,
    int32_t samples_per_channel, double timeout, double* buffer,
    uint32_t buffer_size_samples, uint32_t* samples_read_per_channel);

/**
*   @brief Stops an analog input scan.
*
*   The scan is stopped immediately.  The scan data that has been read into the
*   scan buffer is available until mcc172_a_in_scan_cleanup() is called.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_a_in_scan_stop(uint8_t address);

/**
*   @brief Free analog input scan resources after the scan is complete.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_a_in_scan_cleanup(uint8_t address);

/**
*   @brief Return the number of channels in the current analog input scan.
*
*   This function returns 0 if no scan is active.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return The number of channels, 0 - 2.
*/
int mcc172_a_in_scan_channel_count(uint8_t address);

/**
*   @brief Read the state of shared signals for testing.
*
*   This function reads the state of the ADC clock, sync, and trigger signals.
*   Use it in conjunction with mcc172_a_in_clock_config_write() and
*   mcc172_trigger_config() to put the signals into slave mode then set values
*   on the signals using the Pi GPIO pins.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param clock    Receives the current logic level of the ADC clock.
*   @param sync     Receives the current logic level of sync.
*   @param trigger  Receives the current logic level of trigger.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_test_signals_read(uint8_t address, uint8_t* clock, uint8_t* sync,
    uint8_t* trigger);

/**
*   @brief Write values to shared signals for testing.
*
*   This function puts the shared signals into test mode and sets them to the
*   specified state. The signal levels can then be read on the Pi GPIO pins to
*   confirm.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param mode     Set to 1 for test mode, 0 to return to normal operation.
*   @param clock    The value to write to the ADC clock when in test mode.
*   @param sync     The value to write to sync when in test mode.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc172_test_signals_write(uint8_t address, uint8_t mode, uint8_t clock,
    uint8_t sync);

#ifdef __cplusplus
}
#endif

#endif
