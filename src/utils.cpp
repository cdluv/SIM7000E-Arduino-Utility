#include <Arduino.h>
#include <app.h>
#include "utils.h"
#include <cstring>
#include <cctype>

void initModem() {
    enableHigh();
    delay(200);

    enableLow();
    delay(200);

    enableHigh();
    sleepGsmHigh();
}

void readPhone() {
    while (Serial2.available()) {
        Serial.write(Serial2.read());
    }
}

// ENABLE_GSM = GPIO_NUM_4
void enableHigh() {
    Serial.printf("Setting GPIO%d to 1\n", ENABLE_GSM);
    digitalWrite(ENABLE_GSM, HIGH);
}

 void  enableLow() {
    Serial.printf("Setting GPIO%d to 0\n", ENABLE_GSM);
    digitalWrite(ENABLE_GSM, LOW);
}

// SLEEP_GSM = GPIO_NUM_15
void sleepGsmHigh() {
    Serial.printf("Setting GPIO%d (SLEEP_GSM) to 1 - AWAKE\n", SLEEP_GSM);
    digitalWrite(SLEEP_GSM, HIGH);
}

void sleepGsmLow() {
    Serial.printf("Setting GPIO%d (SLEEP_GSM) to 0 - low power mode\n", SLEEP_GSM);
    digitalWrite(SLEEP_GSM, LOW);
}

void lightSleep() {
    Serial.println("Entering Light Sleep 10s");
    // wake up in 5 sec
    esp_sleep_enable_timer_wakeup(10000000);
    delay(10);
    esp_light_sleep_start();
    Serial.println("!!! WAKE UP !!!");
}

void pwrKeyHigh() {
    Serial.printf("Setting GPIO%d (PWRKEY) to 1\n", PWRKEY_GSM);
    digitalWrite(PWRKEY_GSM, HIGH);
}

void pwrKeyLow() {
    Serial.printf("Setting GPIO%d(PWRKEY) to LOW\n", PWRKEY_GSM);
    digitalWrite(PWRKEY_GSM, LOW);
}

// Function to trim leading and trailing whitespace
void strtrim(char* str) {
    char* end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = 0;
}

// Function to scan, trim, and copy the string
void scanAndTrimString(const char* source, const char* format, char* dest, size_t maxLen) {
    char temp[maxLen + 1]; // Temporary buffer
    if (sscanf(source, format, temp) == 1) {
        strtrim(temp);
        strncpy(dest, temp, maxLen);
        dest[maxLen] = '\0'; // Ensure null-termination
    }
}
