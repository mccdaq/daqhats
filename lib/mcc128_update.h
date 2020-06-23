/*
*   file mcc128_update.h
*   author Measurement Computing Corp.
*   brief This file contains function definitions for MCC 128 firmware updating.
*
*   06/12/2020
*/
#ifndef _MCC128_UPDATE_H
#define _MCC128_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The following functions are used for firmware updates and are not part of the
   user documentation */
int mcc128_open_for_update(uint8_t address);
int mcc128_enter_bootloader(uint8_t address);
int mcc128_bl_transfer(uint8_t address, void* tx_data, void* rx_data,
    uint16_t length);
int mcc128_bl_ready(void);

#ifdef __cplusplus
}
#endif

#endif
