/*
*   file gpio_v2.c
*   author Measurement Computing Corp.
*   brief This file contains lightweight GPIO pin control functions for libgpio v2.
*
*   date 24 Nov 2025
*/
#include <gpiod.h>
#include <pthread.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "gpio.h"

//#define DEBUG

const char* app_name = "daqhats";

static bool gpio_initialized = false;
static struct gpiod_chip* chip = NULL;
static struct
{
    unsigned int num_lines;
    struct gpiod_line_request** requests;
} lines = {0};

// Variables for GPIO interrupt threads
#define NUM_GPIO            32          // max number of GPIO pins we handle for interrupts
static int gpio_int_thread_signal[NUM_GPIO] = {0};
static void* gpio_callback_data[NUM_GPIO] = {0};
static pthread_t gpio_int_threads[NUM_GPIO];
static bool gpio_threads_running[NUM_GPIO] = {0};
static void (*gpio_callback_functions[NUM_GPIO])(void*) = {0};

// libgpiod v2 helper functions
static int _request_output(unsigned int pin, unsigned int value)
{
    struct gpiod_request_config *req_cfg = NULL;
    struct gpiod_line_request *request = NULL;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    int ret = -1;

    if (!gpio_initialized || (pin >= lines.num_lines))
        return ret;

    // if we have an existing request then release it
    if (NULL != lines.requests[pin])
    {
        gpiod_line_request_release(lines.requests[pin]);
        lines.requests[pin] = NULL;
    }

    settings = gpiod_line_settings_new();
    if (!settings)
    {
        return ret;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, 
                                         (0 == value) ? 
                                         GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE);

    line_cfg = gpiod_line_config_new();
    if (!line_cfg)
        goto free_settings;

    ret = gpiod_line_config_add_line_settings(line_cfg, &pin, 1, settings);
    if (ret)
        goto free_line_config;

    req_cfg = gpiod_request_config_new();
    if (!req_cfg)
        goto free_line_config;

    gpiod_request_config_set_consumer(req_cfg, app_name);

    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    
    if (NULL != request)
    {
        ret = 0;
    }
    lines.requests[pin] = request;

free_line_config:
    gpiod_line_config_free(line_cfg);

free_settings:
    gpiod_line_settings_free(settings);

    return ret;
}

static int _write_output_value(unsigned int pin, unsigned int value)
{
    if (!gpio_initialized || (pin >= lines.num_lines))
        return -1;

    // if we don't have an existing request then try to set pin to output
    if (NULL == lines.requests[pin])
    {
        return _request_output(pin, value);
    }

    // write the value
    enum gpiod_line_value out_value = (0 == value) ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
    return gpiod_line_request_set_value(lines.requests[pin], pin, out_value);
}

static int _request_input(unsigned int pin, enum gpiod_line_edge edge)
{
    struct gpiod_request_config *req_cfg = NULL;
    struct gpiod_line_request *request = NULL;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    int ret = -1;

    if (!gpio_initialized || (pin >= lines.num_lines))
        return ret;

    // if we have an existing request then release it
    if (NULL != lines.requests[pin])
    {
        gpiod_line_request_release(lines.requests[pin]);
        lines.requests[pin] = NULL;
    }

    settings = gpiod_line_settings_new();
    if (!settings)
        return ret;

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, edge);
    
    line_cfg = gpiod_line_config_new();
    if (!line_cfg)
        goto free_settings;

    ret = gpiod_line_config_add_line_settings(line_cfg, &pin, 1, settings);
    if (ret)
        goto free_line_config;

    req_cfg = gpiod_request_config_new();
    if (!req_cfg)
        goto free_line_config;

    gpiod_request_config_set_consumer(req_cfg, app_name);

    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    
    if (NULL != request)
    {
        ret = 0;
    }
    lines.requests[pin] = request;

free_line_config:
    gpiod_line_config_free(line_cfg);

free_settings:
    gpiod_line_settings_free(settings);

    return ret;
}

static int _read_input_value(unsigned int pin)
{
    int ret = -1;

    if (!gpio_initialized || (pin >= lines.num_lines))
        return -1;

    // if we don't have an existing request then try to set pin to input
    if (NULL == lines.requests[pin])
    {
        if (-1 == _request_input(pin, GPIOD_LINE_EDGE_NONE))
        {
            return -1;
        }
    }

    // read the value
    enum gpiod_line_value value = gpiod_line_request_get_value(lines.requests[pin], pin);
    switch (value)
    {
    case GPIOD_LINE_VALUE_ACTIVE:
        ret = 1;
        break;
    case GPIOD_LINE_VALUE_INACTIVE:
        ret = 0;
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

void gpio_init(void)
{
    if (gpio_initialized)
    {
        return;
    }

    // determine if this is running on a Pi 5
    if (NULL == (chip = gpiod_chip_open("/dev/gpiochip4")))
    {
        // could not open gpiochip4, so must be < Pi 5
        if (NULL == (chip = gpiod_chip_open("/dev/gpiochip0")))
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

    // get the GPIO line info for the chip and create an array of request pointers
    struct gpiod_chip_info* info = gpiod_chip_get_info(chip);
    if (!info)
    {
        printf("gpio_init: failed to get chip info\n");
        gpiod_chip_close(chip);
        chip = NULL;
        return;
    }
    lines.num_lines = gpiod_chip_info_get_num_lines(info);
    gpiod_chip_info_free(info);
#ifdef DEBUG
    printf("gpio_init: %d lines\n", lines.num_lines);
#endif
    lines.requests = (struct gpiod_line_request**)calloc(lines.num_lines, sizeof(struct gpiod_line_request*));
    if (NULL == lines.requests)
    {
        printf("gpio_init: calloc failed\n");
        return;
    }
    
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
        gpio_release(i);
    }

    // free the requests array
    free(lines.requests);
    
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
    if (-1 == _request_output(pin, value))
    {
        printf("gpio_set_output: _request_output failed\n");
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

    // Set output value
    if (-1 == _write_output_value(pin, value))
    {
        printf("gpio_write: _request_output_value failed\n");
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
    if (-1 == _request_input(pin, GPIOD_LINE_EDGE_NONE))
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
    if (NULL != lines.requests[pin])
    {
        gpiod_line_request_release(lines.requests[pin]);
        lines.requests[pin] = NULL;
    }
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
    int value = _read_input_value(pin);
    if (-1 == value)
    {
        printf("gpio_read _read_input_value failed\n");
    }
    return value;
}

static void *gpio_interrupt_thread(void* arg)
{
    unsigned int pin = (unsigned int)(intptr_t)arg;

    if (!gpio_initialized)
    {
        return NULL;
    }
    if (pin >= lines.num_lines)
    {
        printf("gpio_interrupt_thread: pin %d invalid\n", pin);
        return NULL;
    }

    struct gpiod_edge_event_buffer *event_buffer = gpiod_edge_event_buffer_new(1);
    if (!event_buffer) 
    {
        return NULL;
    }

    struct pollfd pollfd;
    pollfd.fd = gpiod_line_request_get_fd(lines.requests[pin]);
    pollfd.events = POLLIN;

    while (0 == gpio_int_thread_signal[pin])
    {
        // timeout every millisecond to check for shutdown signals
        int ret = poll(&pollfd, 1, 1);
        if (-1 == ret) 
        {
            // error
            goto cleanup;
        }
        else if (0 == ret)
        {
            // timeout
            continue;
        }
        
        int count = gpiod_line_request_read_edge_events(lines.requests[pin], event_buffer, 1);
        if (-1 == count) 
        {
            goto cleanup;
        }
        // call the callback
        gpio_callback_functions[pin](gpio_callback_data[pin]);
    }
    
cleanup:    
    gpiod_edge_event_buffer_free(event_buffer);
    gpio_release(pin);
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
    
    // check for an existing thread
    if (true == gpio_threads_running[pin])
    {
        // there is already an interrupt thread on this pin so signal the
        // thread to end and wait for it
        gpio_int_thread_signal[pin] = 1;
        pthread_join(gpio_int_threads[pin], NULL);
        gpio_threads_running[pin] = false;
    }

    // temporarily release the line and request it with the desired event mode
    gpio_release(pin);
    int result;
    switch (mode)
    {
    case 0: // falling
        result = _request_input(pin, GPIOD_LINE_EDGE_FALLING);
        break;
    case 1: // rising
        result = _request_input(pin, GPIOD_LINE_EDGE_RISING);
        break;
    case 2: // both
        result = _request_input(pin, GPIOD_LINE_EDGE_BOTH);
        break;
    default: // disable events
        gpio_input(pin);
        return 0;
    }
    
    // only get here if we are configuring events

    if (0 != result)
    {
        // error
        printf("gpio_interrupt_callback: _request_input failed\n");
        return -1;
    }

    // clear any unread events
    struct gpiod_edge_event_buffer* buffer = gpiod_edge_event_buffer_new(1);
    
    int ret = 0;
    do
    {
        ret = gpiod_line_request_wait_edge_events(lines.requests[pin], 0);
        if (1 == ret)
        {
            gpiod_line_request_read_edge_events(lines.requests[pin], buffer, 1);
        }
    } while (1 == ret);
    gpiod_edge_event_buffer_free(buffer);

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

    if (-1 == _request_input(pin, GPIOD_LINE_EDGE_FALLING))
    {
        printf("gpio_wait_for_low: _request_input failed\n");
        return -1;
    }

    // return if line is already low
    if (0 == gpio_read(pin))
    {
        gpio_release(pin);
        return 1;
    }

    // clear any unread events
    struct gpiod_edge_event_buffer* buffer = gpiod_edge_event_buffer_new(1);
    
    int ret = 0;
    do
    {
        ret = gpiod_line_request_wait_edge_events(lines.requests[pin], 0);
        if (1 == ret)
        {
            gpiod_line_request_read_edge_events(lines.requests[pin], buffer, 1);
        }
    } while (1 == ret);
    
    // wait for a falling edge
    ret = gpiod_line_request_wait_edge_events(lines.requests[pin], timeout * 1000000);
    
    gpiod_edge_event_buffer_free(buffer);
    gpio_release(pin);
    return ret;
}
