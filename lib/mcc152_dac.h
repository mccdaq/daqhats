/*
*   mcc152_dac.h
*   Measurement Computing Corp.
*   This file contains functions used with the DAC on the MCC 152.
*
*   07/18/2018
*/
#ifndef _MCC152_DAC_H
#define _MCC152_DAC_H

#include <stdint.h>

// Write to a single analog output channel.
int _mcc152_dac_write(uint8_t device, uint8_t address, uint8_t channel, 
    uint16_t code);
// Write to both channels at once.
int _mcc152_dac_write_both(uint8_t device, uint8_t address, uint16_t code0, 
    uint16_t code1);
// Initialize the SPI interface and DAC.
int _mcc152_dac_init(uint8_t device, uint8_t address);

#endif