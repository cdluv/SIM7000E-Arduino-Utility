//
// Created by Gary on 09/02/2025.
//
#include <Arduino.h>

#ifndef SIM7000E_STRUTIL_H
#define SIM7000E_STRUTIL_H

void salt(char* result, char* message, const char* salt);
void strtrim(char* str);


#endif //SIM7000E_STRUTIL_H
