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

typedef struct line_list
{
    unsigned int pin;
    struct gpiod_line* line;
    struct line_list* next;
} line_list;

static bool gpio_initialized = false;
static struct gpiod_chip* chip;
static line_list* lines = NULL;

// Variables for GPIO interrupt threads
#define NUM_GPIO            32          // max number of GPIO pins we handle for interrupts
static int gpio_int_thread_signal[NUM_GPIO] = {0};
static void* gpio_callback_data[NUM_GPIO] = {0};
static pthread_t gpio_int_threads[NUM_GPIO];
static bool gpio_threads_running[NUM_GPIO] = {0};
static void (*gpio_callback_functions[NUM_GPIO])(void*) = {0};

static line_list* _gpio_find_line(unsigned int pin)
{
    // Find the line for the requested pin
    line_list* ptr = lines;
    line_list* prev_ptr = lines;
    while (ptr)
    {
        if (ptr->pin == pin)
        {
            // found
            break;
        }

        prev_ptr = ptr;
        ptr = ptr->next;
    }

    if (!ptr)
    {
        // not found, so add the line
        ptr = malloc(sizeof(line_list));
        if (!ptr)
        {
            // error
            printf("_gpio_find_line: malloc error\n");
            return NULL;
        }
        ptr->pin = pin;
        ptr->line = gpiod_chip_get_line(chip, pin);
        if (!ptr->line)
        {
            // error
            printf("_gpio_find_line: gpiod_chip_get_line failed\n");
            free(ptr);
            return NULL;
        }
        ptr->next = NULL;
        // request ownership of the line
        if (-1 == gpiod_line_request_input(ptr->line, "daqhats"))
        {
            // error
            printf("_gpio_find_line: gpiod_line_request_input failed\n");
            free(ptr);
            return NULL;
        }
#ifdef DEBUG
        printf("_gpio_find_line added line %d %p\n", pin, ptr->line);
#endif
        if (!lines)
        {
            // this is the first pin
            lines = ptr;
        }
        else
        {
            prev_ptr->next = ptr;
        }
    }

    return ptr;
}

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

    gpio_initialized = true;
}

void gpio_close(void)
{
    if (!gpio_initialized)
    {
        return;
    }

    // release any lines
    line_list* ptr = lines;
    line_list* next;
    while (ptr)
    {
#ifdef DEBUG
        printf("gpio_close: ptr %p, line %p\n", ptr, ptr->line);
#endif
        next = ptr->next;
        gpiod_line_release(ptr->line);
        free(ptr);
        ptr = next;
    }

    lines = NULL;
#ifdef DEBUG
    printf("gpio_close: chip %p\n", chip);
#endif
    gpiod_chip_close(chip);
    chip = NULL;

    gpio_initialized = false;
}

void gpio_dir(unsigned int pin, unsigned int dir)
{
#ifdef DEBUG
    printf("gpio_dir %d %d\n", pin, dir);
#endif
    if (!gpio_initialized)
    {
        gpio_init();
    }

    // Find the line for the requested pin
    line_list* ptr = _gpio_find_line(pin);
    if (NULL == ptr)
    {
        // error
        printf("_gpio_find_line failed\n");
        return;
    }

    if (dir == 0)
    {
        // Set pin to output.
        if (-1 == gpiod_line_set_direction_output(ptr->line, 0))
        {
            printf("gpio_dir: gpiod_line_set_direction_output failed\n");
        }
    }
    else
    {
        // Set pin to input.
        if (-1 == gpiod_line_set_direction_input(ptr->line))
        {
            printf("gpio_dir: gpiod_line_set_direction_input failed\n");
        }
    }
}

void gpio_write(unsigned int pin, unsigned int val)
{
#ifdef DEBUG
    printf("gpio_write %d %d\n", pin, val != 0);
#endif
    if (!gpio_initialized)
    {
        gpio_init();
    }

    // Find the line for the requested pin
    line_list* ptr = _gpio_find_line(pin);
    if (NULL == ptr)
    {
        // error
        printf("_gpio_find_line failed\n");
        return;
    }

    // confirm that it is an output
    if (GPIOD_LINE_DIRECTION_INPUT == gpiod_line_direction(ptr->line))
    {
        // Set pin to output.
        if (-1 == gpiod_line_set_direction_output(ptr->line, 0))
        {
            printf("gpio_write: gpiod_line_set_direction_output failed\n");
        }
    }

    if (val == 0)
    {
        // Clear the pin to 0
        if (-1 == gpiod_line_set_value(ptr->line, 0))
        {
            printf("gpio_write: gpiod_line_set_value failed\n");
        }
    }
    else
    {
        // Set the pin to 1
        if (-1 == gpiod_line_set_value(ptr->line, 1))
        {
            printf("gpio_write: gpiod_line_set_value failed\n");
        }
    }
}

int gpio_status(unsigned int pin)
{
    if (!gpio_initialized)
    {
        gpio_init();
    }

    // Find the line for the requested pin
    line_list* ptr = _gpio_find_line(pin);
    if (NULL == ptr)
    {
        // error
        printf("_gpio_find_line failed\n");
        return -1;
    }

    // get the value
    int value = gpiod_line_get_value(ptr->line);
    if (-1 == value)
    {
        printf("gpio_status gpiod_line_get_value failed\n");
    }
    return value;
}

static void *gpio_interrupt_thread(void* arg)
{
    int pin = (int)(intptr_t)arg;
    // timeout every millisecond
    struct timespec timeout =
    {
        .tv_sec = 0,
        .tv_nsec = 1000000,
    };

    line_list* ptr = _gpio_find_line(pin);
    if (!ptr)
    {
        return NULL;
    }

    while (0 == gpio_int_thread_signal[pin])
    {
        int result = gpiod_line_event_wait(ptr->line, &timeout);
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

    // Find the line for the requested pin
    line_list* ptr = _gpio_find_line(pin);
    if (NULL == ptr)
    {
        // error
        printf("_gpio_find_line failed\n");
        return -1;
    }

    // temporarily release the line and request it with the desired event mode
    gpiod_line_release(ptr->line);
    int result;
    switch (mode)
    {
    case 0: // falling
        result = gpiod_line_request_falling_edge_events(ptr->line, "daqhats");
        break;
    case 1: // rising
        result = gpiod_line_request_rising_edge_events(ptr->line, "daqhats");
        break;
    case 2: // both
        result = gpiod_line_request_both_edges_events(ptr->line, "daqhats");
        break;
    default: // disable events
        result = gpiod_line_request_input(ptr->line, "daqhats");
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
    while (1 == gpiod_line_event_wait(ptr->line, &timeout))
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

    // Find the line for the requested pin
    line_list* ptr = _gpio_find_line(pin);
    if (NULL == ptr)
    {
        // error
        printf("_gpio_find_line failed\n");
        return -1;
    }

    // return if line is already low
    if (0 == gpiod_line_get_value(ptr->line))
    {
        return 1;
    }

    // wait for a falling edge

    // temporarily release the line and request it with the desired event mode
    gpiod_line_release(ptr->line);
    if (0 != gpiod_line_request_falling_edge_events(ptr->line, "daqhats"))
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
    while (1 == gpiod_line_event_wait(ptr->line, &timeout_struct))
        ;

    // wait for the next event
    if (timeout > 1000)
    {
        timeout_struct.tv_sec = timeout / 1000;
        timeout -= timeout_struct.tv_sec * 1000;
    }
    timeout_struct.tv_nsec = timeout * 1000000;

    int result = gpiod_line_event_wait(ptr->line, &timeout_struct);

    return result;
}
