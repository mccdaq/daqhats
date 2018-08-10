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

#endif