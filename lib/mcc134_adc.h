/*
*   mcc134_adc.h
*   Measurement Computing Corp.
*   This file contains functions used with the ADC on the MCC 134.
*
*   06/29/2018
*/
#ifndef _MCC134_ADC_H
#define _MCC134_ADC_H

#include <stdint.h>

int _mcc134_adc_init(uint8_t address);
int _mcc134_adc_read_tc_code(uint8_t address, uint8_t hi_input,
    uint8_t lo_input, uint32_t* code);
int _mcc134_adc_read_cjc_code(uint8_t address, uint8_t hi_input,
    uint8_t lo_input, uint32_t* code);

#endif