//
// Created by Gary on 05/11/2024.
//
#include <Arduino.h>

#include "ui.h"
#include <app.h>
#include <utils.h>

char data[1024] = {0};
uint16_t charIndex = 0;
uint32_t timeOfLastStatusDisplay = 0;

// "private" methods
void printStatus();

bool checkForPowerScenario(int ix, char value);


void initUi() {
    Serial.println("UI Ready");
}

void printStatus() {
    int gpio4Status = digitalRead(ENABLE_GSM);
    int gpio13Status = digitalRead(PWRKEY_GSM);
    int gpio15Status = digitalRead(SLEEP_GSM);
    Serial.printf("--------------------------------------------------------------------------\n");
    Serial.printf("PWR   (GPIO13) is %s   ", gpio13Status == HIGH ? "ON " : "OFF");
    Serial.print("  [~]+ [#]-         \n");
    Serial.printf("EN    (GPIO4)  is %s   ", gpio4Status == HIGH ? "ON " : "OFF");
    Serial.print("  [:]+ [;]-         \n");
    Serial.printf("SLEEP (GPIO15) is %s", gpio15Status == HIGH ? "AWAKE " : "ASLEEP");
    Serial.print("  [@]+ [']-         \n");
    Serial.print("CYCLE                     [']\n");
    Serial.print("HELP                      [?]\n");
}

void printStatusIfRequired(int seconds) {
    if (millis() - timeOfLastStatusDisplay > (seconds * 1000)) {
        timeOfLastStatusDisplay = millis();
        printStatus();
        Serial.write(data, charIndex);
    }
}

void printHelp() {
    Serial.println("Commands:");
    Serial.println("  ! - power cycle");
    Serial.println("  ^ - light sleep");
    Serial.println("  ? - print this help");
}

bool checkForPinCommand(int index, int value) {
    if (index > 0) {
        return false;
    }

    bool pinCommand = true;

    switch (value) {
        case ':':
            enableHigh();
            break;
        case ';':
            enableLow();
            break;
        case '@':
            sleepGsmHigh();
            break;
        case '\'':
            sleepGsmLow();
            break;
        case '!':
            pwrKeyHigh();
            enableHigh();
            sleepGsmHigh();
            delay(500);
            sleepGsmLow();
            pwrKeyLow();
            delay(100);
            enableHigh();
            break;
        case '^':
            lightSleep();
            break;
        case '#':
            pwrKeyLow();
            break;
        case '~':
            pwrKeyHigh();
            break;
        case '?':
            printHelp();
            break;
        case '/':
            Serial.print("===>");
            charIndex = 1;
            break;
        default:
            pinCommand = false;
            break;
    }

    return pinCommand;
}

bool checkForPowerScenario(int ix, char value) {

    if (value == '/' && ix == 0) {
        Serial.println("1. Start module and awake EN=1, SL=0");
        Serial.println("2. Put module into low power mode EN=1, SL=1");
        Serial.println("3. Wake module up EN=1, SL=0");
        Serial.println("4. Turn off the module EN=0, SL=0");
        return true;
    }

    if ((ix > 0 || data[0] != '/')) {
        return false;
    }

    switch (value) {
        case '1':
            Serial.println("1. Start module and awake EN=1, SL=0");
            pwrKeyHigh();
            enableHigh();
            sleepGsmLow();
            break;
        case '2':
            Serial.println("2. Put module into low power mode EN=1, SL=1");
            pwrKeyLow();
            enableHigh();
            sleepGsmHigh();
            break;
        case '3':
            Serial.println("3. Wake module up EN=1, SL=0");
            pwrKeyHigh();
            enableHigh();
            sleepGsmLow();
            break;
        case '4':
            Serial.println("4. Turn off the module EN=0, SL=0");
            pwrKeyLow();
            enableLow();
            sleepGsmLow();
            break;

        default:
            Serial.println("Invalid command!");
            break;
    }

    return true;
}

void resetToNewLine() {
    charIndex = 0;
    memset(data, 0, sizeof(data));
}

void ui_loop() {
    readPhone();

    printStatusIfRequired(5);

    while (Serial.available()) {
        char value = (char) Serial.read();

        if (checkForPinCommand(charIndex, value)) {
            printStatus();
            resetToNewLine();
            continue;
        }

        if (checkForPowerScenario(charIndex, value)) {
            printStatus();
            resetToNewLine();
            continue;
        }

        char c = (char) value;
        data[charIndex] = c;        //Serial.print(c); local echo

        charIndex++;

        Serial2.print(c);
        Serial.print(c);

        if (value == '\r' || value == '\n') {
            resetToNewLine();
        }
    }
}