//
// Created by Gary on 09/02/2025.
//

#include "strutil.h"
#include <mbedtls/sha256.h>
//#include <SHA256.h>


// char salt[] = "black box tracker" ??
void salt(char* result, char* message, const char* salt) {

    // Create SHA-256 hash of the message salted

    char saltedMessage[256];
    snprintf(saltedMessage, sizeof(saltedMessage), "%s%s", salt, message);

    mbedtls_sha256_context ctx;
    unsigned char hash[32];

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0); // 0 for SHA-256
    mbedtls_sha256_update_ret(&ctx, (const unsigned char*)saltedMessage, strlen(saltedMessage));
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    // Convert hash to hex string
    char hashString[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hashString + (i * 2), "%02x", hash[i]);
    }
    hashString[64] = 0;

    snprintf(result, 256, "%s:%s", hashString, message);
}
