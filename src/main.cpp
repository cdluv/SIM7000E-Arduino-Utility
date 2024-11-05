#include <Arduino.h>
#include <app.h>
#include "ui.h"
#include "utils.h"

void setup() {
    Serial.begin(460800);
    Serial2.begin(115200, SERIAL_8N1, GPIO_NUM_16, GPIO_NUM_17);
    Serial2.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE);

    pinMode(PWRKEY_GSM, OUTPUT);
    pinMode(ENABLE_GSM, OUTPUT);
    pinMode(SLEEP_GSM, OUTPUT);

    initModem();
    initUi();
}

void loop() {
    ui_loop();
}