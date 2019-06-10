/**
*   file nist.h
*   author Measurement Computing Corp.
*   brief This file contains NIST thermocouple linearization functions.
*
*   date 1 Feb 2018
*/
#ifndef _NIST_H
#define _NIST_H

double NISTCalcVoltage(unsigned int type, double temperature);
double NISTCalcTemperature(unsigned int type, double voltage);

#endif
