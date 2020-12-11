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
#include "mcc128.h"
#include "mcc134.h"
#include "mcc152.h"
#include "mcc172.h"

/// Known DAQ HAT IDs.
enum HatIDs
{
    /// Match any DAQ HAT ID in [hat_list()](@ref hat_list)
    HAT_ID_ANY = 0,
    /// MCC 118 ID
    HAT_ID_MCC_118 = 0x0142,
    /// MCC 118 in firmware update mode ID
    HAT_ID_MCC_118_BOOTLOADER = 0x8142,
    /// MCC 128 ID
    HAT_ID_MCC_128 = 0x0146,
    /// MCC 134 ID
    HAT_ID_MCC_134 = 0x0143,
    /// MCC 152 ID
    HAT_ID_MCC_152 = 0x0144,
    /// MCC 172 ID
    HAT_ID_MCC_172 = 0x0145    
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
    /// Could not communicate with the device.
    RESULT_COMMS_FAILURE       = -7,
    /// Some other error occurred.
    RESULT_UNDEFINED           = -10
};

// Other definitions

/// The maximum number of DAQ HATs that may be connected.
#define MAX_NUMBER_HATS         8   

// Scan / read / write flags
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

/// Scan trigger input modes.
enum TriggerMode
{
    /// Start the scan on a rising edge of TRIG.
    TRIG_RISING_EDGE    = 0,
    /// Start the scan on a falling edge of TRIG.
    TRIG_FALLING_EDGE   = 1,
    /// Start the scan any time TRIG is high.
    TRIG_ACTIVE_HIGH    = 2,
    /// Start the scan any time TRIG is low.
    TRIG_ACTIVE_LOW     = 3
};

// Scan status bits

/// A hardware overrun occurred.
#define STATUS_HW_OVERRUN       (0x0001)
/// A scan buffer overrun occurred.
#define STATUS_BUFFER_OVERRUN   (0x0002)
/// The trigger event occurred.
#define STATUS_TRIGGERED        (0x0004)
/// The scan is running (actively acquiring data.)
#define STATUS_RUNNING          (0x0008)

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

/**
*   Read the current interrupt status.
*
*   It returns the status of the interrupt signal.  This signal can be shared by
*   multiple boards so the status of each board that may generate must be read
*   and the interrupt source(s) cleared before the interrupt will become
*   inactive.
*
*   This function only applies when using devices that can generate an
*   interrupt:
*       - MCC 152
*
*   @return 1 if interrupt is active, 0 if inactive.
*/
int hat_interrupt_state(void);

/**
*   Wait for an interrupt to occur.
*
*   It waits for the interrupt signal to become active, with a timeout
*   parameter.
*   
*   This function only applies when using devices that can generate an
*   interrupt:
*       - MCC 152
*
*   @param timeout  Wait timeout in milliseconds. -1 to wait forever, 0 to
*       return immediately.
*   @return [RESULT_TIMEOUT](@ref RESULT_TIMEOUT),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS), or 
*       [RESULT_UNDEFINED](@ref RESULT_UNDEFINED).
*/
int hat_wait_for_interrupt(int timeout);

/**
*   Enable an interrupt callback function.
*
*   Set a function that will be called when a DAQ HAT interrupt occurs. The 
*   function must have a void return type and void * argument, such as:
*
*       void function(void* user_data)
*
*   The function will be called when the DAQ HAT interrupt signal becomes
*   active, and cannot be called again until the interrupt signal becomes
*   inactive. Active sources become inactive when manually cleared (such as
*   reading the digital I/O inputs or clearing the interrupt enable.) If not
*   latched, an active source also becomes inactive when the value returns to
*   the original value (the value at the source before the interrupt was
*   generated.)
*
*   The user_data argument can be used for passing a reference to anything 
*   needed by the callback function. It will be passed to the callback function
*   when the interrupt occurs. Set it to NULL if not needed.

*   There may only be one callback function at a time; if you call this
*   when a function is already set as the callback function then it will be
*   replaced with the new function and the old function will no longer be called
*   if an interrupt occurs.
*
*   The callback function may be disabled with hat_interrupt_callback_disable().
*
*   This function only applies when using devices that can generate an
*   interrupt:
*       - MCC 152
*
*   @param function     The callback function.
*   @param user_data    The data to pass to the callback function.
*   @return [RESULT_SUCCESS](@ref RESULT_SUCCESS) or 
*       [RESULT_UNDEFINED](@ref RESULT_UNDEFINED).
*/
int hat_interrupt_callback_enable(void (*function)(void*), void* user_data);

/**
*   Disable interrupt callbacks.
*
*   Removes any callback function from the interrupt handler.
*
*   @return [RESULT_SUCCESS](@ref RESULT_SUCCESS) or
*       [RESULT_UNDEFINED](@ref RESULT_UNDEFINED).
*/
int hat_interrupt_callback_disable(void);

#ifdef __cplusplus
}
#endif

#endif
