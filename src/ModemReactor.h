//
// Created by Gary on 09/02/2025.
//

#ifndef SIM7000E_MODEMREACTOR_H
#define SIM7000E_MODEMREACTOR_H

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>

#define MODEM_DEBUG

// Defines a struct to hold all fields from the AT+CPSI response.
struct CpsiData {
    char networkType[16];  // Token 0: e.g., "LTE CAT-M1"
    bool online;     // Token 1: "Online" or "Offline" (determined later)
    int mcc;               // Token 2 (from "234-15"): MCC = 234
    int mnc;               // Token 2 (from "234-15"): MNC = 15
    uint32_t lac;      // Token 3: LAC in hexadecimal (e.g., 0x182C)
    long cellId;           // Token 4: e.g., 2280970
    int rx;                // Token 5: e.g., 306
    char band[16];         // Token 6: e.g., "EUTRAN-BAND20"
    int channel;           // Token 7: e.g., 6300
    int param8;            // Token 8: e.g., 3
    int param9;            // Token 9: e.g., 3
    int meas1;             // Token 10: e.g., -11
    int meas2;             // Token 11: e.g., -94
    int meas3;             // Token 12: e.g., -66
    int meas4;             // Token 13: e.g., 11
    int fieldsParsed;      // Count of fields parsed from last +CPSI message.
};

struct CNActData {
    int contextId;
    int activated;
    int connected;
    char ipAddress[17];
};

struct CgpAddr {
    int contextId;
    char pdpType[16];
};

// Define a structure to hold the parsed PSUTTZ fields.
struct PsuttzMessage {
    int n;       // Notification Type Indicator
    int mode;    // Update Mode
    int year;    // Year (could be two-digit or four-digit based on module)
    int month;   // Month (1-12)
    int day;     // Day (1-31)
    int hour;    // Hour (0-23)
    int minute;  // Minute (0-59)
    int second;  // Second (0-59)
    int tz;      // Time Zone offset (in minutes or as defined by module)
    int dst;     // Daylight Saving Time indicator (typically 0 or 1)

    void toString() const {
#ifdef MODEM_DEBUG
        Serial.printf("    Notification Type (n): %d\n", n);
        Serial.printf("    Update Mode (mode)   : %d\n", mode);
        Serial.printf("    Year                 : %d\n", year);
        Serial.printf("    Month                : %d\n", month);
        Serial.printf("    Day                  : %d\n", day);
        Serial.printf("    Hour                 : %d\n", hour);
        Serial.printf("    Minute               : %d\n", minute);
        Serial.printf("    Second               : %d\n", second);
        Serial.printf("    Time Zone Offset (tz): %d\n", tz);
        Serial.printf("    Daylight Saving (dst): %d\n", dst);
    }
#endif // MODEM_DEBUG
};

struct CeregData {
    int status1;
    int status2;
    int trackingAreaCode;
    int cellId;
    int accessTechnology;
    int cause_type;
    bool registered;

    void toString() const {
#ifdef MODEM_DEBUG
        Serial.printf("CeregData:\n");
        Serial.printf("  status1: %d\n", status1);
        Serial.printf("  status2: %d\n", status2);
        Serial.printf("  tracking area code: %4x\n", trackingAreaCode);
        Serial.printf("  cell id: %x\n", cellId);
        Serial.printf("  access technology: %x\n", accessTechnology);
        Serial.printf("  cause_type: %d\n", cause_type);
        Serial.printf("  registered: %d\n", registered);
#endif // MODEM_DEBUG
    }
};

struct ModemState {
    bool cpinReady;
    bool smsReady;
    CeregData ceregData = CeregData();
    int cregStatus1;
    int cregStatus2;
    bool registeredCreg;
    int cgregStatus1;
    int cgregStatus2;
    bool registeredCgreg;
    int attached;
    int signalStrength;
    int signalQuality;

    char ipAddress[20];
    bool ipConnected;
    bool ipShut;
    bool ipClosed;

    bool pdpActive;
    bool pdpDeactivated;

    int appPdpContextId;
    char appPdpStatus[17]={0};

    bool appPdpActive;
    bool appPdpDeactivated;

    bool sendOk;

    int apnContextId;
    char apn[64];

    int caSendIdentifier;
    int caSendResult;
    int caSendBytesSent;

    CNActData cnActData[4] = {CNActData(), CNActData(), CNActData(), CNActData()};

    CpsiData cpsiData = CpsiData();

    CgpAddr cgpAddr[16] = {CgpAddr{}, CgpAddr{}, CgpAddr{}, CgpAddr{},
                           CgpAddr{}, CgpAddr{}, CgpAddr{}, CgpAddr{},
                           CgpAddr{}, CgpAddr{}, CgpAddr{}, CgpAddr{},
                           CgpAddr{}, CgpAddr{}, CgpAddr{}, CgpAddr{},
    };

    PsuttzMessage psuttzMessage = PsuttzMessage();

    int year, month, day,hour, minute, second, timezone;

    const char *decodeRegistrationStatus(int status) const {
#ifdef MODEM_DEBUG
        switch (status) {
            case 0:
                return "NOT REG OR SEARCHING";
            case 1:
                return "HOME";
            case 2:
                return "SEARCHING";
            case 3:
                return "REG DENIED";
            case 4:
                return "NO COVERAGE";
            case 5:
                return "ROAMING";
            default:
                return "Unknown";
        }
#else
    return "";
#endif // MODEM_DEBUG
    }

    const char *decodeNetworkRegistration(int value) const {
#ifdef MODEM_DEBUG
        switch (value) {
            case 0:
                return "Unsolicited results disabled.";
            case 1:
                return "Unsolicited result codes - numeric information only.";
            case 2:
                return "Unsolicited result codes - Location Area Code and Cell ID.";
            default:
                return "Unknown value.";
        }
#else
    return "";
#endif // MODEM_DEBUG
    }

    void toString() const {
#ifdef  MODEM_DEBUG
        Serial.printf("ModemState:\n");
        Serial.printf("  cpinReady:        %d\n", cpinReady);
        Serial.printf("  smsReady:         %d\n", smsReady);
        ceregData.toString();
        Serial.printf("  cregStatus1:      %s\n", decodeNetworkRegistration(cregStatus1));
        Serial.printf("  cregStatus2:      %s\n", decodeRegistrationStatus(cregStatus2));
        Serial.printf("  registeredCreg:   %d\n", registeredCreg);
        Serial.printf("  cgregStatus1:     %s\n", decodeNetworkRegistration(cgregStatus1));
        Serial.printf("  cgregStatus2:     %s\n", decodeRegistrationStatus(cgregStatus2));
        Serial.printf("  registeredCgreg:  %d\n", registeredCgreg);
        Serial.printf("  attached:         %d\n", attached);
        Serial.printf("  signalStrength:   %d\n", signalStrength);
        Serial.printf("  signalQuality:    %d\n", signalQuality);
        Serial.printf("  ipAddress:        %s\n", ipAddress);
        Serial.printf("  ipConnected:      %d\n", ipConnected);
        Serial.printf("  sendOk:           %d\n", sendOk);
        Serial.printf("  ipShut:           %d\n", ipShut);
        Serial.printf("  ipClosed:         %d\n", ipClosed);
        Serial.printf("  pdpActive:        %d\n", pdpActive);
        Serial.printf("  pdpDeactivated:   %d\n", pdpDeactivated);
        Serial.printf("  appPdpActive:     %d\n", appPdpActive);
        Serial.printf("  appPdpDeactivated: %d\n", appPdpDeactivated);
        Serial.printf("  appPdpContextId:  %d\n", appPdpContextId);
        Serial.printf("  appPdpStatus:     %s\n", appPdpStatus);
        Serial.printf("  apnContextId:     %d\n", apnContextId);
        Serial.printf("  apn:              %s\n", apn);
        Serial.printf("  caSendIdentifier: %d\n", caSendIdentifier);
        Serial.printf("  caSendResult:     %d\n", caSendResult);
        Serial.printf("  caSendBytesSent:  %d\n", caSendBytesSent);

        Serial.printf("  CNACT Data:\n");
        for (int i = 0; i < 4; ++i) {
            Serial.printf("    ctxId: %d, ACTIVATED: %d, ipAddr: %s\n",
                          cnActData[i].contextId, cnActData[i].activated, cnActData[i].ipAddress);
        }
        Serial.printf("  CPSI Data:\n");
        Serial.printf("    networkType:   %s\n", cpsiData.networkType);
        Serial.printf("    online:        %s\n", cpsiData.online ? "Online" : "Offline");
        Serial.printf("    mcc:           %d\n", cpsiData.mcc);
        Serial.printf("    mnc:           %d\n", cpsiData.mnc);
        Serial.printf("    lac:           %X\n", cpsiData.lac);
        Serial.printf("    cellId:        %ld\n", cpsiData.cellId);
        Serial.printf("    rx:            %d\n", cpsiData.rx);
        Serial.printf("    band:          %s\n", cpsiData.band);
        Serial.printf("    channel:       %d\n", cpsiData.channel);
        Serial.printf("    param8:        %d\n", cpsiData.param8);
        Serial.printf("    param9:        %d\n", cpsiData.param9);
        Serial.printf("    meas1:         %d\n", cpsiData.meas1);
        Serial.printf("    meas2:         %d\n", cpsiData.meas2);
        Serial.printf("    meas3:         %d\n", cpsiData.meas3);
        Serial.printf("    meas4:         %d\n", cpsiData.meas4);
        Serial.printf("    fieldsParsed:  %d\n", cpsiData.fieldsParsed);

        Serial.printf("    %d/%d/%d, %d:%d:%d\n", year, month, day, hour, minute, second);
        Serial.printf("    Timezone: %d\n", timezone);
        Serial.printf("  PSUTTZ Message:\n");
        psuttzMessage.toString();

        for (int i = 0; i < 16; ++i) {
            Serial.printf("  cgpAddr[%d]: contextId: %d, pdpType: %s\n", i, cgpAddr[i].contextId, cgpAddr[i].pdpType);
        }
    };
    #endif // MODEM_DEBUG

};

struct Reactor {
    String message;
    void (*reactor)(char * response, ModemState *modemState );
    Reactor(const std::string &msg, void (*react)(char * response, ModemState * modemState)) : message(msg.c_str()), reactor(react) {}

};

bool checkAndHandleUnsolicitedResponses(char *responseLine, ModemState *modemState);

#endif //SIM7000E_MODEMREACTOR_H
