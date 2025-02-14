//
// Created by Gary on 05/11/2024.
//
#include <cctype>
#include <esp32-hal.h>

#ifndef SIM7000E_UTILS_H
#define SIM7000E_UTILS_H
void initModem();

void enableHigh();
void enableLow();
void pwrKeyHigh();
void pwrKeyLow();
void sleepGsmHigh();
void sleepGsmLow();
void readPhone();
void lightSleep();

#endif //SIM7000E_UTILS_CPP_H

