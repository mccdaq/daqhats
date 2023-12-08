/*
*   file util.h
*   author Measurement Computing Corp.
*   brief This file contains utility definitions for MCC HATs.
*
*   date 17 Jan 2018
*/
#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <time.h>
#include "daqhats.h"

enum SpiBus
{
    SPI_BUS_0 = 0,
    SPI_BUS_1
};

#define MAX_SPI_TRANSFER        4096        // Defined in the spidev driver

// Delay / timeout constants
#define MSEC                    1000UL      // Milliseconds multiplier, for
                                            // functions that take a microsecond
                                            // argument
#define SEC                     1000*MSEC   // Seconds multiplier, for functions
                                            // that take a microsecond argument

/// \cond
// The Raspberry Pi SPI device driver names
#define SPI_DEVICE_0            "/dev/spidev0.0"
#define SPI_DEVICE_1            "/dev/spidev0.1"
/// \endcond

// The Raspberry Pi I2C device driver names
#define I2C_DEVICE_1            "/dev/i2c-1"

#ifdef __cplusplus
extern "C" {
#endif


// internal functions for use by board classes
int _obtain_lock(void);
int _obtain_board_lock(uint8_t address);
void _release_lock(int lock_fd);
void _release_board_lock(uint8_t address);

uint32_t _difftime_us(struct timespec* start, struct timespec* end);
uint32_t _difftime_ms(struct timespec* start, struct timespec* end);
void _address_init(void);
void _set_address(uint8_t address);
void _free_address(void);
int _hat_info(uint8_t address, struct HatInfo* pEntry, char* pData,
    uint16_t* pSize);

#ifdef __cplusplus
}
#endif

#endif
