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
            pinCommand = false;
            break;

        default:
            pinCommand = false;
            break;
    }

    return pinCommand;
}

bool checkForPowerScenario(int ix, char value) {

    if (ix == 0) {
        if (value == '/') {
            // starts with /, so is a potential  power scenario.
            Serial.println("1. Start module and awake EN=1, SL=0");
            Serial.println("2. Put module into low power mode EN=1, SL=1");
            Serial.println("3. Wake module up EN=1, SL=0");
            Serial.println("4. Turn off the module EN=0, SL=0");
            return false;
        }
    }
    if (data[0] != '/' || ix != 1) {
        // doesn't start with / so not a power scenario
        return false;
    }

    // data[0] == '/' and ix == 1
    // .... so evaluate which power scenario to run (if any)
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
            return false;
    }

    data[ix+1] = 0;
    Serial.printf("POWER Command executed %s", data);
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
        char chr = (char) Serial.read();

        if (checkForPinCommand(charIndex, chr)) {
            printStatus();
            resetToNewLine();
            continue;
        }

        if (checkForPowerScenario(charIndex, chr)) {
            printStatus();
            resetToNewLine();
            continue;
        }

        data[charIndex] = chr;        //Serial.print(c); local echo
        charIndex++;

        Serial2.print(chr); // echo to phone
        Serial.print(chr); // echo to console

        if (chr == '\r' || chr == '\n') {
            Serial2.flush();
            resetToNewLine();
        }
    }
}