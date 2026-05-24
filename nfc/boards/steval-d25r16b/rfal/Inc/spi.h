//
// Created by Maty Martan on 25.02.2026.
//

#ifndef LIONKEY_SPI_H
#define LIONKEY_SPI_H
#include <stddef.h>
#include <stdint.h>
#include "rfal_platform.h"

#define ST25R_READ_REG(addr)    (0x40u | (addr & 0x3Fu))
#define ST25R_WRITE_REG(addr)  (addr & 0x3Fu)

#define COMM_HANDLE hspi2

int32_t NFC_SPI_SendRcv (uint8_t *pTxData, uint8_t *pRxData, size_t dataSize);

uint8_t spi_read_reg(uint8_t address);
uint8_t st25_read_reg(uint8_t address);
void spi_write_reg(uint8_t address, uint8_t value);
void st25_write_reg(uint8_t address, uint8_t value);


#endif //LIONKEY_SPI_H