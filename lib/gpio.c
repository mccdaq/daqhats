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
#include <poll.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "bcm_host.h"
#include "gpio.h"

//#define DEBUG

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

#define GET_GPIO(g) ((*(gpio+13)&(1ul<<g)) == (1ul<<g)) // 0 if LOW, 1 if HIGH

static bool gpio_initialized = false;
static void* gpio_map = NULL;

static volatile unsigned* gpio;

// Variables for GPIO interrupt threads
#define NUM_GPIO            32          // max number of GPIO pins we handle for
                                        // interrupts
static int gpio_int_read_fds[NUM_GPIO] =
{
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1
};
static int gpio_int_thread_signal[NUM_GPIO] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};
static void* gpio_callback_data[NUM_GPIO] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static pthread_t gpio_int_threads[NUM_GPIO];
static void (*gpio_callback_functions[NUM_GPIO])(void*);

static void gpio_init(void)
{
    //uint8_t buffer[4];
    int fd;
    uint32_t gpio_base;

    gpio_initialized = false;

    // Determine which base address to use.
    gpio_base = bcm_host_get_peripheral_address() + GPIO_OFFSET;

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

#ifdef DEBUG
    openlog("daqhats", LOG_CONS | LOG_NDELAY, LOG_USER);
#endif

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

int gpio_status(int pin)
{
    return GET_GPIO(pin);
}

static void *gpio_interrupt_thread(void* arg)
{
    struct pollfd poll_data;
    int ret;
    uint8_t c;
    int pin;

    pin = *(int*)arg;
    free(arg);
    if (pin >= NUM_GPIO)
    {
        return NULL;
    }

    while (gpio_int_thread_signal[pin] == 0)
    {
        poll_data.fd = gpio_int_read_fds[pin];
        poll_data.events = POLLPRI | POLLERR;
        poll_data.revents = 0;

        // timeout after 1ms
        ret = poll(&poll_data, 1, 1);

        if (ret > 0)
        {
            // read to clear the interrupt
            lseek(gpio_int_read_fds[pin], 0, SEEK_SET);
            read(gpio_int_read_fds[pin], &c, 1);

            // call the callback
            gpio_callback_functions[pin](gpio_callback_data[pin]);
        }
    }

    return NULL;
}

int gpio_interrupt_callback(int pin, int mode, void (*function)(void*),
    void* data)
{
    int event_fd;
    int value_fd;
    char basename[30];
    char event_filename[64];
    char value_filename[64];
    char buffer[32];
    char mode_string[32];
    int count;
    int i;
    struct stat sb;

    if (pin >= NUM_GPIO)
    {
#ifdef DEBUG
        syslog(LOG_ERR, "gpio error 1");
#endif
        return -1;
    }

    // make sure gpio has been exported
    sprintf(basename, "/sys/class/gpio/gpio%d", (uint8_t)pin);
    sprintf(event_filename, "%s/edge", basename);
    if (stat(basename, &sb) != 0)
    {
        // export the interrupt pin
        int fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd == -1)
        {
#ifdef DEBUG
            syslog(LOG_ERR, "gpio error 2");
#endif
            return -1;
        }
        sprintf(buffer, "%d", pin);
        write(fd, buffer, strlen(buffer));
        close(fd);

        // wait for the edge file to appear with group write access
        do
        {
            usleep(10);
        } while (!( (stat(event_filename, &sb) == 0) &&     // file exists
                    ((sb.st_mode & S_IWGRP) == S_IWGRP) )); // group writable
    }

    // make sure edge is set
    event_fd = open(event_filename, O_RDWR);
    if (event_fd == -1)
    {
#ifdef DEBUG
        syslog(LOG_ERR, "gpio error 3");
#endif
        return -1;
    }
    read(event_fd, buffer, 32);

    switch (mode)
    {
    case 0: // falling
        sprintf(mode_string, "falling");
        break;
    case 1: // rising
        sprintf(mode_string, "rising");
        break;
    case 2: // both
        sprintf(mode_string, "both");
        break;
    default:    // disable
        sprintf(mode_string, "none");
        lseek(event_fd, 0, SEEK_SET);
        write(event_fd, mode_string, strlen(mode_string));
        close(event_fd);

        if (gpio_int_read_fds[pin] != -1)
        {
            // there is already an interrupt thread on this pin so signal the
            // thread to end and wait for it
            gpio_int_thread_signal[pin] = 1;
            pthread_join(gpio_int_threads[pin], NULL);
            close(gpio_int_read_fds[pin]);
            gpio_int_read_fds[pin] = -1;
        }
        return 0;
    }

    // only get here if we are configuring an edge
    if (strcmp(buffer, mode_string) != 0)
    {
        lseek(event_fd, 0, SEEK_SET);
        write(event_fd, mode_string, strlen(mode_string));
    }
    close(event_fd);

    // check for an existing thread
    if (gpio_int_read_fds[pin] != -1)
    {
        // there is already an interrupt thread on this pin so signal the
        // thread to end and wait for it
        gpio_int_thread_signal[pin] = 1;
        pthread_join(gpio_int_threads[pin], NULL);
        close(gpio_int_read_fds[pin]);
        gpio_int_read_fds[pin] = -1;
    }

    // clear any pending interrupts
    sprintf(value_filename, "%s/value", basename);
    value_fd = open(value_filename, O_RDONLY);
    if (value_fd == -1)
    {
#ifdef DEBUG
        syslog(LOG_ERR, "gpio error 4");
#endif
        return -1;
    }

    ioctl(value_fd, FIONREAD, &count);
    for (i = 0; i < count; i++)
    {
        read(value_fd, buffer, 1);
    }

    // set the callback function
    gpio_callback_functions[pin] = function;
    gpio_callback_data[pin] = data;
    gpio_int_read_fds[pin] = value_fd;
    gpio_int_thread_signal[pin] = 0;
    int* ppin = (int*)malloc(sizeof(int));
    *ppin = pin;

    // start the interrupt thread
    pthread_create(&gpio_int_threads[pin], NULL, gpio_interrupt_thread, ppin);

    return 0;
}

int gpio_wait_for_low(int pin, int timeout)
{
    int event_fd;
    int value_fd;
    struct pollfd poll_data;
    char basename[30];
    char event_filename[64];
    char value_filename[64];
    char buffer[32];
    int ret;
    struct stat sb;

    // make sure gpio has been exported
    sprintf(basename, "/sys/class/gpio/gpio%d", (uint8_t)pin);
    sprintf(event_filename, "%s/edge", basename);
    if (stat(basename, &sb) != 0)
    {
        int fd = open("/sys/class/gpio/export", O_RDWR);
        if (fd == -1)
        {
            return -1;
        }
        sprintf(buffer, "%d", pin);
        write(fd, buffer, strlen(buffer));
        close(fd);

        // wait for the edge file to appear with group write access
        do
        {
            usleep(10);
        } while (!( (stat(event_filename, &sb) == 0) &&     // file exists
                    ((sb.st_mode & S_IWGRP) == S_IWGRP) )); // group writable
    }

    // return if it is already low
    sprintf(value_filename, "%s/value", basename);
    value_fd = open(value_filename, O_RDONLY);
    if (value_fd == -1)
    {
        return -1;
    }
    read(value_fd, buffer, 1);
    if (buffer[0] == '0')
    {
        return 1;
    }

    // make sure edge is set
    event_fd = open(event_filename, O_RDWR);
    if (event_fd == -1)
    {
        return -1;
    }
    read(event_fd, buffer, 32);
    if (strcmp(buffer, "falling") != 0)
    {
        lseek(event_fd, 0, SEEK_SET);
        sprintf(buffer, "falling");
        write(event_fd, buffer, strlen(buffer));
    }
    close(event_fd);

    poll_data.fd = value_fd;
    poll_data.events = POLLPRI | POLLERR;
    poll_data.revents = 0;

    read(value_fd, buffer, 1);

    ret = poll(&poll_data, 1, timeout);
    lseek(value_fd, 0, SEEK_SET);
    read(value_fd, buffer, 1);

    close(value_fd);
    if (ret == 0)
    {
        // timeout
        return 0;
    }
    else if (ret == -1)
    {
        // error
        return -1;
    }
    else
    {
        return 1;
    }
}
