/*
*   file gpio.c
*   author Measurement Computing Corp.
*   brief This file contains lightweight GPIO pin control functions.
*
*   date 17 Jan 2018
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "gpio.h"

// RPi 1 base address - use as default
#define PERIPH_BASE_1     0x20000000

#define PERIPH_SIZE       (4*1024)

#define GPIO_OFFSET       0x00200000

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or 
// SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= \
                            (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

static bool gpio_initialized = false;
static void* gpio_map = NULL;

static volatile unsigned* gpio;

static void gpio_init(void)
{
    uint8_t buffer[4];
    int fd;
    uint32_t gpio_base;

    gpio_initialized = false;

    // Determine which base address to use.

    // Try to open /proc/device-tree/soc/ranges to read the value from there
    fd = open("/proc/device-tree/soc/ranges", O_RDONLY);
    if (fd >= 0)
    {
        lseek(fd, 4, SEEK_SET);
        if (read(fd, buffer, 4) == 4)
        {
            gpio_base = ((uint32_t)buffer[0] << 24) |
                        ((uint32_t)buffer[1] << 16) |
                        ((uint32_t)buffer[2] << 8) |
                        ((uint32_t)buffer[3]);
            gpio_base += GPIO_OFFSET;
        }
        else
        {
            // use the RPi 1 value
            gpio_base = PERIPH_BASE_1 + GPIO_OFFSET;
        }
        close(fd);
    }
    else
    {
        // use the RPi 1 value
        gpio_base = PERIPH_BASE_1 + GPIO_OFFSET;
    }


    // Try to use /dev/mem in case we are not running in a recent version of
    // Raspbian.  Must be root for this to work.
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) >= 0)
    {
    }
    else
    {
        // Get access to the peripheral registers through /dev/gpiomem
        if ((fd = open("/dev/gpiomem", O_RDWR | O_SYNC)) < 0)
        {
            printf("Can't open /dev/gpiomem.\n");
            gpio = NULL;
            return;
        }
        gpio_base = 0;
    }

    gpio_map = mmap(
        NULL,                   // Any address in our space will do
        PERIPH_SIZE,            // Map length
        PROT_READ | PROT_WRITE, // Enable reading & writting to mapped memory
        MAP_SHARED,             // Shared with other processes
        fd,                     // File to map
        gpio_base               // Offset to GPIO peripheral
    );
    close(fd);

    if (gpio_map == MAP_FAILED)
    {
        printf("mmap failed %d\n", (int)gpio_map);
        gpio = NULL;
        return;
    }
    gpio = (volatile unsigned*)gpio_map;

    gpio_initialized = true;
}

void gpio_dir(int pin, int dir)
{
    if (!gpio_initialized)
    {
        gpio_init();
    }

    if (gpio)
    {
        if (dir == 0)
        {
            // Set pin to output.  Always set to input first.
            INP_GPIO(pin);
            OUT_GPIO(pin);
        }
        else
        {
            // Set pin to input.
            INP_GPIO(pin);
        }
    }
}

void gpio_write(int pin, int val)
{
    if (!gpio_initialized)
    {
        gpio_init();
    }

    if (gpio)
    {
        if (val == 0)
        {
            // Clear the pin to 0
            GPIO_CLR = 1 << pin;
        }
        else
        {
            // Set the pin to 1
            GPIO_SET = 1 << pin;
        }
    }
}
