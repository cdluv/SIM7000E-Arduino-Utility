//
// Created by Gary on 09/02/2025.
//

#ifndef SIM7000E_ATCOMMANDHANDLER_H
#define SIM7000E_ATCOMMANDHANDLER_H
#include <ModemReactor.h>

void setup_commandHandler(HardwareSerial *serial, ModemState *modemState_p, bool *timeout);
void onReceived();      // Callback for serial data

char* send(const char* command, const char* expectedResponse, unsigned long responseTimeoutMs, bool* responseTimeout);
bool connectToNetwork();
bool connect(const char* ipAddress, int port);
bool sendUpdate(char * message);
bool close();


#endif //SIM7000E_ATCOMMANDHANDLER_H
