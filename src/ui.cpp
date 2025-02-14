//
// Created by Gary on 05/11/2024.
//
#include <Arduino.h>

#include "ui.h"
#include <app.h>
#include <utils.h>
#include "ATCommandHandler.h"
#include "ModemReactor.h"

char data[1024] = {0};
uint16_t charIndex = 0;
uint32_t timeOfLastStatusDisplay = 0;
extern ModemState state;

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
    Serial.print("  ON=~  OFF=#  \n");
    Serial.printf("EN    (GPIO4)  is %s   ", gpio4Status == HIGH ? "ON " : "OFF");
    Serial.print("  on=:  OFF=;  \n");
    Serial.printf("SLEEP (GPIO15) is %s", gpio15Status == HIGH ? "HI " : "LO ");
    Serial.print("  on=@  OFF=\' \n");
    Serial.print("CYCLE   [\\]\n");
    Serial.print("HELP    [?]\n");
}

void printStatusIfRequired(int seconds) {
    if (millis() - timeOfLastStatusDisplay > (seconds * 1000)) {
        timeOfLastStatusDisplay = millis();
        //printStatus();
        Serial.write(data, charIndex);
    }
}

void printHelp() {
    Serial.println("Commands:");
    Serial.println("  ! - power cycle");
    Serial.println("  ^ - light sleep");
    Serial.println("  S - pin status");
    Serial.println("  ? - print this help");
    Serial.println("  : - enableHigh");
    Serial.println("  ; - enableLow");
    Serial.println("  @ - sleepGsmHigh");
    Serial.println("  ' - sleepGsmLow");
    Serial.println("  $ - send CTRL+Z");
    Serial.println("  / - start power scenario and ADDITIONAL commands");
    Serial.println("  ! - power cycle");
    Serial.println("  ^ - light sleep");
    Serial.println("  # - pwrKeyLow");
    Serial.println("  ~ - pwrKeyHigh");
    Serial.println("  \\ - power cycle");

    Serial.printf("Serial Buffer occupied     : %d \n", Serial.available());
    Serial.printf("Serial.availableForWrite() : %d \n", Serial.availableForWrite());
    Serial.printf("Serial2 Buffer occupied    : %d \n", Serial2.available());
    Serial.printf("Serial2.availableForWrite(): %d \n", Serial2.availableForWrite());
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
        case 'S':
            printStatus();
            Serial.printf("Serial Buffer occupied     : %d \n", Serial.available());
            Serial.printf("Serial.availableForWrite() : %d \n", Serial.availableForWrite());
            Serial.printf("Serial2 Buffer occupied    : %d \n", Serial2.available());
            Serial.printf("Serial2.availableForWrite(): %d \n", Serial2.availableForWrite());
            break;

        case '!':
            pwrKeyHigh();
            enableHigh();
            sleepGsmLow();

            delay(500);

            pwrKeyLow();

            delay(2000);

            enableHigh();
            sleepGsmHigh();
            break;
        case '$':
            // Ascii for ctrl+z
            Serial2.print(0x1a);
            Serial.println("CTRL+Z sent");
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
        case '\\':
            Serial.println("Power cycle Phase 1");
            sleepGsmLow();
            pwrKeyHigh();
            enableHigh();

            delay(500);

            Serial.println("Power cycle Phase 2");
            sleepGsmHigh();
            pwrKeyLow();

            delay(2000);

            Serial.println("Power cycle Phase 3");
            pwrKeyHigh();

        default:
            pinCommand = false;
            break;
    }

    if (pinCommand) {
        Serial.println("PIN Command executed ");
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
        case '5':
            Serial.println("Running module diagnostics");
            state.toString();
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

void drainPhoneUART() {
    Serial.println("Draining phone UART....");
    while (Serial2.available()) {
        char chr = (char) Serial2.read();
        if (chr != '`') {
            Serial.print(chr);
        }
    }
    Serial.println("..... Drained");
}

bool ui_loop() {
    printStatusIfRequired(5);

    while (Serial.available()) {
        char chr = (char) Serial.read();

        if (chr == '`') {
            chr = 0x1a;
        }

        //Serial2.print(chr);

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

        Serial.printf("%c", chr); // echo to console

        if (chr == '\r' || chr == '\n') {
            data[charIndex] = 0;
            Serial2.printf("%s\n",data); // echo to phone
            Serial2.flush();

            Serial.printf(">>> %s\n",data);

            resetToNewLine();
        }

        if (data[0] == '*' && data[1] == '*' && data[2] == '*') {
            Serial.println("Exiting UI loop");
            Serial2.flush();
            resetToNewLine();
            return false;
        }
    }
    while (Serial2.available()) {
        char chr = (char) Serial2.read();
        if (chr != '`') {
            Serial.print(chr);
        }
    }
    return true;
}