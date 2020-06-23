#include <stdio.h>
#include <stdlib.h>
#include <daqhats/daqhats.h>

int main(int argc, char* argv[])
{
    int info_count;
    int index;
    struct HatInfo* info_list;
    double value;
    uint16_t version;
    uint16_t boot_version;
    uint8_t address;

    // get device list
    info_count = hat_list(HAT_ID_ANY, NULL);

    if (info_count ==  0)
    {
        printf("0 boards found\n");
        return 1;
    }

    info_list = (struct HatInfo*)malloc(info_count * sizeof(struct HatInfo));
    hat_list(HAT_ID_ANY, info_list);

    // display the list
    printf("Found %d board(s):\n\n", info_count);

    for (index = 0; index < info_count; index++)
    {
        address = info_list[index].address;
        printf("Address: %d\n", address);
        switch (info_list[index].id)
        {
        case HAT_ID_MCC_118:
            printf("Type: MCC 118\n");
            break;
        case HAT_ID_MCC_128:
            printf("Type: MCC 128\n");
            break;
        case HAT_ID_MCC_134:
            printf("Type: MCC 134\n");
            break;
        case HAT_ID_MCC_152:
            printf("Type: MCC 152\n");
            break;
        case HAT_ID_MCC_172:
            printf("Type: MCC 172\n");
            break;
        default:
            printf("Type: Unknown\n");
            break;
        }
        printf("Hardware version: %d\n", info_list[index].version);
        printf("Name: %s\n", info_list[index].product_name);

        if (info_list[index].id == HAT_ID_MCC_118)
        {
            if (mcc118_open(address) == RESULT_SUCCESS)
            {
                if (mcc118_firmware_version(address, &version, &boot_version)
                    == RESULT_SUCCESS)
                {
                    printf("Firmware version:   %X.%02X\n", (uint8_t)(version >> 8), (uint8_t)version);
                    printf("Bootloader version: %X.%02X\n", (uint8_t)(boot_version >> 8), (uint8_t)boot_version);
                }
                else
                {
                    printf("Can't read firmware version\n");
                }
                mcc118_close(address);
            }
            else
            {
                printf("Can't open device\n");
            }
        }
        else if (info_list[index].id == HAT_ID_MCC_128)
        {
            if (mcc128_open(address) == RESULT_SUCCESS)
            {
                if (mcc128_firmware_version(address, &version) == RESULT_SUCCESS)
                {
                    printf("Firmware version:   %X.%02X\n", (uint8_t)(version >> 8), (uint8_t)version);
                }
                else
                {
                    printf("Can't read firmware version\n");
                }
                mcc128_close(address);
            }
            else
            {
                printf("Can't open device\n");
            }
        }
        else if (info_list[index].id == HAT_ID_MCC_172)
        {
            if (mcc172_open(address) == RESULT_SUCCESS)
            {
                if (mcc172_firmware_version(address, &version) == RESULT_SUCCESS)
                {
                    printf("Firmware version:   %X.%02X\n", (uint8_t)(version >> 8), (uint8_t)version);
                }
                else
                {
                    printf("Can't read firmware version\n");
                }
                mcc172_close(address);
            }
            else
            {
                printf("Can't open device\n");
            }
        }

        if (index < (info_count - 1))
        {
            printf("\n");
        }
    }

    free(info_list);
    return 0;
}
