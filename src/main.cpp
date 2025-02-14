#include <Arduino.h>
#include <app.h>
#include "ui.h"
#include "utils.h"
#include "ATCommandHandler.h"
#include "ModemReactor.h"
#include "TinyGsmCommon.h"

// TODO:  Checkout https://supabase.com/ for a free backend database

bool timeout = false;
ModemState* appModemState;

//define DB Serial.println
#define DB(x)

void setup() {
    Serial.setTxBufferSize(1024);
    Serial.setRxBufferSize(1024);
    Serial.begin(460800);

    //Serial2.begin(9600, SERIAL_8N1, GPIO_NUM_16, GPIO_NUM_17);

    pinMode(PWRKEY_GSM, OUTPUT);
    pinMode(ENABLE_GSM, OUTPUT);
    pinMode(SLEEP_GSM, OUTPUT);

    initModem();
    initUi();

    Serial2.begin(115200, SERIAL_8N1, GPIO_NUM_16, GPIO_NUM_17);
    Serial2.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE);
    appModemState = new ModemState();

    setup_commandHandler(&Serial2, appModemState, &timeout);

}

void wakeSim7000E() {
    if (digitalRead(ENABLE_GSM) == HIGH) {
        DB("Module is already on");

        if (digitalRead(SLEEP_GSM) == LOW) {
            DB("Module is asleep - waking up");
            sleepGsmHigh();
        } else {
            DB("Waking up GSM module");
            enableHigh();
        }

        // Force a reset the device
        send("AT+CFUN=6", "OK", 1000, &timeout);

    } else {
        DB("Waking up GSM module");
        enableHigh();
        sleepGsmHigh();
        delay(50);

        enableLow();
        delay(50);

        enableHigh();
    }
}
void wakeSim7080G() {

    digitalWrite(ENABLE_GSM, HIGH);
    digitalWrite(SLEEP_GSM, HIGH);
    digitalWrite(PWRKEY_GSM, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(PWRKEY_GSM, LOW);
    digitalWrite(ENABLE_GSM, LOW);
    delay(100);

    delay(1000);
    digitalWrite(PWRKEY_GSM, HIGH);
    digitalWrite(SLEEP_GSM, HIGH);
    digitalWrite(ENABLE_GSM, HIGH);

    DB("Retry start modem .");
}

//  Wait up to 15 seconds for modem to send "SMS Ready"
bool phoneIsReady(ModemState *modemState) {
    uint32_t timeoutTime = millis() + 15000;

    while (millis() < timeoutTime) {
        timeout = false;
        send("AT", "OK", 2000, &timeout);

        // Has startup settled down yet?
        if (modemState->smsReady) {
            return true;
        }
        delay(500);
    }
    return false;
}

void enterDiagnosticMode(ModemState *modemState) {
    Serial.println("*** ENTERING DIAG MODE ***");
    while ( ui_loop() ) {
        while  (Serial2.available()) {
            String data = Serial2.readString();

            if (data == nullptr || data.length() == 0) {
                continue;
            }
            checkAndHandleUnsolicitedResponses((char*)data.c_str(), modemState);
        }

        delay(50);
    }
    Serial.println("*** Exiting diagnostic mode ***");
}

void loop() {
    ui_loop();
    //return;

    long startTime = millis();

    //wakeSim7080G();
    wakeSim7000E();

    timeout = false;
    bool updated = false;

    static auto ipAddress = "139.59.180.17" ;
    static int port = 8080;

    if (phoneIsReady(appModemState)) {
        DB("Configuring Modem for network connection");

        if (connectToNetwork()) {
            if (!connect(ipAddress, port)) {
                DB("Failed to connect to server");
                return;
            }
            DB("Sending update");

            char message[128] = {0};

            snprintf(message, sizeof(message), "Hello from cell: %8lx, tac: %x, message: %s",
                     appModemState->cpsiData.cellId,
                     appModemState->ceregData.trackingAreaCode,
                     appModemState->ipAddress);

            updated = sendUpdate(message);

            if ( updated ) {
                DB(">>>>>>>Update sent successfully");
                timeout = false;
            }

            close();
        }
    }

    if (timeout) {
        DBG("Timeout occurred.");
    }

    DBG("Entering diagnostic mode");
    //enterDiagnosticMode();

    Serial.print("Modem state:\n");
    appModemState->toString();
    DBG("SHUTTING DOWN====================================================================");


    DBG("Power-down module");
    send("AT+CFUN=0", "OK", 2000, &timeout);
    send("AT+CPOWD=1", "NORMAL POWER DOWN", 2000, &timeout);

    enableLow();
    sleepGsmLow();

    // put esp32 into light sleep for x seconds
    int x = 60;
    uint32_t duration = millis() - startTime;
    Serial.printf("Awake for : %ld ms\n", duration);
    Serial.printf("Entering Light Sleep %ds", x);
    Serial.flush();

    esp_sleep_enable_timer_wakeup(x * 1000000);
    esp_light_sleep_start();
    DBG("!!! WAKE UP !!!");
}