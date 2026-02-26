//
// Created by Maty Martan on 25.02.2026.
//

#include "nfc_test.h"

#include <string.h>

#include "utils.h"
#include "main.h"
#include "spi.h"

void spi_loopback_test(void) {
    debug_log("starting SPI loopback test..." nl);

    uint8_t ret = 0;
    while (1)
    {
        for (int i = 0; i < 100; i++) {
            ret = spi_read_reg(ST25R_READ_REG(i));
            debug_log("read register: 0x40%2X, returned %2X" nl, i, ret);
            HAL_Delay(500);
        }
    }

}

void spi_ping_nfc_test(void) {
    uint8_t id = st25_read_reg(0x3F); // IC identity register address per errata :contentReference[oaicite:8]{index=8}
    debug_log("ST25R3916 IC ID: 0x%02X" nl, id);
}
