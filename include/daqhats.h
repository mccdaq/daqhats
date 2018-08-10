/**
*   @file daqhats.h
*   @author Measurement Computing Corp.
*   @brief This file contains definitions used across all DAQ HATs.
*
*   @date 1 Feb 2018
*/
#ifndef _DAQHATS_H
#define _DAQHATS_H

// include files for DAQ HAT boards
#include "mcc118.h"

/// Known DAQ HAT IDs.
enum HatIDs
{
    /// Match any DAQ HAT ID in [hat_list()](@ref hat_list)
    HAT_ID_ANY = 0,
    /// MCC 118 ID
    HAT_ID_MCC_118 = 0x0142,
    /// MCC 118 in firmware update mode ID
    HAT_ID_MCC_118_BOOTLOADER = 0x8142
};

/// Return values from the library functions.
enum ResultCode
{
    /// Success, no errors
    RESULT_SUCCESS             = 0,
    /// A parameter passed to the function was incorrect.
    RESULT_BAD_PARAMETER       = -1,
    /// The device is busy.
    RESULT_BUSY                = -2,
    /// There was a timeout accessing a resource.
    RESULT_TIMEOUT             = -3,
    /// There was a timeout while obtaining a resource lock.
    RESULT_LOCK_TIMEOUT        = -4,
    /// The device at the specified address is not the correct type.
    RESULT_INVALID_DEVICE      = -5,
    /// A needed resource was not available.
    RESULT_RESOURCE_UNAVAIL    = -6,
    /// Some other error occurred.
    RESULT_UNDEFINED           = -10
};

// Other definitions

/// The maximum number of DAQ HATs that may be connected.
#define MAX_NUMBER_HATS         8   

// Scan / read flags
/// Default behavior.
#define OPTS_DEFAULT            (0x0000)
/// Read / write unscaled data.
#define OPTS_NOSCALEDATA        (0x0001)
/// Read / write uncalibrated data.
#define OPTS_NOCALIBRATEDATA    (0x0002)
/// Use an external clock source.
#define OPTS_EXTCLOCK           (0x0004)
/// Use an external trigger source.
#define OPTS_EXTTRIGGER         (0x0008)
/// Run until explicitly stopped.
#define OPTS_CONTINUOUS         (0x0010)

/// Contains information about a specific board.
struct HatInfo
{
    /// The board address.
    uint8_t address;
    /// The product ID, one of [HatIDs](@ref HatIDs)
    uint16_t id;
    /// The hardware version
    uint16_t version;
    /// The product name
    char product_name[256];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
*   Return a list of detected DAQ HAT boards.
*
*   It creates the list from the DAQ HAT EEPROM files that are currently on the
*   system. In the case of a single DAQ HAT at address 0 this information is
*   automatically provided by Raspbian. However, when you have a stack of
*   multiple boards you must extract the EEPROM images using the
*   \b daqhats_read_eeproms tool.
*
*   Example usage:
*   @code
*       int count = hat_list(HAT_ID_ANY, NULL);

*       if (count > 0)
*       {
*           struct HatInfo* list = (struct HatInfo*)malloc(count * 
*               sizeof(struct HatInfo));
*           hat_list(HAT_ID_ANY, list);
*
*           // perform actions with list
*
*           free(list);
*       }
*   @endcode
*
*   @param filter_id  An optional [ID](@ref HatIDs) filter to only return boards
*       with a specific ID. Use [HAT_ID_ANY](@ref HAT_ID_ANY) to return all
*       boards.
*   @param list    A pointer to a user-allocated array of struct HatInfo. The 
*       function will fill the structures with information about the detected 
*       boards. You may have an array of the maximum number of boards
*       ([MAX_NUMBER_HATS](@ref MAX_NUMBER_HATS)) or call this function while
*       passing NULL for list, which will return the count of boards found, then
*       allocate the correct amount of memory and call this function again with
*       a valid pointer.
*   @return The number of boards found.
*/
int hat_list(uint16_t filter_id, struct HatInfo* list);

/**
*   Return a text description for a DAQ HAT result code.
*
*   @param result  The [Result code](@ref ResultCode) returned from a DAQ HAT
*       function
*   @return The error message.
*/
const char* hat_error_message(int result);

#ifdef __cplusplus
}
#endif

#endif
