#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "daqhats.h"
#include "mcc118_update.h"

// Flash address/size values are given in bytes
#define BYTES_PER_ADDR  2ul 

// Main firmware address
#define USER_START      (0x1800*BYTES_PER_ADDR)
#define USER_LENGTH     ((0xB000*BYTES_PER_ADDR) - USER_START)

// Config word addresses that require special handling
#define FSIGN_ADDR      (0xAF14*BYTES_PER_ADDR)

// Bootloader address
#define BOOT_START      (0x0800*BYTES_PER_ADDR)
#define BOOT_LENGTH     ((0x1800*BYTES_PER_ADDR) - BOOT_START)

#define USER_VERSION_ADDRESS (0x19FC*BYTES_PER_ADDR)
#define BOOT_VERSION_ADDRESS (0x17FC*BYTES_PER_ADDR)

#define TOTAL_LENGTH    (0xB000*BYTES_PER_ADDR)

uint8_t virtual_flash[TOTAL_LENGTH];

uint32_t ext_lin_address;
uint32_t ext_seg_address;

uint16_t hex_user_version;
uint16_t hex_boot_version;

void print_usage(void)
{
    // don't advertise bootloader update option
    printf("Usage: mcc118_firmware_update <address> <hex file>\n");
    printf("  address: the board address (0-7)\n");
    printf("  hex file: the name of the hex file containing the firmware\n");
}

uint16_t convert_hex_line(char* s, uint8_t* buffer, uint16_t max)
{
    int len;
    int count = 0;
    int index;
    int nibble;
    uint8_t temp;
    uint8_t ch;
    
    if (s[0] != ':')
    {
        return 0;
    }
    
    if (buffer == NULL)
    {
        return 0;
    }
    
    // remove cr or lf
    s[strcspn(s, "\r\n")] = 0;
    len = strlen(s);
    
    // convert the remaining stream to an array of bytes
    nibble = 0;
    for (index = 1; (index < len) && (count < max); index++)
    {
        ch = s[index];
        if (('0' <= ch) && (ch <= '9'))
        {
            temp = ch - '0';
        }
        else if (('a' <= ch) && (ch <= 'f'))
        {
            temp = ch - 'a' + 10;
        }
        else if (('A' <= ch) && (ch <= 'F'))
        {
            temp = ch - 'A' + 10;
        }
        else
        {
            // not a hex character
            return 0;
        }
        
        if (nibble == 0)
        {
            // high nibble
            buffer[count] = temp << 4;
            nibble = 1;
        }
        else
        {
            // low nibble
            buffer[count] |= temp;
            count++;
            nibble = 0;
        }
    }
    
    return count;
}

// Initialize the virtual flash before loading a hex file
void init_virtual_flash(void)
{
    uint32_t index;

    ext_lin_address = 0;
    ext_seg_address = 0;
    
    for (index = 0; index < TOTAL_LENGTH; index++)
    {
        if ((index+1) % 4)
        {
            virtual_flash[index] = 0xFF;
        }
        else
        {
            virtual_flash[index] = 0;
        }
    }

    // The FSIGN config word has a reserved bit and must stay at 0xFF7FFF
    // but will not be in hex file. Set that value in the virtual flash for 
    // correct CRC calculation.
    virtual_flash[FSIGN_ADDR + 1] = 0x7F;
}

uint16_t process_hex_line(char* string, uint8_t* buffer, uint16_t buflen,
    uint8_t* pType, uint32_t* pAddress)
{
    uint8_t rec_count;
    uint16_t hex_address;
    uint8_t rec_type;
    uint32_t full_address;
    uint16_t count;
    uint16_t index;
    uint8_t checksum;
    
    if (buffer == NULL)
    {
        return 0;
    }

    // convert the string to an array of bytes
    count = convert_hex_line(string, buffer, buflen);
    // a hex record has a minimum length of 5 bytes
    if (count < 5)
    {
        return 0;
    }

    // verify the checksum
    checksum = 0;
    for (index = 0; index < count; index++)
    {
        checksum += buffer[index];
    }
    if (checksum != 0)
    {
        return 0;
    }
    
    rec_count = buffer[0];
    hex_address = ((uint16_t)buffer[1] << 8) + buffer[2];
    rec_type = buffer[3];
    if (pType)
    {
        *pType = rec_type;
    }
    
    switch (rec_type)
    {
    case 0:
        // data, validate address
        full_address = hex_address + ext_lin_address + ext_seg_address;
        if (pAddress)
        {
            *pAddress = full_address;
        }
        // update the virtual flash
        memcpy(&virtual_flash[full_address], &buffer[4], rec_count);
        break;
    case 2:
        // extended segment address
        ext_seg_address = ((uint16_t)buffer[4] << 8) + buffer[5];
        ext_seg_address <<= 4;
        break;
    case 4:
        // extended linear address
        ext_lin_address = ((uint16_t)buffer[4] << 8) + buffer[5];
        ext_lin_address <<= 16;
        break;
    default:
        break;
    }
    
    return count;
}

bool verify_hex_file(FILE* file)
{
    // - check every line to make sure they are valid hex lines
    // - check the starting and ending addresses to make sure they are in range
    bool error;
    char string[256];
    uint8_t buffer[128];
    int count;
    uint8_t checksum;
    int index;
    uint8_t rec_count;
    uint16_t hex_address;
    uint32_t full_address;
    uint8_t rec_type;
    uint32_t lin_address;
    uint32_t seg_address;
    uint32_t max_address;
    uint32_t min_address;
    
    error = false;
    lin_address = 0;
    seg_address = 0;
    max_address = 0;
    min_address = 0xFFFFFFFF;
    
    rewind(file);
    while ((fgets(string, 256, file) != NULL) && !error)
    {
        // convert the string to an array of bytes
        count = convert_hex_line(string, buffer, 128);
        // a hex record has a minimum length of 5 bytes
        if (count < 5)
        {
            error = true;
            continue;
        }

        // verify the checksum
        checksum = 0;
        for (index = 0; index < count; index++)
        {
            checksum += buffer[index];
        }
        if (checksum != 0)
        {
            error = true;
            continue;
        }
    
        rec_count = buffer[0];
        hex_address = ((uint16_t)buffer[1] << 8) + buffer[2];
        rec_type = buffer[3];
    
        switch (rec_type)
        {
        case 0:
            // data, create full address and validate
            full_address = hex_address + lin_address + seg_address;
            if (full_address < min_address)
            {
                min_address = full_address;
            }
            if ((full_address + rec_count) > max_address)
            {
                max_address = (full_address + rec_count);
            }
            if (max_address >= TOTAL_LENGTH)
            {
                error = true;
                continue;
            }
            // check for the version locations
            if ((full_address <= USER_VERSION_ADDRESS) &&
                ((full_address + rec_count) >= (USER_VERSION_ADDRESS + 2)))
            {
                index = USER_VERSION_ADDRESS - full_address + 4;
                hex_user_version = buffer[index] | 
                    ((uint16_t)(buffer[index+1]) << 8);
            }
            if ((full_address <= BOOT_VERSION_ADDRESS) &&
                ((full_address + rec_count) >= (BOOT_VERSION_ADDRESS + 2)))
            {
                index = BOOT_VERSION_ADDRESS - full_address + 4;
                hex_boot_version = buffer[index] | 
                    ((uint16_t)(buffer[index+1]) << 8);
            }
            break;
        case 2:
            // extended segment address
            seg_address = ((uint16_t)buffer[4] << 8) + buffer[5];
            seg_address <<= 4;
            break;
        case 4:
            // extended linear address
            lin_address = ((uint16_t)buffer[4] << 8) + buffer[5];
            lin_address <<= 16;
            break;
        default:
            // ignore
            break;
        }
        
    }
    
    rewind(file);

    return !error;
}

static const uint16_t crc_table[16] = 
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef
};

// Calculate the CRC of an array of bytes
uint16_t calculate_crc(uint32_t len, uint8_t* buffer)
{
    uint16_t i;
    uint16_t crc = 0;
       
    while (len--)
    {
        i = (crc >> 12) ^ (*buffer >> 4);
        crc = crc_table[i & 0x0F] ^ (crc << 4);
        i = (crc >> 12) ^ (*buffer >> 0);
        crc = crc_table[i & 0x0F] ^ (crc << 4);
        
        buffer++;
	} 

    return crc;
}

char kbhit(void)
{
    struct termios info, old_info;
    char c;
    
    // set terminal to return after a single character
    tcgetattr(0, &info);
    memcpy(&old_info, &info, sizeof(info));
    info.c_lflag &= ~ICANON;
    info.c_cc[VMIN] = 1;
    info.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &info);
    
    c = getchar();
    
    // restore terminal
    tcsetattr(0, TCSANOW, &old_info);
    
    return c;
}

// Update the main firmware
int update_firmware(int address, char* filename)
{
    char string[256];
    uint8_t buffer[128];
    FILE* file;
    int fd;
    int count;
    uint8_t rec_type;
    uint16_t crc_calc;
    uint32_t flash_address;
    uint16_t crc;
    uint16_t fw_version;
    uint16_t boot_version;
    char c;

    init_virtual_flash();
    
    // open the hex file
    if ((file = fopen(filename, "r")) == NULL)
    {
        printf("Error opening %s.\n", filename);
        return 1;
    }
    
    // verify that this is a hex file and is appropriate for the MCC 118 micro
    if (!verify_hex_file(file))
    {
        printf("Error - hex file %s is not valid.\n", filename);
        fclose(file);
        return 1;
    }
    
    printf("Checking versions...\n");
    
    printf("Hex file firmware version %X.%02X\n",
        (uint8_t)(hex_user_version >> 8), (uint8_t)hex_user_version);
        
    if (mcc118_open(address) != RESULT_SUCCESS)
    {
        printf("Error opening the device at address %d.\n", address);
        fclose(file);
        return 1;
    }
    
    if (mcc118_firmware_version(address, &fw_version, &boot_version) !=
        RESULT_SUCCESS)
    {
        printf("Error getting the firmware version.\n");
        return 1;
    }
  
    printf("Device firmware version   %X.%02X\n", 
        (uint8_t)(fw_version >> 8), (uint8_t)fw_version);

    printf("Do you want to continue? Press Y to continue, any other key to "
        "exit. > ");
    
    c = kbhit();
    printf("\n");
    
    if (!((c == 'y') || (c == 'Y')))
    {
        printf("Exiting\n");
        fclose(file);
        mcc118_close(address);
        return 1;
    }
    
    if (mcc118_enter_bootloader(address) != RESULT_SUCCESS)
    {
        printf("Error entering the bootloader.\n");
        mcc118_close(address);
        fclose(file);
        return 1;
    }
        
    // erase the flash memory
    printf("Erasing...");
    if (mcc118_bl_erase(address) != RESULT_SUCCESS)
    {
        printf("Error\n");
        mcc118_close(address);
        fclose(file);
        return 1;
    }
    printf("done\n");
    
    // write the data from the hex file
    printf("Writing...");
    while (fgets(string, 256, file) != NULL)
    {
        if ((count = process_hex_line(string, buffer, 128, &rec_type,
            &flash_address)) == 0)
        {
            continue;
        }
        
        switch (rec_type)
        {
        case 0:
            // data, don't send records in the bootloader region
            if ((flash_address < USER_START) ||
                (flash_address >= (USER_START+USER_LENGTH)))
            {
                // skip line
                continue;
            }
            break;
        default:
            break;
        }
        
        // send the record to the device
        if (mcc118_bl_write(address, buffer, count) != RESULT_SUCCESS)
        {
            printf("error\n");
            fclose(file);
            mcc118_close(address);
            return 1;
        }
    }
    fclose(file);
    printf("done\n");
    
    // read the CRC and compare
    printf("Verifying...");
    if (mcc118_bl_read_crc(address, USER_START, USER_LENGTH, &crc) !=
        RESULT_SUCCESS)
    {
        printf("error\n");
        mcc118_close(address);
        return 1;
    }
    
    crc_calc = calculate_crc(USER_LENGTH, &virtual_flash[USER_START]);
    if (crc != crc_calc)
    {
        printf("CRC mismatch %04X vs %04X\n", crc_calc, crc);
        mcc118_close(address);
        return 1;
    }
    printf("done\n");
    
    // jump to new firmware
    printf("Starting firmware...");
    if (mcc118_bl_jump(address) != RESULT_SUCCESS)
    {
        printf("error\n");
        mcc118_close(address);
        return 1;
    }

    printf("done\n");
    
    mcc118_close(address);
    return 0;
}

int update_bootloader(int address, char* filename)
{
    FILE* file;
    char string[256];
    uint8_t buffer[256];
    uint32_t flash_address;
    int count;
    uint16_t fw_version;
    uint16_t boot_version;
    char c;

    printf("Updating bootloader - are you sure? Press Y to continue, any other "
        "key to exit > ");
    c = kbhit();
    printf("\n");
    
    if (!((c == 'y') || (c == 'Y')))
    {
        printf("Exiting\n");
        return 1;
    }
    
    init_virtual_flash();
    
    // open the hex file
    if ((file = fopen(filename, "r")) == NULL)
    {
        printf("Error opening %s.\n", filename);
        return 1;
    }
    
    // verify that this is a hex file and is appropriate for the MCC 118 micro
    if (!verify_hex_file(file))
    {
        printf("Error - hex file %s is not valid.\n", filename);
        fclose(file);
        return 1;
    }
    
    printf("Checking versions...\n");
    
    printf("Hex file bootloader version %X.%02X\n",
        (uint8_t)(hex_boot_version >> 8), (uint8_t)hex_boot_version);
    
    if (mcc118_open(address) != RESULT_SUCCESS)
    {
        printf("Error opening the device at address %d.\n", address);
        return 1;
    }
    
    if (mcc118_firmware_version(address, &fw_version, &boot_version) !=
        RESULT_SUCCESS)
    {
        printf("Error getting the firmware version.\n");
        return 1;
    }

    printf("Device bootloader version   %X.%02X\n", 
        (uint8_t)(boot_version >> 8), (uint8_t)boot_version);

    printf("Do you want to continue? Press Y to continue, any other key to "
        "exit. > ");

    c = kbhit();
    printf("\n");
    
    if (!((c == 'y') || (c == 'Y')))
    {
        printf("Exiting\n");
        fclose(file);
        mcc118_close(address);
        return 1;
    }

    // read the hex file into virtual flash
    while (fgets(string, 256, file) != NULL)
    {
        process_hex_line(string, buffer, 128, NULL, NULL);
    }
    fclose(file);
    
    // unlock the bootloader memory
    printf("Unlocking bootloader...");
    buffer[0] = 0x55;
    buffer[1] = 0xAA;
    if (mcc118_bootmem_write(address, 0xFFF0, 2, buffer) != RESULT_SUCCESS)
    {
        printf("error\n");
        mcc118_close(address);
        return 1;
    }
    printf("done\n");
    
    // erase the bootloader memory
    printf("Erasing bootloader...");
    buffer[0] = 0x55;
    buffer[1] = 0xAA;
    if (mcc118_bootmem_write(address, 0x8000, 2, buffer) != RESULT_SUCCESS)
    {
        printf("error\n");
        mcc118_close(address);
        return 1;
    }
    printf("done\n");
    
    // write the bootloader memory
    printf("Writing bootloader...");
    for (flash_address = BOOT_START; flash_address < USER_START;
        flash_address += 128)
    {
        if (mcc118_bootmem_write(address, flash_address, 128, 
            &virtual_flash[flash_address]) != RESULT_SUCCESS)
        {
            printf("error\n");
            mcc118_close(address);
            return 1;
        }
    }
    printf("done\n");
    
    printf("Verifying bootloader...");
    for (flash_address = BOOT_START; flash_address < USER_START; 
        flash_address += 256)
    {
        if (mcc118_bootmem_read(address, flash_address, 256, buffer) != 
            RESULT_SUCCESS)
        {
            printf("error\n");
            mcc118_close(address);
            return 1;
        }
        if (memcmp(buffer, &virtual_flash[flash_address], 256) != 0)
        {
            printf("mismatch %04X\n", flash_address);
            mcc118_close(address);
            return 1;
        }
    }
    printf("done\n");
    
    printf("Restarting...");
    if (mcc118_reset(address) != RESULT_SUCCESS)
    {
        printf("error\n");
        mcc118_close(address);
        return 1;
    }
    printf("done\n");
    
    mcc118_close(address);
    return 0;
}

int main(int argc, char* argv[])
{
    unsigned int address;
    uint8_t address_index;
    uint8_t hexfile_index;
    char filename[1024];
    bool write_bootloader;
    int result;
    uint16_t fw_version;
    uint16_t boot_version;
    
    // validate arguments
    if ((argc < 3) || (argc > 4))
    {
        print_usage();
        return 1;
    }
    
    if (argc == 4)
    {
        // look for -b option
        if (strcmp(argv[1], "-b") != 0)
        {
            print_usage();
            return 1;
        }
        write_bootloader = true;
        address_index = 2;
        hexfile_index = 3;
    }
    else
    {
        write_bootloader = false;
        address_index = 1;
        hexfile_index = 2;
    }
    
    if ((sscanf(argv[address_index], "%u", &address) != 1) ||
        (address >= MAX_NUMBER_HATS))
    {
        print_usage();
        return 1;
    }

    if (sscanf(argv[hexfile_index], "%s", filename) != 1)
    {
        print_usage();
        return 1;
    }
    
    if (write_bootloader)
    {
        result = update_bootloader(address, filename);
    }
    else
    {
        result = update_firmware(address, filename);
    }
    
    if (result == 1)
    {
        return 1;
    }

    printf("Checking device...");
    // wait for device to enter main firmware
    usleep(800000);
    
    if (mcc118_open(address) != RESULT_SUCCESS)
    {
        printf("error\n");
        return 1;
    }
    if (mcc118_firmware_version(address, &fw_version, &boot_version) !=
        RESULT_SUCCESS)
    {
        printf("error\n");
        return 1;
    }
    
    if (write_bootloader)
    {
        printf("bootloader version %X.%02X\n", 
            (uint8_t)(boot_version >> 8), (uint8_t)boot_version);
    }
    else
    {
        printf("firmware version %X.%02X\n", 
            (uint8_t)(fw_version >> 8), (uint8_t)fw_version);
    }
    return 0;
}
