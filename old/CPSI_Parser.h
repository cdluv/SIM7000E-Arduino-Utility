//
// Created by Gary on 07/02/2025.
//

#ifndef SIM7000E_CPSI_PARSER_H
#define SIM7000E_CPSI_PARSER_H

struct CpsiData {
    char status[9];  // "ONLINE" or similar (max 8 characters + terminator)
    int  mobileCountryCode, mobileNetworkCode, locationAreaCode, rx;
    long cellId;
};

CpsiData* parse(char* resp);

#endif //SIM7000E_CPSI_PARSER_H
