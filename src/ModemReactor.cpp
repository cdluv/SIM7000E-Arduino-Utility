//
// Created by Gary on 09/02/2025.
//
#include <Arduino.h>
#include "ModemReactor.h"
#include <cstring>



#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedParameter"

void setCpinReady(char *response, ModemState *modemState) {
    modemState->cpinReady = true;
}

void setCpinNotReady(char * response,ModemState *modemState) {
    modemState->cpinReady = false;
}

void setSmsReady(char * response, ModemState *modemState) {
    modemState->smsReady = true;
}

void handleStatusCheck(char * response, int * status1, int * status2) {
    if (strchr(response, ',')) {
        sscanf(response, "%d,%d", status1, status2);
    } else {
        sscanf(response, "%d", status2);
    }

    // print the status
    Serial.printf("Status1: %d, Status2: %d\n", *status1, *status2);
}
void handleCeregStatus(char *response, ModemState *modemState) {
    // Skip past +CEREG:
    response += 7;

    int n, stat, tac, ci, act, cause_type, reject_cause;
    int parsedItems = sscanf(response, R"(%d,%d,"%x","%x","%x",%d,%d)", &n, &stat, &tac, &ci, &act, &cause_type, &reject_cause);
    CeregData* ceregData = &modemState->ceregData;
    ceregData->status1 = n;
    ceregData->status2 = stat;
    ceregData->trackingAreaCode = (parsedItems > 2) ? tac : -1;
    ceregData->cellId = (parsedItems > 3) ? ci : -1;
    ceregData->accessTechnology = (parsedItems > 4) ? act : -1;
    ceregData->cause_type = (parsedItems > 5) ? cause_type : -1;

    ceregData->registered = (stat == 1 || stat == 5);

    // Show the registration status
    Serial.printf("Registered: %d\n", modemState->ceregData.registered);
}
/**
void handleCeregStatus(char * response, ModemState *modemState) {
    // Skip past +CEGREG:
    response += 8;

    handleStatusCheck(response, &modemState->ceregStatus1, &modemState->ceregStatus2);
    modemState->registeredCereg = modemState->ceregStatus1 == 1 &&
            ( modemState->ceregStatus2 == 5  || modemState->ceregStatus2 == 1);

    // Show the registration status
    Serial.printf("Registered: %d\n", modemState->registeredCereg);

}
**/

void handleCgregStatus(char * response, ModemState *modemState) {
    // Skip past +CGREG:
    response += 7;
    handleStatusCheck(response, &modemState->cgregStatus1, &modemState->cgregStatus2);
    modemState->registeredCgreg = modemState->cgregStatus1 == 1 &&
                                 ( modemState->cgregStatus2 == 5 || modemState->cgregStatus2 == 1);

    Serial.printf("Registered: %d\n", modemState->registeredCgreg);
}

void handleCregStatus(char * response, ModemState *modemState) {
    response +=6;
    handleStatusCheck(response, &modemState->cregStatus1, &modemState->cregStatus2);
    modemState->registeredCreg = modemState->cregStatus1 == 1 &&
                                  ( modemState->cregStatus2 == 5 || modemState->cregStatus2 == 1);

    Serial.printf("Registered: %d\n", modemState->registeredCreg);
}

void handleCgattStatus(char * response, ModemState *modemState) {
    sscanf(response, "+CGATT: %d", &modemState->attached);
}

void handleCsqStatus(char * response, ModemState *modemState) {
    //response += 5;
    sscanf(response, "+CSQ: %d,%d", &modemState->signalStrength, &modemState->signalQuality);
    Serial.printf("Signal Strength: %d, Signal Quality: %d\n", modemState->signalStrength, modemState->signalQuality);
}

void handleCgnApnResponse(char * response, ModemState *modemState) {
    int contextId;
    char apn[64];
    sscanf(response, "+CGNAPN: %d,\"%63[^\"]\"", &contextId, apn);
    modemState->apnContextId = contextId;
    strcpy(modemState->apn, apn);
}



void handleCAState(char *response, ModemState *modemState) {
    int linkNum, result;
    sscanf(response, "+CASTATE: %d,%d", &linkNum, &result);
    modemState->cnActData[linkNum].contextId = linkNum;
    modemState->ipConnected = (result == 1);
    modemState->ipShut = false;
    modemState->ipClosed = !modemState->ipConnected;
    modemState->sendOk = false;
}

void handleCaopen(char *response, ModemState *modemState) {
    int linkNum, result;
    sscanf(response, "+CAOPEN: %d,%d", &linkNum, &result);
    modemState->ipConnected = (result == 1);
    modemState->ipShut = false;
    modemState->ipClosed = !modemState->ipConnected;
    modemState->sendOk = false;
}

void handleCASend(char * response, ModemState *modemState) {
    sscanf(response, "+CASEND: %d,%d,%d",
           &modemState->caSendIdentifier,
           &modemState->caSendResult,
           &modemState->caSendBytesSent);
}

void handleCpsiStatus(char * resp, ModemState *modemState) {
    char tmpStatus[16];

    // The response begins with "+CPSI: " (7 characters). We skip those,
    // then use sscanf() to parse the 14 comma-separated tokens.
    // Note: Token 2 ("234-15") is split into two integers using %d-%d.
    //       Token 3 (LAC) is parsed as a hexadecimal number using %x.

    // Does sscanf copy string values to the char arrays?

    CpsiData* cpsiData = &modemState->cpsiData;
    int ret = sscanf(resp + 7,
                     "%15[^,],%15[^,],%d-%d,%x,%ld,%d,%15[^,],%d,%d,%d,%d,%d,%d,%d",
                     cpsiData->networkType,   // Token 0
                     tmpStatus,               // Token 1
                     &cpsiData->mcc,          // Token 2 (first part, MCC)
                     &cpsiData->mnc,          // Token 2 (second part, MNC)
                     &cpsiData->lac,          // Token 3 (hex LAC)
                     &cpsiData->cellId,       // Token 4
                     &cpsiData->rx,           // Token 5
                     cpsiData->band,          // Token 6
                     &cpsiData->channel,      // Token 7
                     &cpsiData->param8,       // Token 8
                     &cpsiData->param9,       // Token 9
                     &cpsiData->meas1,        // Token 10
                     &cpsiData->meas2,        // Token 11
                     &cpsiData->meas3,        // Token 12
                     &cpsiData->meas4);       // Token 13


    // Determine connection status from the temporary status string.
    // (Here we compare exactly to "Online". Adjust for case-insensitivity if needed.)
    cpsiData->online = (strcmp(tmpStatus, "Online") == 0);

    // Expect 15 items to be successfully scanned.
    if (ret != 15) {
        Serial.print("Parsing error: expected 15 fields, got ");
        Serial.println(ret);
    }
    cpsiData->fieldsParsed = ret;
}


void handleCclkMessage(char * resp, ModemState *modemState) {

    sscanf(resp, "+CCLK: \"%d/%d/%d,%d:%d:%d+%d\"",
           &modemState->year,
           &modemState->month,
           &modemState->day,
           &modemState->hour,
           &modemState->minute,
           &modemState->second,
           &modemState->timezone
   );

    // Print the parsed values for debugging
    Serial.printf("Parsed CCLK: %02d/%02d/%02d %02d:%02d:%02d Timezone: +%02d\n",
                  modemState->year,
                  modemState->month,
                  modemState->day,
                  modemState->hour,
                  modemState->minute,
                  modemState->second,
                  modemState->timezone
    );
}
void handlePsuttzMessage(char* resp,ModemState *modemState) {
    // Handler that parses the *PSUTTZ message and updates the variables

        PsuttzMessage* msg = &modemState->psuttzMessage;
        // Use sscanf to parse the message into the structure fields.
        int ret = sscanf(resp, "*PSUTTZ: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                         &msg->n,
                         &msg->mode,
                         &msg->year,
                         &msg->month,
                         &msg->day,
                         &msg->hour,
                         &msg->minute,
                         &msg->second,
                         &msg->tz,
                         &msg->dst);

        // Check if all 10 fields were successfully parsed.
        if (ret != 10) {
            Serial.printf("Error parsing PSUTTZ message: expected 10 fields but got %d\n", ret);
        }
}
void handleCgpAddrStatus(char * resp,ModemState *modemState) {
    int contextId;
    char pdpType[16];
    sscanf(resp, "+CGPADDR: %d,\"%15[^\"]\"", &contextId, pdpType);
    modemState->cgpAddr[contextId].contextId = contextId;
    strcpy(modemState->cgpAddr[contextId].pdpType, pdpType);
}

void handleCgdContStatus(char * resp,ModemState *modemState) {
    int cid;
    char pdpType[16];
    char apn[64];
    char pdpAddr[16];
    int dComp;
    int hComp;
    sscanf(resp, "+CGDCONT: %d,\"%15[^\"]\",\"%63[^\"]\",\"%15[^\"]\",%d,%d",
           &cid, pdpType, apn, pdpAddr, &dComp, &hComp);

    CgpAddr *ctx = &modemState->cgpAddr[cid];

    ctx->contextId = cid;
    strcpy(ctx->pdpType, pdpType);
}

void handleIpConnect(char * response, ModemState *modemState) {
    modemState->ipConnected = true;
    modemState->ipShut = false;
    modemState->ipClosed = false;
    modemState->sendOk = false;
}

void handleIpConnectError(char * response, ModemState *modemState) {
    modemState->ipConnected = false;
    modemState->ipShut = false;
    modemState->ipClosed = true;
    modemState->sendOk = false;
}

void handleSendOk(char * response, ModemState *modemState) {
    modemState->sendOk = true;
}

void handleIpShut(char * response, ModemState *modemState) {
    modemState->ipShut = true;
    modemState->ipConnected = false;
    modemState->ipClosed = true;
}

void handleIpClose(char * response, ModemState *modemState) {
    modemState->ipConnected = false;
    modemState->ipClosed = true;
    modemState->sendOk = false;
    modemState->ipShut = false;

}

void handlePdpDeact(char *response,ModemState *modemState) {
    modemState->pdpActive = false;
    modemState->pdpDeactivated = true;
}

void handlePdpAct(char *response,ModemState *modemState) {
    modemState->pdpActive = true;
    modemState->pdpDeactivated = false;
}

void handleAppCNAct(char *response,ModemState *modemState) {
    int contextId, activated;
    char ipAddress[17];
    sscanf(response, R"(+CNACT: %d,%d,"%16[^"]")", &contextId, &activated, ipAddress);
    modemState->cnActData[contextId].contextId = contextId;
    modemState->cnActData[contextId].activated = activated;
    strncpy(modemState->cnActData[contextId].ipAddress, ipAddress, 17);
}

void handleAppPdpDeact(char *response,ModemState *modemState) {
    sscanf(response, "+APP PDP: %d,%15s", &modemState->appPdpContextId, &modemState->appPdpStatus);

    modemState->appPdpActive = false;
    modemState->appPdpDeactivated = true;
}

void handleAppPdpAct(char *response,ModemState *modemState) {
    sscanf(response, "+APP PDP: %d,%15s", &modemState->appPdpContextId, &modemState->appPdpStatus);
    modemState->appPdpActive = true;
    modemState->appPdpDeactivated = false;
}

// IMPORTANT: The reactions array must be sorted alphabetically in  ascending order.
Reactor reactions[] = {
        Reactor{"+APP PDP",          handlePdpAct},
        Reactor{"+CAOPEN:",          handleCaopen},
        Reactor{"+CASEND:",          handleCASend},
        Reactor{"+CASTATE:",         handleCAState},
        Reactor{"+CCLK:",            handleCclkMessage},
        Reactor{"+CEREG:",           handleCeregStatus},
        Reactor{"+CGATT:",           handleCgattStatus},
        Reactor{"+CGPADDR:",         handleCgpAddrStatus},
        Reactor{"+CGREG:",           handleCgregStatus},
        Reactor{"+CNACT",            handleAppCNAct},
        Reactor{"+CPIN: NOT READY",  setCpinNotReady},
        Reactor{"+CPIN: READY",      setCpinReady},
        Reactor{"+CPSI:",            handleCpsiStatus},
        Reactor{"+CREG:",            handleCregStatus},
        Reactor{"+CSQ:",             handleCsqStatus},
        Reactor{"+PDP ACT",          handlePdpAct},
        Reactor{"+PDP DEACT",        handlePdpDeact},
        Reactor{"CLOSE OK",          handleIpClose},
        Reactor{"CONNECT ERROR",     handleIpConnectError},
        Reactor{"CONNECT FAIL",      handleIpConnectError},
        Reactor{"CONNECT OK",        handleIpConnect},
        Reactor{"SEND OK",           handleSendOk},
        Reactor{"SHUT OK",           handleIpShut},
        Reactor{"SMS Ready",         setSmsReady},
        Reactor{"STATE: PDP ACT",    handlePdpAct},
        Reactor{"STATE: PDP DEACT",  handlePdpDeact},
};

/*
char* findFirstNonWhitespace(char* str, int len) {
    int pos = 0;

    while (*str == ' ' || *str == '\t' && pos < len) {
        str++;
        pos++;
    }
    return str;
}
*/

// Binary search across the reactions array to find the appropriate handler for the response.
bool checkAndHandleUnsolicitedResponses(char *responseLine, ModemState *modemState) {
    int left = 0;
    int right = sizeof(reactions) / sizeof(reactions[0]) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        const char* reactionMessage = reactions[mid].message.c_str();
        size_t len = reactions[mid].message.length();

        int cmp = strncmp(responseLine, reactionMessage, len);
        if (cmp == 0) {
            //Serial.printf("Handling unsolicited response: %s\n", responseLine);
            reactions[mid].reactor(responseLine, modemState);

            return true;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return false;
}
/**
bool checkAndHandleUnsolicitedResponses(char *responseLine, ModemState *modemState) {
    for (int i = 0; i < sizeof(reactions) / sizeof(reactions[0]); i++) {

        // find the first non-whitespace character in responseLine
        if (responseLine == nullptr || responseLine[0] == '\0') {
            return false;
        }

        char *firstChar = findFirstNonWhitespace(responseLine, strlen(responseLine));

        const char* reactionMessage = reactions[i].message.c_str();
        size_t len = reactions[i].message.length();

        if (strncmp(firstChar, reactionMessage, len) == 0) {
            Serial.printf("Handling unsolicited response: %s\n", responseLine);
            reactions[i].reactor(responseLine, modemState);
            return true;
        }
    }
    return false;
}
 */
#pragma clang diagnostic pop
