/*
*   file gpio.c
*   author Measurement Computing Corp.
*   brief This file contains lightweight GPIO pin control functions.
*
*   date 17 Jan 2018
*/
#include <gpiod.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "gpio.h"

//#define DEBUG

const char* app_name = "daqhats";

static bool gpio_initialized = false;
static struct gpiod_chip* chip;
static struct gpiod_line_bulk lines;

// Variables for GPIO interrupt threads
#define NUM_GPIO            32          // max number of GPIO pins we handle for interrupts
static int gpio_int_thread_signal[NUM_GPIO] = {0};
static void* gpio_callback_data[NUM_GPIO] = {0};
static pthread_t gpio_int_threads[NUM_GPIO];
static bool gpio_threads_running[NUM_GPIO] = {0};
static void (*gpio_callback_functions[NUM_GPIO])(void*) = {0};

void gpio_init(void)
{
    if (gpio_initialized)
    {
        return;
    }

    // determine if this is running on a Pi 5
    if (NULL == (chip = gpiod_chip_open_by_name("gpiochip4")))
    {
        // could not open gpiochip4, so must be < Pi 5
        if (NULL == (chip = gpiod_chip_open_by_name("gpiochip0")))
        {
            // could not open gpiochip0 - error
            printf("gpio_init: could not open gpiochip\n");
            return;
        }
        else
        {
#ifdef DEBUG
            printf("gpio_init: found gpiochip0\n");
#endif
        }
    }
    else
    {
#ifdef DEBUG
        printf("gpio_init: found gpiochip4\n");
#endif
    }

#ifdef DEBUG
    printf("gpio_init: chip = %p\n", chip);
#endif

    // get all the GPIO lines for the chip
#if 0
    // This isn't compatible with older versions of libgpiod
    if (-1 == gpiod_chip_get_all_lines(chip, &lines))
    {
        printf("gpio_init: error getting lines\n");
        gpiod_chip_close(chip);
        chip = NULL;
        return;
    }
#else
    gpiod_line_bulk_init(&lines);
    unsigned int count = gpiod_chip_num_lines(chip);
    for (unsigned int i = 0; i < count; i++)
    {
        struct gpiod_line* line = gpiod_chip_get_line(chip, i);
        if (NULL == line)
        {
            printf("gpio_init: gpiod_chip_get_line returned NULL\n");
            gpiod_chip_close(chip);
            chip = NULL;
            return;
        }
        gpiod_line_bulk_add(&lines, line);
    }
#endif

    gpio_initialized = true;
}

void gpio_close(void)
{
    if (!gpio_initialized)
    {
        return;
    }

#ifdef DEBUG
    printf("gpio_close\n");
#endif

    // release any lines
    for (unsigned int i = 0; i < lines.num_lines; i++)
    {
        gpiod_line_release(lines.lines[i]);
    }

    gpiod_chip_close(chip);
    chip = NULL;

    gpio_initialized = false;
}

void gpio_set_output(unsigned int pin, unsigned int value)
{
#ifdef DEBUG
    printf("gpio_set_output %d %d\n", pin, value);
#endif
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_set_output: pin %d invalid\n", pin);
        return;
    }

    // Set pin to output.
    if (-1 == gpiod_line_request_output(lines.lines[pin], app_name, value))
    {
        printf("gpio_set_output: gpiod_line_request_output failed\n");
    }
}

void gpio_write(unsigned int pin, unsigned int value)
{
#ifdef DEBUG
    printf("gpio_write %d %d\n", pin, value);
#endif
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_write: pin %d invalid\n", pin);
        return;
    }

    // Set pin value.
    if (-1 == gpiod_line_set_value(lines.lines[pin], value))
    {
        printf("gpio_write: gpiod_line_set_value failed\n");
    }
}

void gpio_input(unsigned int pin)
{
#ifdef DEBUG
    printf("gpio_input %d\n", pin);
#endif
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_input: pin %d invalid\n", pin);
        return;
    }

    // Set pin to input.
    if (-1 == gpiod_line_request_input(lines.lines[pin], app_name))
    {
        printf("gpio_input: gpiod_line_request_input failed\n");
    }
}

void gpio_release(unsigned int pin)
{
#ifdef DEBUG
    printf("gpio_release %d\n", pin);
#endif
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_release: pin %d invalid\n", pin);
        return;
    }

    // Release pin
    gpiod_line_release(lines.lines[pin]);
}

int gpio_read(unsigned int pin)
{
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_read: pin %d invalid\n", pin);
        return -1;
    }

    // get the value
    int value = gpiod_line_get_value(lines.lines[pin]);
    if (-1 == value)
    {
        printf("gpio_read gpiod_line_get_value failed\n");
    }
    return value;
}

static void *gpio_interrupt_thread(void* arg)
{
    unsigned int pin = (unsigned int)(intptr_t)arg;
    // timeout every millisecond
    struct timespec timeout =
    {
        .tv_sec = 0,
        .tv_nsec = 1000000,
    };
    if (pin >= lines.num_lines)
    {
        printf("gpio_interrupt_thread: pin %d invalid\n", pin);
        return NULL;
    }

    while (0 == gpio_int_thread_signal[pin])
    {
        int result = gpiod_line_event_wait(lines.lines[pin], &timeout);
        if (1 == result)
        {
            // call the callback
            gpio_callback_functions[pin](gpio_callback_data[pin]);
        }
    }

    return NULL;
}

int gpio_interrupt_callback(unsigned int pin, unsigned int mode, void (*function)(void*),
    void* data)
{
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_interrupt_callback: pin %d invalid\n", pin);
        return -1;
    }

    // temporarily release the line and request it with the desired event mode
    gpiod_line_release(lines.lines[pin]);
    int result;
    switch (mode)
    {
    case 0: // falling
        result = gpiod_line_request_falling_edge_events(lines.lines[pin], app_name);
        break;
    case 1: // rising
        result = gpiod_line_request_rising_edge_events(lines.lines[pin], app_name);
        break;
    case 2: // both
        result = gpiod_line_request_both_edges_events(lines.lines[pin], app_name);
        break;
    default: // disable events
        result = gpiod_line_request_input(lines.lines[pin], app_name);
        if (true == gpio_threads_running[pin])
        {
            // there is already an interrupt thread on this pin so signal the
            // thread to end and wait for it
            gpio_int_thread_signal[pin] = 1;
            pthread_join(gpio_int_threads[pin], NULL);
            gpio_threads_running[pin] = false;
        }
        return 0;
    }

    // only get here if we are configuring events

    if (0 != result)
    {
        // error
        printf("gpio_interrupt_callback: gpiod_line_request failed\n");
        return -1;
    }

    // check for an existing thread
    if (true == gpio_threads_running[pin])
    {
        // there is already an interrupt thread on this pin so signal the
        // thread to end and wait for it
        gpio_int_thread_signal[pin] = 1;
        pthread_join(gpio_int_threads[pin], NULL);
        gpio_threads_running[pin] = false;
    }

    // clear any pending interrupts
    struct timespec timeout =
    {
        .tv_sec = 0,
        .tv_nsec = 0,
    };
    while (1 == gpiod_line_event_wait(lines.lines[pin], &timeout))
        ;

    // set the callback function
    gpio_callback_functions[pin] = function;
    gpio_callback_data[pin] = data;
    gpio_int_thread_signal[pin] = 0;

    // start the interrupt thread
    if (0 == pthread_create(&gpio_int_threads[pin], NULL, gpio_interrupt_thread, (void*)(intptr_t)pin))
    {
        gpio_threads_running[pin] = true;
    }

    return 0;
}

int gpio_wait_for_low(unsigned int pin, unsigned int timeout)
{
    if (!gpio_initialized)
    {
        gpio_init();
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_wait_for_low: pin %d invalid\n", pin);
        return -1;
    }

    gpio_input(pin);

    // return if line is already low
    if (0 == gpio_read(pin))
    {
        gpio_release(pin);
        return 1;
    }

    // wait for a falling edge

    // temporarily release the line and request it with the desired event mode
    gpio_release(pin);

    // the line will not be available for any other processes / threads while this is waiting
    if (0 != gpiod_line_request_falling_edge_events(lines.lines[pin], app_name))
    {
        // error
        printf("gpio_wait_for_low: gpiod_line_request_falling_edge_events failed\n");
        return -1;
    }

    // clear any pending events
    struct timespec timeout_struct =
    {
        .tv_sec = 0,
        .tv_nsec = 0,
    };
    while (1 == gpiod_line_event_wait(lines.lines[pin], &timeout_struct))
        ;

    // wait for the next event
    if (timeout > 1000)
    {
        timeout_struct.tv_sec = timeout / 1000;
        timeout -= timeout_struct.tv_sec * 1000;
    }
    timeout_struct.tv_nsec = timeout * 1000000;

    int result = gpiod_line_event_wait(lines.lines[pin], &timeout_struct);
    gpio_release(pin);

    return result;
}
