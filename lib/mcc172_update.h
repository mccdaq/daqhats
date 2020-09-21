/*
*   file mcc172_update.h
*   author Measurement Computing Corp.
*   brief This file contains function definitions for MCC 172 firmware updating.
*
*   03/19/2019
*/
#ifndef _MCC172_UPDATE_H
#define _MCC172_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The following functions are used for firmware updates and are not part of the 
   user documentation */
int mcc172_open_for_update(uint8_t address);
int mcc172_enter_bootloader(uint8_t address);
int mcc172_bl_transfer(uint8_t address, void* tx_data, void* rx_data, 
    uint16_t length);
int mcc172_bl_ready(void);

#ifdef __cplusplus
}
#endif

#endif
