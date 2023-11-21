/*
*   file gpio.h
*   author Measurement Computing Corp.
*   brief This file contains lightweight GPIO pin control functions.
*
*   date 17 Jan 2018
*/
#ifndef _GPIO_H
#define _GPIO_H

// Simple GPIO functions for setting output values on address pins

void gpio_init(void);
void gpio_close(void);

void gpio_dir(unsigned int pin, unsigned int dir);
void gpio_write(unsigned int pin, unsigned int val);
int gpio_status(unsigned int pin);
int gpio_wait_for_low(unsigned int pin, unsigned int timeout);
int gpio_interrupt_callback(unsigned int pin, unsigned int mode, void (*function)(void*),
    void* data);
void gpio_release_pin(unsigned int pin);

#endif