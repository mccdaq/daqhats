/**
*   @file mcc134.h
*   @author Measurement Computing Corp.
*   @brief This file contains definitions for the MCC 134.
*
*   11/13/2018
*/
#ifndef _MCC_134_H
#define _MCC_134_H

#include <stdint.h>

/// MCC 134 constant device information.
struct MCC134DeviceInfo
{
    /// The number of analog input channels (4.)
    const uint8_t NUM_AI_CHANNELS;
    /// The minimum ADC code (-8,388,608.)
    const int32_t AI_MIN_CODE;
    /// The maximum ADC code (8,388,607.)
    const int32_t AI_MAX_CODE;
    /// The input voltage corresponding to the minimum code (-0.078125V.)
    const double AI_MIN_VOLTAGE;
    /// The input voltage corresponding to the maximum code (+0.078125V - 1 LSB.)
    const double AI_MAX_VOLTAGE;
    /// The minimum voltage of the input range (-0.078125V.)
    const double AI_MIN_RANGE;
    /// The maximum voltage of the input range (0.078125V.)
    const double AI_MAX_RANGE;
};

// TC types from nist.c
/// Thermocouple type constants
enum TcTypes
{
    /// J type
    TC_TYPE_J   = 0,
    /// K type
    TC_TYPE_K   = 1,
    /// T type    
    TC_TYPE_T   = 2,
    /// E type    
    TC_TYPE_E   = 3,
    /// R type
    TC_TYPE_R   = 4,
    /// S type
    TC_TYPE_S   = 5,
    /// B type
    TC_TYPE_B   = 6,
    /// N type
    TC_TYPE_N   = 7,
    /// Input disabled
    TC_DISABLED = 0xFF
};

/// Return value for an open thermocouple.
#define OPEN_TC_VALUE           (-9999.0)
/// Return value for thermocouple voltage outside the valid range.
#define OVERRANGE_TC_VALUE      (-8888.0)
/// Return value for thermocouple voltage outside the common-mode range.
#define COMMON_MODE_TC_VALUE    (-7777.0)


#ifdef __cplusplus
extern "C" {
#endif

/**
*   @brief Open a connection to the MCC 134 device at the specified address.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_open(uint8_t address);

/**
*   @brief Check if an MCC 134 is open.
*
*   @param address  The board address (0 - 7).
*   @return 1 if open, 0 if not open.
*/
int mcc134_is_open(uint8_t address);

/**
*   @brief Close a connection to an MCC 134 device and free allocated resources.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_close(uint8_t address);

/**
*   @brief Return constant device information for all MCC 134s.
*
*   @return Pointer to struct MCC134DeviceInfo.
*       
*/
struct MCC134DeviceInfo* mcc134_info(void);

/**
*   @brief Read the MCC 134 serial number
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the serial
*       number as a string. The buffer must be at least 9 characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_serial(uint8_t address, char* buffer);

/**
*   @brief Read the MCC 134 calibration date
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the date as
*       a string (format "YYYY-MM-DD"). The buffer must be at least 11
*       characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_calibration_date(uint8_t address, char* buffer);

/**
*   @brief Read the MCC 134 calibration coefficients for a single channel.
*
*   The coefficients are applied in the library as:
*
*       calibrated_ADC_code = (raw_ADC_code * slope) + offset
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 3).
*   @param slope    Receives the slope.
*   @param offset   Receives the offset.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_calibration_coefficient_read(uint8_t address, uint8_t channel,
    double* slope, double* offset);

/**
*   @brief Temporarily write the MCC 134 calibration coefficients for a single
*       channel.
*
*   The user can apply their own calibration coefficients by writing to these
*   values. The values will reset to the factory values from the EEPROM whenever
*   mcc134_open() is called.
*
*   The coefficients are applied in the library as:
*
*       calibrated_ADC_code = (raw_ADC_code * slope) + offset
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The channel number (0 - 3).
*   @param slope    The new slope value.
*   @param offset   The new offset value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_calibration_coefficient_write(uint8_t address, uint8_t channel,
    double slope, double offset);

/**
*   @brief Write the thermocouple type for a channel.
*
*   Tells the MCC 134 library what thermocouple type is connected to the
*   specified channel and enables the channel. This is required for correct
*   temperature calculations. The type is one of [TcTypes](@ref TcTypes) and the
*   board will default to all disabled (set to [TC_DISABLED](@ref TC_DISABLED))
*   when it is first opened.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog input channel number (0 - 3).
*   @param type  The thermocouple type, one of [TcTypes](@ref TcTypes).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_tc_type_write(uint8_t address, uint8_t channel, uint8_t type);

/**
*   @brief Read the thermocouple type for a channel.
*
*   Reads the current thermocouple type for the specified channel. The type is
*   one of [TcTypes](@ref TcTypes) and the board will default to all channels
*   disabled (set to [TC_DISABLED](@ref TC_DISABLED)) when it is first opened.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog input channel number (0 - 3).
*   @param type  Receives the thermocouple type, one of [TcTypes](@ref TcTypes).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_tc_type_read(uint8_t address, uint8_t channel, uint8_t* type);

/**
*   @brief Write the temperature update interval.
*
*   Tells the MCC 134 library how often to update temperatures, with the
*   interval specified in seconds.  The library defaults to updating every
*   second, but you may increase this interval if you do not plan to call
*   mcc134_t_in_read() very often. This will reduce the load on shared resources
*   for other DAQ HATs.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param interval The interval in seconds (1 - 255).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_update_interval_write(uint8_t address, uint8_t interval);

/**
*   @brief Read the temperature update interval.
*
*   Reads the library temperature update rate in seconds.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param interval Receives the update rate.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_update_interval_read(uint8_t address, uint8_t* interval);

/**
*   @brief Read a temperature input channel.
*   
*   Reads the specified channel and returns the value as degrees Celsius. The
*   channel must be enabled with mcc134_tc_type_write() or the function will
*   return [RESULT_BAD_PARAMETER](@ref RESULT_BAD_PARAMETER).
*
*   This function returns immediately with the most recent internal temperature
*   reading for the specified channel. When a board is open, the library will
*   read each channel once per second. This interval can be increased with
*   mcc134_update_interval_write(). There will be a delay when the board is
*   first opened because the read thread has to read the cold junction
*   compensation sensors and thermocouple inputs before it can return the first
*   value.
*
*   The returned temperature can have some special values to indicate abnormal
*   conditions:
*       - [OPEN_TC_VALUE](@ref OPEN_TC_VALUE) if an open thermocouple is
*           detected on the channel.
*       - [OVERRANGE_TC_VALUE](@ref OVERRANGE_TC_VALUE) if an overrange is
*           detected on the channel.
*       - [COMMON_MODE_TC_VALUE](@ref COMMON_MODE_TC_VALUE) if a common-mode
*           error is detected on the channel. This occurs when thermocouples
*           attached to the board are at different voltages.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog input channel number (0 - 3.)
*   @param value    Receives the temperature value in degrees C.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_t_in_read(uint8_t address, uint8_t channel, double* value);

/**
*   @brief Read an analog input channel and return the value.
*
*   This function returns immediately with the most recent voltage or ADC code
*   reading for the specified channel. The channel must be enabled with
*   mcc134_tc_type_write() or the function will return
*   [RESULT_BAD_PARAMETER](@ref RESULT_BAD_PARAMETER).
*
*   The library reads the ADC at the time interval set with
*   mcc134_update_interval_write(), which defaults to 1 second. This interval
*   may be increased with mcc134_update_interval_write().
*
*   The returned voltage can have a special value to indicate abnormal
*   conditions:
*       - [COMMON_MODE_TC_VALUE](@ref COMMON_MODE_TC_VALUE) if a common-mode
*           error is detected on the channel. This occurs when thermocouples
*           attached to the board are at different voltages.
*
*   The valid options are:
*       - [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA): Return ADC code (a value 
*           between -8,388,608 and 8,388,607) rather than voltage.
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
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog input channel number (0 - 3).
*   @param options  Options bitmask.
*   @param value    Receives the analog input value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_a_in_read(uint8_t address, uint8_t channel, uint32_t options,
    double* value);

/**
*   @brief Read the cold junction compensation temperature for a specified
*   channel.
*   
*   Returns the most recent cold junction sensor temperature for the specified
*   thermocouple terminal. The library automatically performs cold junction
*   compensation, so this function is only needed for informational use or if
*   you want to perform your own compensation. The temperature is returned in
*   degress C.
*
*   The library reads the cold junction compensation sensors at the time
*   interval set with mcc134_update_interval_write(), which defaults to 1
*   second. This interval may be increased with mcc134_update_interval_write().
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog input channel number, 0 - 3.
*   @param value    Receives the read value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc134_cjc_read(uint8_t address, uint8_t channel, double* value);


#ifdef __cplusplus
}
#endif

#endif
