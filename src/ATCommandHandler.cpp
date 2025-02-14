//
// Created by Gary on 09/02/2025.
//
#include <Arduino.h>
#include "ModemReactor.h"
#include "ATCommandHandler.h"
#include <cstring>
#include "strutil.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wwritable-strings"
#define TIMEOUT_MS 5000
#define BUFFER_SIZE 1024
#define MODEM_BAUD_RATE 115200
#define SIM7000E
extern ModemState state;          // Various states of the Modem
extern boolean timeout;         // Timeouts are global so that they can be accessed from the callback functions

char lineBuffer[BUFFER_SIZE];   // Global buffer to hold the incoming line
char sendBuffer[BUFFER_SIZE];   // Global buffer to hold the outgoing line
char* readLineWithTimeout(unsigned long timeoutMs, bool* timeoutOccurred);


char* send(const char* command, const char* expectedResponse, unsigned long responseTimeoutMs = TIMEOUT_MS, bool* responseTimeout = nullptr) {
    if (responseTimeout != nullptr && *responseTimeout) {
        //Serial.printf("previous timeout - skipped: %s\n", command);
        return nullptr;
    }

    //Serial.printf("> %s\n", command);

    // Send the command
    Serial2.println(command);

    if (responseTimeout) {
        *responseTimeout = false;
    }

    while (true) {
        char* lineBuffer = readLineWithTimeout(responseTimeoutMs, responseTimeout);
        if (responseTimeout && *responseTimeout) {
            Serial.printf(" Timeout - %s.  Data: [%s]\n", *responseTimeout ? "true" : "false", lineBuffer);
            return lineBuffer;
        }

        // check if lineBuffer is empty
        if (lineBuffer[0] == '\0') {
            continue;
        }

        if (checkAndHandleUnsolicitedResponses(lineBuffer, &state)) {
            if (expectedResponse[0] == '\0' || strstr(lineBuffer, expectedResponse) || strstr(lineBuffer, "ERROR")) {
                Serial.printf("1 < %s\n", lineBuffer);
                return lineBuffer;
            }

            continue;
        }

        Serial.printf("2 < %s\n", lineBuffer);

        // Did we get an error?
        if (strstr(lineBuffer, "ERROR")) {
            return lineBuffer;
        }

        // Have we matched an expected response yet?
        if (!strstr(lineBuffer, expectedResponse)) {
            // Keep going
            continue;
        }

        // We found the expected response
        return lineBuffer;
    }
}


char* sendAndHandleResponse(const char* command, const char* expectedResponse, unsigned long responseTimeoutMs = TIMEOUT_MS, bool* responseTimeout = nullptr, void (*callback)() = nullptr  ) {
    char *response = send(command, expectedResponse, responseTimeoutMs, responseTimeout);
    callback();
    return response;
}


// This function attempts to read a line from Serial (UART) with a timeout.
// If a complete line (ending with '\n') is received within TIMEOUT_MS,
// the function returns a pointer to a null-terminated string in lineBuffer.
// If a timeout occurs before receiving the full line, it returns NULL.
char* readLineWithTimeout(unsigned long responseTimeoutMs = TIMEOUT_MS, bool* timeoutOccurred = nullptr) {
    if (timeoutOccurred != nullptr) {
        *timeoutOccurred = false;
    }

    uint32_t lastCharTime = millis();
    int index =0;

    // Wait for the first character.
    // If no character is received within TIMEOUT_MS, return NULL.
    while (Serial2.available() == 0) {
        if (millis() - lastCharTime >= responseTimeoutMs) {
            if (timeoutOccurred != nullptr) {
                *timeoutOccurred = true;
                lineBuffer[index] = '\0';
            }
            return lineBuffer;
        }
        delay(100);
    }

    // We have at least one character now; update the timeout timer.
    lastCharTime = millis();

    // received bytes
    int receivedByteCount = 0;

    // Read characters until a newline is encountered.
    while (true) {
        if (Serial2.available() > 0) {
            int c = Serial2.read();

            // Got something - save the time
            lastCharTime = millis();

            // If we get a newline, check the line buffer for unsolicited messages
            if (c == '\n') {
                lineBuffer[receivedByteCount] = '\0';

                if (checkAndHandleUnsolicitedResponses(lineBuffer, &state)) {
                    Serial.printf("2 < %s\n", lineBuffer);
                    break;
                }
                break;
            }

            if (c == '\r') {
                continue;
            }

            if (receivedByteCount < BUFFER_SIZE - 1) {
                lineBuffer[receivedByteCount++] = c;
                lineBuffer[receivedByteCount] = '\0';
                continue;
            }

            Serial.println("!!!!!!!   Buffer overflow");
            lineBuffer[BUFFER_SIZE -1] = '\0';

        } else {
            if (lastCharTime + responseTimeoutMs > millis() ) {
                if (timeoutOccurred != nullptr) {
                    *timeoutOccurred = true;
                }
                lineBuffer[receivedByteCount] = '\0';
                break;
            }
            delay(100);
        }
    }
    return lineBuffer;
}


// Relies on checkAndHandleUnsolicitedResponses to change the state of "registeredField"
// This happens automatically, when "sendAndWaitForResponse" is called.
void pollForStateChangeBool(const char * queryString, const bool* registeredField) {

    if (!timeout) {
        uint32_t regExpireTime = millis() + 120000;

        while (millis() < regExpireTime && !*registeredField) {
            timeout = false;
            send(queryString, "OK", 2000, &timeout);
            delay(500);
        }
    } else {
        Serial.printf("Skipping %s\n", queryString);
    }
}

/**
bool waitForState(const char * queryString, int* field, int validValuesLength, int* validValues[]) {
    if (!timeout) {
        uint32_t regExpireTime = millis() + 120000;

        while (millis() < regExpireTime) {
            send(queryString, "OK",2000, &timeout);
            for (int i = 0; i < validValuesLength; i++) {
                if (*field == *validValues[i]) {
                    return true;
                }
            }
            delay(500);
        }

    } else {
        Serial.printf("Previous timeout.  Skipping %s\n", queryString);
    }
    return false;
}
**/
bool checkSignalStrength() {
    long expiryTime = millis() + 120000;

    while (millis() < expiryTime) {
        timeout = false;
        send("AT+CSQ", "OK", 1000, &timeout);
        if (!timeout) {
            if (state.signalStrength > 20 && state.signalStrength < 99) {
                // Good signal
                return true;
            }
        }
        delay(500);
    }

    // Bad signal strength
    return false;
}

bool getIpAddress(bool *timeoutField);

void resetTcpStack() {
    timeout = false;
    send("AT+CIPSHUT", "SHUT OK", 2000, &timeout);
    send("AT+CIPMUX=0", "OK", 500, &timeout);
    send("AT+CIPRXGET=1", "OK", 500, &timeout);
    send(R"(AT+CSTT="globaldata.iot","","")", "OK", 30000, &timeout);

    // If CIICR fails, check APN, try AT_CGATT=0, AT+CGATT=1
    long expiryTime = millis() + 60000;
    while (millis() < expiryTime) {
        char *response = send("AT+CIICR", "OK", 30000, &timeout);
        if (response && strstr(response, "ERROR")) {
            send("AT+CGATT=0", "OK", 30000, &timeout);
            send("AT+CGATT=1", "OK", 30000, &timeout);
        } else {
            Serial.printf("CIICR OK? -->'%s'\n", response);
            getIpAddress(&timeout);
            break;
        }
    }
}


bool getIpAddress(bool *timeoutField) {
    *timeoutField = false;

    char* ipAddress = send("AT+CIFSR", "", 2000, timeoutField);

    if (*timeoutField) {
        Serial.println("Failed to get IP address - timing out");
        return false;
    }

    // Check if ERROR
    if (strstr(ipAddress, "ERROR") != nullptr) {
        resetTcpStack();

        *timeoutField = false;
        ipAddress = send("AT+CIFSR", "", 1000, timeoutField);
        if (ipAddress && strstr(ipAddress, "ERROR")) {
            // 2nd attempt - fail
            return false;
        }
    }

    // Copy IP address to state
    strncpy(state.ipAddress, ipAddress, sizeof(state.ipAddress));
    Serial.printf("IP Address: '%s'\n", state.ipAddress);
    return true;

}

void pollForNetworkOnline() {
    uint32_t regExpireTime = millis() + 120000;

    if (!timeout) {
        Serial.println("Polling for network to become online");
        while (millis() < regExpireTime) {
            timeout = false;
            send("AT+CPSI?", "OK", 500, &timeout);
            if (state.cpsiData.online ) {
                // Get the network time
                send("AT+CCLK?", "OK", 500, &timeout);
                return;
            }
            #ifdef SIM7000E
            // Fix the network settings, and go again
            send("AT+CNMP=38", "OK", 500, &timeout);
            send("AT+CNMP=13", "OK", 500, &timeout);
            send("AT+CPSI?", "OK", 500, &timeout);
            #endif
        }
    } else {
        Serial.printf("Skipping %s\n", " - no network?");
    }
}

#ifdef SIM7000E
bool connectToNetwork() {

    timeout = false;

    send("ATE0", "OK", 500, &timeout);
    send("ATE0", "OK", 500, &timeout);

    Serial.println("INITIAL STATE==========================================================================");
    timeout = false;


    send("AT+CGDCONT?", "OK", 500, &timeout);
    Serial.println("Connecting to network" );

    send("AT+COPS=0", "OK", 75000, &timeout);
    send("at+CNMP=38", "OK", 1000, &timeout);
    send("at+CMNB=1", "OK", 1000, &timeout);
    send(R"(AT+CGDCONT=1,"IP","globaldata.iot")", "OK", 1000, &timeout);


    send("AT+CEREG=2", "OK", 120000, &timeout);
    if (!timeout) {
        pollForStateChangeBool("AT+CEREG?", &state.ceregData.registered);
    }

    if (!timeout) {
        if (!checkSignalStrength()) {
            timeout = true;
            Serial.println("Failed to get signal - timing out");
        }
    }

    if (!timeout) {
        pollForNetworkOnline();
    }

    /**
        AT+CIPMUX=0    # Set single connection mode
        AT+CIPRXGET=1  # Enable manual data reading
        AT+CSTT="globaldata.iot","",""  # Set APN (Only needed if PDP fails)
        AT+CIICR        # Bring up wireless connection
        AT+CIFSR        # Get IP address
        AT+CIPSTART="TCP","139.59.180.17",8080  # Start TCP session

        👉 If *CIPSTART fails, ensure **APN is correct* and PDP is active (AT+CGPADDR should return an IP).
     */

    timeout=false;

    send(R"(AT+CSTT="globaldata.iot","","")", "OK", 30000, &timeout);

    // Get local IP address
    if (!getIpAddress(&timeout)) {
        Serial.println("Failed to get IP address - timing out");
        return true;
    }

    Serial.println("NETWORK SETUP COMPLETE ====================================================================");

    return !timeout;
}

bool connect(const char* ipAddress, int port) {
    static const char connectString[] = R"(at+cipstart="TCP","%s",%d)";
    int retries = 3;

    while (retries-- > 0) {
        char connectCommand[100] = {0};
        // create at+cipstart string using ip address and port in connectString
        snprintf(connectCommand, sizeof(connectCommand), connectString, ipAddress, port);

        char * response = send(connectCommand, "CONNECT OK", 10000, &timeout);

        if (strstr(response, "ERROR")) {
            Serial.println("Failed to connect to server - retrying");
            resetTcpStack();
            continue;

        } else {
            return true;
        }
    }
    return false;
}

const char* saltMsg = "black box tracker";

bool sendUpdate(char* message) {
    if (!state.ipConnected) {
        Serial.println("NOT CONNECTED TO SERVER!");
        return false;
    }

    Serial2.println("AT+CIPSEND");
    unsigned long timeoutTimeMs = millis() + 5000;
    while (Serial2.read() != '>') {
        if (millis() > timeoutTimeMs) {
            Serial.println("Failed to get '>' marker");
            return false;
        }
        delay(10);
    }

    salt(sendBuffer, message, saltMsg);

    // Need to wait for Modem to respond with ">"
    Serial.printf(">>> SENDING: %s\n", sendBuffer);
    Serial2.printf("%s\n\x1A", sendBuffer);
    Serial2.flush();

    return true;
}

bool close() {
    if (!state.ipConnected) {
        Serial.println("NOT CONNECTED TO SERVER!");
        return false;
    }
    timeout = false;
    delay(1000);
    send("AT+CIPCLOSE", "CLOSE OK", 5000, &timeout);

    return true;
}
#endif

//#define SIM7080G
#ifdef SIM7080G
bool connectToNetwork() {

    timeout = false;

    sendAndWaitForResponse("ATE0", "OK", 1000, &timeout);
    sendAndWaitForResponse("ATE0", "OK", 1000, &timeout);

    Serial.println("INITIAL STATE==========================================================================");
    timeout = false;
    // Power off radio
    sendAndWaitForResponse("AT+CFUN=0", "OK", 1000, &timeout);
    // Set the IP context and APN
    sendAndWaitForResponse(R"(AT+CGDCONT=1,"IP","globaldata.iot")","OK", 10000, &timeout);
    // Power back on
    sendAndWaitForResponse("AT+CFUN=1", "OK", 1000, &timeout);
    // TODO - Wait for state to become 1.
    // Check if attached
    sendAndWaitForResponse("AT+CGATT?", "OK", 1000, &timeout);
    // Query the APN delivered byt the network after registration
    sendAndWaitForResponse("AT+CGNAPN", "OK", 1000, &timeout);
    // Configure APN for Context 0
    sendAndWaitForResponse(R"(AT+CNCFG=0,1,"globaldata.iot")", "OK", 1000, &timeout);
    // Activate Context 0
    sendAndWaitForResponse("AT+CNACT=0,1", "OK", 1000, &timeout);
    // Get local IP
    sendAndWaitForResponse("AT+CNACT?", "OK", 1000, &timeout);
    // Turn off SSL
    sendAndWaitForResponse(R"(AT+CASSLCFG=0,"SSL",0")", "OK", 1000, &timeout);

    Serial.println("Connecting to network" );

    if (!timeout) {
        if (!checkSignalStrength()) {
            timeout = true;
            Serial.println("Failed to get signal - timing out");
        }
    }

    if (!timeout) {
        pollForNetworkOnline();
    }

    // Needed?
    sendAndWaitForResponse("AT+CEREG=1", "OK", 120000, &timeout);
    if (!timeout) {
        pollForStateChangeBool((char*)"AT+CEREG?", &state.registeredCereg);
    }
    timeout=false;
    Serial.println("NETWORK SETUP COMPLETE ====================================================================");
    return true;
}


void hexDump(const char *msgBuffer, int msgLen) {
    for (int i = 0; i < msgLen; i += 16) {
        Serial.printf("%08X  ", i); // Print the offset

        // Print hex values
        for (int j = 0; j < 16; j++) {
            if (i + j < msgLen) {
                Serial.printf("%02X ", msgBuffer[i + j]);
            } else {
                Serial.print("   ");
            }
        }
        Serial.print("  ");
        // Print ASCII values
        for (int j = 0; j < 16; j++) {
            if (i + j < msgLen) {
                char c = msgBuffer[i + j];
                if (c < 32 || c > 126) {
                    c = '.';
                }
                Serial.print(c);
            }
        }
        Serial.println();
    }

    Serial.println();
}

bool sendSim7080Message(char* message) {
    //Define the message to send
    char msgBuffer[126];
    memset(msgBuffer, 42, sizeof(msgBuffer)); // Fill with '*' characters
    msgBuffer[sizeof(msgBuffer) - 1] = '\0'; // Null terminate

    sprintf(msgBuffer, "SIM7080G: %s %lu\r\n", message, millis());
    int msgLen = strlen(msgBuffer);

    // Prepare to send...
    char sendCommandBuffer[20] = {0};
    sprintf(sendCommandBuffer, "AT+CASEND=0,%d", msgLen);

    // Show what's being sent
    Serial.print(sendCommandBuffer);
    Serial.printf("> [%d] %s", msgLen, msgBuffer);

    Serial.println("Command:");
    hexDump(sendCommandBuffer, strlen(sendCommandBuffer));
    Serial.println("Message:");
    hexDump(msgBuffer, msgLen);


    // Send it
    Serial2.println(sendCommandBuffer);
    Serial2.flush();

    // Wait for '>'
    int chr = 0;
    long start = millis();
    long markerTimeout = start + 5000;

    char bucketBuffer[20] = {0};

    // Timeout = Baud Rate * Timeout in Seconds / 100
/*
    while (chr != '>' && millis() < markerTimeout) {

        if (Serial2.available()) {
            chr = Serial2.read();
        }
        delay(10);
    }

    if (millis() > markerTimeout) {
        Serial.println("Timeout waiting for '>' marker");
        return false;
    }

*/

    //Serial2.setRxTimeout( (MODEM_BAUD_RATE / 11) *5  );
    int bytesReceived = Serial2.readBytesUntil('>', bucketBuffer, 20);
    if (bytesReceived == 0) {
        Serial.println("Failed to get '>' marker");
        return false;
    }

    if (!strchr(bucketBuffer, chr)) {
        Serial.println("Failed to get '>' marker");
        return false;
    }

    Serial.printf(">>> SENDING: '%s'\n" , msgBuffer);
    hexDump(msgBuffer, msgLen);

    Serial2.print(msgBuffer);
    Serial2.flush();

    delay(3000);
    Serial.println("One more CRLF??");
    Serial2.println();
    delay(3000);

    sendAndWaitForResponse("AT+CASTATE?", "OK", 5000, &timeout);

    timeout = false;
    sendAndWaitForResponse("AT+CACLOSE=0", "OK", 5000, &timeout);

    sendAndWaitForResponse("AT+CASTATE?", "OK", 5000, &timeout);

    Serial.println(R"(AT+CAOPEN=0,0,"TCP","139.59.180.17",8080)");

    return true;
}

bool sendUpdate() {
    int retries = 3;

    while (retries-- > 0) {
        // Connect to Data Collector
        char * response = sendAndWaitForResponse(R"(AT+CAOPEN=0,0,"TCP","139.59.180.17",8080)", "OK", 1000, &timeout);

        if (strstr(response, "ERROR")) {
            Serial.println("Failed to connect to server - retrying");
            resetTcpStack();
            continue;

        } else {
            break;
        }
    }

    if (!state.ipConnected) {
        Serial.println("Failed to connect to server");
        return false;
    }

    return sendSim7080Message("Hello");

}
#endif


#pragma clang diagnostic pop