//
// Created by Maty Martan on 11.01.2026.
//

#include "nfc.h"
#include "main.h"
#include "nfc_test.h"
#include "spi.h"
#include "utils.h"
#include "rfal_nfca.h"

extern SPI_HandleTypeDef hspi2;

void nfc_init(void)
{
    debug_log("initializing NFC..." nl);

    // perform a simple SPI ping test to verify that we can read from the NFC chip
    run_nfc_tests();
    debug_log("NFC initialized" nl);
}


