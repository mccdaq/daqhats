#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/spi/spidev.h>
#include <termios.h>
#include "gpio.h"
#include "util.h"
#include "daqhats.h"
#include "mcc128_update.h"

// allocated at run time to contain the entire file
uint8_t* frame_file_buffer;
uint32_t frame_file_size;
uint32_t frame_file_index;

void print_usage(void)
{
    printf("Usage: mcc128_firmware_update <address> <file>\n");
    printf("  address: the board address (0-7)\n");
    printf("  file: the name of the firmware file\n");
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

// Return a pointer to the next frame in the frame buffer and the size of the
// frame.  frame_file_index must be set to 0 before first use.
int get_next_frame(uint8_t** frame_ptr, bool* last_frame)
{
    uint32_t index;
    uint8_t* ptr;
    uint16_t len;

    index = frame_file_index;
    ptr = &frame_file_buffer[index];

    if (index >= frame_file_size)
    {
        // reached end of file
        return 0;
    }

    // verify signature
    if ((ptr[2] != 0x13) || (ptr[3] != 0x02) ||
        (ptr[4] != 0xFD) || (ptr[5] != 0x67))
    {
        // invalid signature
        return -1;
    }

    // length
    len = ptr[1] + ((uint16_t)ptr[0] << 8);

    if (frame_ptr)
    {
        *frame_ptr = ptr;
    }

    frame_file_index += (len + 2);

    if (last_frame)
    {
        if (frame_file_index >= frame_file_size)
        {
            *last_frame = true;
        }
        else
        {
            *last_frame = false;
        }
    }
    return len + 2;
}

int update_firmware(int address, char* filename)
{
    int ret;
    uint8_t tx_data[256];
    uint8_t rx_data[256];
//    int count;
    int i;
    FILE* binfile;
    bool finished;
    bool first_read;
    bool error;
    struct spi_ioc_transfer tr;
    uint8_t* ptr;
    int length;
    int tr_len;
    int count;
    bool last_frame;
    struct stat st;
    char c;
    uint16_t fw_version;

    if ((stat(filename, &st) == -1) ||
        (st.st_size == 0) ||
        ((binfile = fopen(filename, "rb")) == NULL))
    {
        printf("Error opening %s.\n", filename);
        return 1;
    }

    frame_file_size = st.st_size;
    frame_file_buffer = (uint8_t*)calloc(1, frame_file_size);

    if (fread(frame_file_buffer, 1, frame_file_size, binfile) != frame_file_size)
    {
        printf("Error reading %s.", filename);
        free(frame_file_buffer);
        fclose(binfile);
        return 1;
    }

    fclose(binfile);
    frame_file_index = 0;

    ret = mcc128_open(address);
    if (ret == RESULT_SUCCESS)
    {
        if (mcc128_firmware_version(address, &fw_version) == RESULT_SUCCESS)
        {
            printf("Checking existing version...\n");

            printf("Device firmware version %X.%02X\n",
                (uint8_t)(fw_version >> 8), (uint8_t)fw_version);
        }
    }
    else if (ret == RESULT_INVALID_DEVICE)
    {
        printf("The device at address %d is not an MCC 128.\n", address);
        free(frame_file_buffer);
        return 1;
    }
    else
    {
        ret = mcc128_open_for_update(address);
        if (ret == RESULT_SUCCESS)
        {
            printf("The device at address %d cannot be confirmed as an MCC 128.\n",
                address);
        }
        else
        {
            printf("Unable to update the device: %d\n", ret);
            free(frame_file_buffer);
            return 1;
        }
    }
    printf("Do you want to continue? Press Y to continue, any other key to "
        "exit. > ");

    c = kbhit();
    printf("\n");

    if (!((c == 'y') || (c == 'Y')))
    {
        printf("Exiting\n");
        free(frame_file_buffer);
        mcc128_close(address);
        return 1;
    }

    printf("Updating...\n");

    mcc128_enter_bootloader(address);

    finished = false;
    error = false;
    first_read = true;

    while (!finished && !error)
    {
         // wait for bootloader to be ready
        count = 0;
        while (!mcc128_bl_ready() &&
               (count < 1000*10))
        {
            usleep(100);
            count++;
        }

        if (!mcc128_bl_ready())
        {
            printf("Timeout waiting for NCHG\n");
            finished = true;
            continue;
        }

        // read the status
        tx_data[0] = 0xFF;
        if (first_read)
        {
            tr_len = 3;
            tx_data[1] = 0xFF;
            tx_data[2] = 0xFF;
        }
        else
        {
            tr_len = 1;
        }

        if (mcc128_bl_transfer(address, tx_data, rx_data, tr_len) != RESULT_SUCCESS)
        {
            printf("Error: ioctl failed\n");
        }

        if (first_read)
        {
            // check the ID
            if ((rx_data[1] != 0x25) &&
                (rx_data[1] != 0xC6))
            {
                printf("\nUnexpected device ID 0x%02X\n", rx_data[1]);
                error = true;
            }
            first_read = false;
        }

        switch (rx_data[0] & 0xC0)
        {
        case 0xC0:  // WAITING_BOOTLOAD_CMD
            //printf("Status: WAITING_BOOTLOAD_CMD %02X, sending unlock\n", rx_data[0]);
            // unlock the device
            tx_data[0] = 0xDC;
            tx_data[1] = 0xAA;
            tr_len = 2;
            if (mcc128_bl_transfer(address, tx_data, NULL, tr_len) != RESULT_SUCCESS)
            {
                printf("Error: ioctl failed\n");
            }
            break;
        case 0x80:  // WAITING_FRAME_DATA
            //printf("Status: WAITING_FRAME_DATA %02X, ", rx_data[0]);
            length = get_next_frame(&ptr, &last_frame);
            if (length == -1)
            {
                printf("invalid frame signature\n");
                error = true;
            }
            else if (length == 0)
            {
                printf("data file complete\n");
                finished = true;
            }
            else
            {
                //printf("sending next frame\n");
                if (mcc128_bl_transfer(address, ptr, NULL, length) != RESULT_SUCCESS)
                {
                    printf("Error: ioctl failed\n");
                }
            }
            break;
        case 0x40:  // APP_CRC_FAIL
            printf("Status: APP_CRC_FAIL %02X, sending unlock\n", rx_data[0]);
            // unlock the device
            tx_data[0] = 0xDC;
            tx_data[1] = 0xAA;
            tr.tx_buf = (uintptr_t)tx_data;
            tr.rx_buf = (uintptr_t)NULL;
            tr_len = 2;
            if (mcc128_bl_transfer(address, tx_data, NULL, tr_len) != RESULT_SUCCESS)
            {
                printf("Error: ioctl failed\n");
            }
            break;
        case 0x00:  // others
            // check the status bits
            switch (rx_data[0])
            {
            case 0x02:  // FRAME_CRC_CHECK
                //printf("Status: FRAME_CRC_CHECK\n");
                break;
            case 0x03:  // FRAME_CRC_FAIL
                printf("Status: FRAME_CRC_FAIL\n");
                error = true;
                break;
            case 0x04:  // FRAME_CRC_PASS
                //printf("Status: FRAME_CRC_PASS\n");

                if (last_frame)
                {
                    finished = true;
                }
                break;
            case 0x06:  // ERROR_DETECTED
                printf("Status: ERROR_DETECTED\n");
                break;
            default:
                printf("Unexpected status %02X\n", rx_data[0]);
                error = true;
                break;
            }
            break;
        default:
            break;
        }

        usleep(2);
    }

    free(frame_file_buffer);
    mcc128_close(address);

    if (error)
    {
        return 1;
    }
    else
    {
        printf("Finished\n");
        return 0;
    }
}

int main(int argc, char* argv[])
{
    int address;
    int retry_count;
    bool success;
    char* filename;
    uint16_t fw_version;

    // validate arguments
    if (argc != 3)
    {
        print_usage();
        return 1;
    }

    if ((sscanf(argv[1], "%u", &address) != 1) ||
        (address > 7))
    {
        print_usage();
        return 1;
    }

    filename = argv[2];

    // update the firmware
    if (update_firmware(address, filename) == 1)
    {
        return 1;
    }

    // check the device
    printf("Checking device...\n");

    // wait for device to boot the new firmware
    retry_count = 0;
    success = false;
    do
    {
        sleep(1);

        if (mcc128_open(address) == RESULT_SUCCESS)
        {
            if (mcc128_firmware_version(address, &fw_version) == RESULT_SUCCESS)
            {
                success = true;
                printf("firmware version %X.%02X\n",
                    (uint8_t)(fw_version >> 8), (uint8_t)fw_version);
            }
        }
    } while ((retry_count < 5) && !success);

    if (!success)
    {
        printf("Error\n");
        return 1;
    }
    else
    {
        return 0;
    }
}
