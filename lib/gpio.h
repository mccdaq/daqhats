/*
*   file gpio.h
*   author Measurement Computing Corp.
*   brief This file contains lightweight GPIO pin control functions.
*
*   date 17 Jan 2018
*/
#ifndef _GPIO_H
#define _GPIO_H

void gpio_init(void);
void gpio_close(void);

void gpio_set_output(unsigned int pin, unsigned int val);
void gpio_write(unsigned int pin, unsigned int val);
void gpio_input(unsigned int pin);
void gpio_release(unsigned int pin);
int gpio_read(unsigned int pin);
int gpio_wait_for_low(unsigned int pin, unsigned int timeout);
int gpio_interrupt_callback(unsigned int pin, unsigned int mode, void (*function)(void*),
    void* data);

#endif