//
// Created by Maty Martan on 25.02.2026.
//

#ifndef LIONKEY_NFC_TEST_H
#define LIONKEY_NFC_TEST_H
#include <stdbool.h>

void run_nfc_tests(void);

bool nfc_init_test(void);
void spi_loopback_test(void);
void spi_ping_nfc_test(void);

#endif //LIONKEY_NFC_TEST_H