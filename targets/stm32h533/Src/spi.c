//
// Created by Maty Martan on 25.02.2026.
//
#include "spi.h"
#include "main.h"

extern SPI_HandleTypeDef hspi2;

static void spi_put_cs_low(void) { HAL_GPIO_WritePin(ST25_CS_GPIO_Port, ST25_CS_Pin, GPIO_PIN_RESET); }
static void spi_put_cs_high(void) { HAL_GPIO_WritePin(ST25_CS_GPIO_Port, ST25_CS_Pin, GPIO_PIN_SET); }

uint8_t spi_read_reg(uint8_t addr)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = addr;
    tx[1] = 0x00;

    spi_put_cs_low();

    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 100);

    spi_put_cs_high();

    return rx[1];
}
uint8_t st25_read_reg(uint8_t address) {
    return spi_read_reg(ST25R_READ_REG(address));
}

void st25_write_reg(uint8_t address, uint8_t value) {
    spi_write_reg(ST25R_WRITE_REG(address), value);
}

