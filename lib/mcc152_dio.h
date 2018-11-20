/*
*   mcc152_dio.h
*   Measurement Computing Corp.
*   This file contains functions used with the I/O expander on the MCC 152.
*
*   07/18/2018
*/
#ifndef _MCC152_CJC_H
#define _MCC152_CJC_H

#include <stdint.h>
#include <stdbool.h>

#define DIO_CHANNEL_ALL             0xFF

#define DIO_REG_INPUT_PORT          0x00
#define DIO_REG_OUTPUT_PORT         0x01
#define DIO_REG_POLARITY            0x02
#define DIO_REG_CONFIG              0x03
#define DIO_REG_OUTPUT_STRENGTH_0   0x40
#define DIO_REG_OUTPUT_STRENGTH_1   0x41
#define DIO_REG_INPUT_LATCH         0x42
#define DIO_REG_PULL_ENABLE         0x43
#define DIO_REG_PULL_SELECT         0x44
#define DIO_REG_INT_MASK            0x45
#define DIO_REG_INT_STATUS          0x46
#define DIO_REG_OUTPUT_CONFIG       0x4F

// Initialize the DIO interface.
int _mcc152_dio_init(int address);
// Read a DIO register.
int _mcc152_dio_reg_read(uint8_t address, uint8_t reg, uint8_t channel,
    uint8_t* value);
// Write a DIO register.
int _mcc152_dio_reg_write(uint8_t address, uint8_t reg, uint8_t channel,
    uint8_t value, bool use_cache);

#endif