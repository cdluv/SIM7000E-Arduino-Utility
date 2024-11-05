#include <Arduino.h>
#include <app.h>
#include "utils.h"

void initModem() {
    enableHigh();
    pwrKeyHigh();
    sleepGsmLow();
}

void readPhone() {
    while (Serial2.available()) {
        Serial.write(Serial2.read());
    }
}

// ENABLE_GSM = GPIO_NUM_4
void enableHigh() {
    Serial.println("Setting GPIO4 (EN) to HIGH");
    digitalWrite(ENABLE_GSM, HIGH);
}

void enableLow() {
    Serial.println("Setting GPIO14 (EN) to LOW");
    digitalWrite(ENABLE_GSM, LOW);
}

// SLEEP_GSM = GPIO_NUM_15
void sleepGsmHigh() {
    Serial.println("Setting GPIO15 (SLEEP_GSM) to HIGH - low power mode");

    digitalWrite(SLEEP_GSM, HIGH);
}

void sleepGsmLow() {
    Serial.println("Setting GPIO15 (SLEEP_GSM) to LOW - AWAKE");

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
    Serial.println("Setting GPIO13 (PWRKEY) to HIGH");
    digitalWrite(PWRKEY_GSM, HIGH);
}

void pwrKeyLow() {
    Serial.println("Setting GPIO13 (PWRKEY) to LOW");
    digitalWrite(PWRKEY_GSM, LOW);
}
