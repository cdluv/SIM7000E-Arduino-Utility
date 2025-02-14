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

ModemState * modemState;          // Various states of the Modem
boolean *modemTimeout;         // Timeouts are global so that they can be accessed from the callback functions
QueueHandle_t appQueue;
HardwareSerial *modemSerial;

char lineBuffer[BUFFER_SIZE];   // Global buffer to hold the incoming line
char sendBuffer[BUFFER_SIZE];   // Global buffer to hold the outgoing line

char* readLineWithTimeout(unsigned long timeoutMs, bool* timeoutOccurred);
void receiveTask(void *pvParameters);



void setup_commandHandler(HardwareSerial* serial, ModemState *modemState_p, bool *timeout) {
    assert(serial != nullptr);
    assert(modemState_p != nullptr);
    assert(timeout != nullptr);

    memset(lineBuffer, 0, BUFFER_SIZE);
    memset(sendBuffer, 0, BUFFER_SIZE);

    modemSerial = serial;
    modemState = modemState_p;
    modemTimeout = timeout;

    //serial->onReceive(onReceived, false);


    appQueue = xQueueCreate(2, BUFFER_SIZE);

    if (appQueue == nullptr) {
        // Queue was not created and must not be used.
        Serial.println("Queue creation failed!");
    } else {
        Serial.println("Queue created successfully!");
    }

    xTaskCreatePinnedToCore(receiveTask, "ReceiveTask", 2048, nullptr, 1, nullptr, 1);

}

// Internal function to send a message to the queue - called by the onReceive hook
void sendToQueue(const char* message) {
    if (xQueueSend(appQueue, (void*)message, pdMS_TO_TICKS(100)) != pdPASS) {
        Serial.printf("Failed to send '%s' to queue %x\n", message);
    } else {
        Serial.printf("Message '%s' sent to queue!\n", message);
    }
}

char receivedData[BUFFER_SIZE] = {0};
int receivedIndex = 0;

void onReceived() {
    char c;

    while (modemSerial->available() > 0 && receivedIndex < BUFFER_SIZE - 1) {
        c = modemSerial->read();
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            receivedData[receivedIndex++]= '\0';
            break;
        }
        receivedData[receivedIndex++] = c;
    }

    if (receivedIndex == BUFFER_SIZE - 1) {
        receivedIndex = 0;
        receivedData[0] = '\0';
        // FIXME?  Nothing to worry about here - phone shouldn't generate a buffer full of data that isn't
        //         newline terminated.  Even if it does, we'll throw away the full buffer and start over.

        Serial.println("Buffer overflow in onReceived handler - ignoring data");
        return;
    }

    if ( c != '\n') {
        // Not enough data to proceed
        return;
    }

    if (receivedIndex > 0) {
        //char* allocatedData = (char*)malloc((receivedIndex + 1) * sizeof(char));

        //strcpy(allocatedData, receivedData);
        if (checkAndHandleUnsolicitedResponses(receivedData, modemState)) {
            //free(allocatedData);
        } else {
            sendToQueue(receivedData);
            Serial.printf("Received Data: %s\n", receivedData);
        }
    }
    receivedIndex = 0;
    receivedData[0] = '\0';
}

void receiveTask(void *pvParameters) {
    while (true) {
        onReceived();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


char* send(const char* command, const char* expectedResponse, unsigned long responseTimeoutMs = TIMEOUT_MS, bool* responseTimeout = nullptr) {
    if (responseTimeout != nullptr && *responseTimeout) {
        return nullptr;
    }
    //Serial.printf("> %s\n", command);

    // Send the command
    modemSerial->println(command);

    if (responseTimeout) {
        *responseTimeout = false;
    }

    while (true) {
        char* lineBuffer = readLineWithTimeout(responseTimeoutMs, responseTimeout);
        if (lineBuffer == nullptr || (responseTimeout && *responseTimeout)) {
            Serial.printf(" Timeout - %s.\n", responseTimeout ? "true" : "false");
            return lineBuffer;
        }

        // check if lineBuffer is empty
        if (lineBuffer[0] == '\0') {
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

    char* receivedMessage;

    if (xQueueReceive(appQueue, &receivedMessage, pdMS_TO_TICKS(responseTimeoutMs) ) != pdPASS) {
        // No message in the queue
        *timeoutOccurred = true;
        return nullptr;
    }


    Serial.printf("Processing message from queue: %s\n", receivedMessage);
    // copy receivedMessage to lineBuffer
    int len =  strlen(receivedMessage);
    strncpy(lineBuffer, receivedMessage, len);
    //lineBuffer[len] = '\0';
    Serial.println("1");
    return lineBuffer;
}

// Relies on checkAndHandleUnsolicitedResponses to change the state of "registeredField"
// This happens automatically, when "sendAndWaitForResponse" is called.
void pollForStateChangeBool(const char * queryString, const bool* registeredField) {

    if (! *modemTimeout) {
        uint32_t regExpireTime = millis() + 120000;

        while (millis() < regExpireTime && !*registeredField) {
            *modemTimeout = false;
            send(queryString, "OK", 2000, modemTimeout);
            delay(250);
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
        *modemTimeout = false;
        send("AT+CSQ", "OK", 1000, modemTimeout);
        if (!*modemTimeout) {
            if (modemState->signalStrength > 20 && modemState->signalStrength < 99) {
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
    *modemTimeout = false;
    send("AT+CIPSHUT", "SHUT OK", 2000, modemTimeout);
    send("AT+CIPMUX=0", "OK", 500, modemTimeout);
    send("AT+CIPRXGET=1", "OK", 500, modemTimeout);
    send(R"(AT+CSTT="globaldata.iot","","")", "OK", 30000, modemTimeout);

    // If CIICR fails, check APN, try AT_CGATT=0, AT+CGATT=1
    long expiryTime = millis() + 60000;
    while (millis() < expiryTime) {
        char *response = send("AT+CIICR", "OK", 30000, modemTimeout);
        if (response && strstr(response, "ERROR")) {
            send("AT+CGATT=0", "OK", 30000, modemTimeout);
            send("AT+CGATT=1", "OK", 30000, modemTimeout);
        } else {
            Serial.printf("CIICR OK? -->'%s'\n", response);
            getIpAddress(modemTimeout);
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
    strncpy(modemState->ipAddress, ipAddress, sizeof(modemState->ipAddress));
    Serial.printf("IP Address: '%s'\n", modemState->ipAddress);
    return true;

}

void pollForNetworkOnline() {
    uint32_t regExpireTime = millis() + 120000;

    if (! *modemTimeout) {
        Serial.println("Polling for network to become online");
        while (millis() < regExpireTime) {
            *modemTimeout = false;
            send("AT+CPSI?", "OK", 500, modemTimeout);
            if (modemState->cpsiData.online ) {
                // Get the network time
                send("AT+CCLK?", "OK", 500, modemTimeout);
                return;
            }
            #ifdef SIM7000E
            // Fix the network settings, and go again
            send("AT+CNMP=38", "OK", 500, modemTimeout);
            send("AT+CNMP=13", "OK", 500, modemTimeout);
            send("AT+CPSI?", "OK", 500, modemTimeout);
            #endif
        }
    } else {
        Serial.printf("Skipping %s\n", " - no network?");
    }
}

#ifdef SIM7000E
bool connectToNetwork() {

    *modemTimeout = false;

    send("ATE0", "OK", 500, modemTimeout);
    send("ATE0", "OK", 500, modemTimeout);

    Serial.println("INITIAL STATE==========================================================================");
    *modemTimeout = false;


    send("AT+CGDCONT?", "OK", 500, modemTimeout);
    Serial.println("Connecting to network" );

    send("AT+COPS=0", "OK", 75000, modemTimeout);
    send("at+CNMP=38", "OK", 1000, modemTimeout);
    send("at+CMNB=1", "OK", 1000, modemTimeout);
    send(R"(AT+CGDCONT=1,"IP","globaldata.iot")", "OK", 1000, modemTimeout);


    send("AT+CEREG=2", "OK", 120000, modemTimeout);
    if (!*modemTimeout) {
        pollForStateChangeBool("AT+CEREG?", &modemState->ceregData.registered);
    }

    if (!*modemTimeout) {
        if (!checkSignalStrength()) {
            *modemTimeout = true;
            Serial.println("Failed to get signal - timing out");
        }
    }

    if (! *modemTimeout) {
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

    *modemTimeout=false;

    send(R"(AT+CSTT="globaldata.iot","","")", "OK", 30000, modemTimeout);

    // Get local IP address
    if (!getIpAddress(modemTimeout)) {
        Serial.println("Failed to get IP address - timing out");
        return true;
    }

    Serial.println("NETWORK SETUP COMPLETE ====================================================================");

    return !*modemTimeout;
}

bool connect(const char* ipAddress, int port) {
    static const char connectString[] = R"(at+cipstart="TCP","%s",%d)";
    int retries = 3;

    while (retries-- > 0) {
        char connectCommand[100] = {0};
        // create at+cipstart string using ip address and port in connectString
        snprintf(connectCommand, sizeof(connectCommand), connectString, ipAddress, port);

        char * response = send(connectCommand, "CONNECT OK", 10000, modemTimeout);

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


bool prepareToSend() {
    modemSerial->println("AT+CIPSEND");

    unsigned long timeoutTimeMs = millis() + 5000;

    while (modemSerial->read() != '>') {
        if (millis() > timeoutTimeMs) {
            Serial.println("Failed to get '>' marker");
            return false;
        }
        delay(10);
    }
    return true;
}

const char* saltMsg = "black box tracker";

bool sendUpdate(char* message) {
    if (!modemState->ipConnected) {
        Serial.println("NOT CONNECTED TO SERVER!");
        return false;
    }

    prepareToSend();

    salt(sendBuffer, message, saltMsg);

    // Need to wait for Modem to respond with ">"
    Serial.printf(">>> SENDING: %s\n", sendBuffer);
    modemSerial->printf("%s\n\x1A", sendBuffer);
    modemSerial->flush();

    return true;
}

bool close() {
    if (!modemState->ipConnected) {
        Serial.println("NOT CONNECTED TO SERVER!");
        return false;
    }
    *modemTimeout = false;
    delay(1000);
    send("AT+CIPCLOSE", "CLOSE OK", 5000, modemTimeout);

    return true;
}
#endif

//#define SIM7080G
#ifdef SIM7080G
bool connectToNetwork() {

    timeout = false;

    sendAndWaitForResponse("ATE0", "OK", 1000, modemTimeout);
    sendAndWaitForResponse("ATE0", "OK", 1000, modemTimeout);

    Serial.println("INITIAL STATE==========================================================================");
    timeout = false;
    // Power off radio
    sendAndWaitForResponse("AT+CFUN=0", "OK", 1000, modemTimeout);
    // Set the IP context and APN
    sendAndWaitForResponse(R"(AT+CGDCONT=1,"IP","globaldata.iot")","OK", 10000, modemTimeout);
    // Power back on
    sendAndWaitForResponse("AT+CFUN=1", "OK", 1000, modemTimeout);
    // TODO - Wait for state to become 1.
    // Check if attached
    sendAndWaitForResponse("AT+CGATT?", "OK", 1000, modemTimeout);
    // Query the APN delivered byt the network after registration
    sendAndWaitForResponse("AT+CGNAPN", "OK", 1000, modemTimeout);
    // Configure APN for Context 0
    sendAndWaitForResponse(R"(AT+CNCFG=0,1,"globaldata.iot")", "OK", 1000, modemTimeout);
    // Activate Context 0
    sendAndWaitForResponse("AT+CNACT=0,1", "OK", 1000, modemTimeout);
    // Get local IP
    sendAndWaitForResponse("AT+CNACT?", "OK", 1000, modemTimeout);
    // Turn off SSL
    sendAndWaitForResponse(R"(AT+CASSLCFG=0,"SSL",0")", "OK", 1000, modemTimeout);

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
    sendAndWaitForResponse("AT+CEREG=1", "OK", 120000, modemTimeout);
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

    sendAndWaitForResponse("AT+CASTATE?", "OK", 5000, modemTimeout);

    timeout = false;
    sendAndWaitForResponse("AT+CACLOSE=0", "OK", 5000, modemTimeout);

    sendAndWaitForResponse("AT+CASTATE?", "OK", 5000, modemTimeout);

    Serial.println(R"(AT+CAOPEN=0,0,"TCP","139.59.180.17",8080)");

    return true;
}

bool sendUpdate() {
    int retries = 3;

    while (retries-- > 0) {
        // Connect to Data Collector
        char * response = sendAndWaitForResponse(R"(AT+CAOPEN=0,0,"TCP","139.59.180.17",8080)", "OK", 1000, modemTimeout);

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