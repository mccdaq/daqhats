/*
*   file mcc118_update.h
*   author Measurement Computing Corp.
*   brief This file contains function definitions for MCC 118 firmware updating.
*
*   date 17 Jan 2018
*/
#ifndef _MCC118_UPDATE_H
#define _MCC118_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The following functions are used for firmware updates and are not part of the 
   user documentation */
int mcc118_reset(uint8_t address);
int mcc118_bootmem_write(uint8_t address, uint16_t mem_address, uint16_t count, 
    uint8_t* buffer);
int mcc118_bootmem_read(uint8_t address, uint16_t mem_address, uint16_t count, 
    uint8_t* buffer);

int mcc118_enter_bootloader(uint8_t address);
int mcc118_bl_erase(uint8_t address);
int mcc118_bl_write(uint8_t address, uint8_t* hex_record, uint8_t length);
int mcc118_bl_read_crc(uint8_t address, uint32_t mem_address, uint32_t count, 
    uint16_t* pCRC);
int mcc118_bl_jump(uint8_t address);

#ifdef __cplusplus
}
#endif

#endif
