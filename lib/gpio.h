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

void gpio_dir(int pin, int dir);
void gpio_write(int pin, int val);
int gpio_status(int pin);
int gpio_wait_for_low(int pin, int timeout);
int gpio_interrupt_callback(int pin, int mode, void (*function)(void*),
    void* data);

#endif