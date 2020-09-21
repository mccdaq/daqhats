#include <stdio.h>
#include <stdlib.h>
#include <daqhats/daqhats.h>

// Utility to check for any MCC 152s at an address other than 0; used to enable
// the I2C bus if needed.
int main(int argc, char* argv[])
{
    int info_count;
    int index;
    struct HatInfo* info_list;
    uint8_t address;
    int result;

    // get device list
    info_count = hat_list(HAT_ID_ANY, NULL);

    if (info_count ==  0)
    {
        printf("0\n");
        return 0;
    }

    info_list = (struct HatInfo*)malloc(info_count * sizeof(struct HatInfo));
    hat_list(HAT_ID_ANY, info_list);

    result = 0;
    for (index = 0; index < info_count; index++)
    {
        address = info_list[index].address;
        switch (info_list[index].id)
        {
        case HAT_ID_MCC_152:
            if (address != 0)
            {
                result = 1;
            }
            break;
        default:
            break;
        }
    }

    free(info_list);
    printf("%d\n", result);
    return result;
}
