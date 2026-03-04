/**
* @file d25r16b_pinout.h
 * @brief ST25R3916 pinout configuration for STEVAL-D25R16B board.
 *
 * This header defines board-specific macros for the ST25R3916 RF transceiver
 * connection (chip-select, interrupt pin and EXTI IRQn).
 *
 * Usage:
 *  - Include this file in modules that need to know which GPIO ports and pins
 *    the ST25R3916 is connected to.
 *  - To adapt to another board, update the macro values below and ensure the
 *    corresponding GPIO and EXTI initialization in the project reflects these.
 *
 * Macros:
 *  - ST25R_SS_PORT: GPIO port used for chip-select (SS).
 *  - ST25R_SS_PIN: GPIO pin used for chip-select.
 *  - IRQ_ST25R_EXTI_IRQn: EXTI IRQn number used for the ST25R interrupt line.
 *  - ST25R_INT_PORT: GPIO port for the ST25R interrupt output.
 *  - ST25R_INT_PIN: GPIO pin for the ST25R interrupt output.
 *
 * Notes:
 *  - `stm32h5xx_hal.h` is included to provide GPIO and IRQ definitions.
 */

#ifndef LIONKEY_PINOUT_H
#define LIONKEY_PINOUT_H

#ifdef __cplusplus
extern "C" {
#endif


#include "stm32h5xx_hal.h"

/* ST25R3916 pinout configuration for STEVAL-D25R16B board */

#define ST25R_SS_PORT       GPIOB
#define ST25R_SS_PIN        GPIO_PIN_1
#define IRQ_ST25R_EXTI_IRQn EXTI0_IRQn
#define ST25R_INT_PORT      GPIOB
#define ST25R_INT_PIN       GPIO_PIN_0

#ifdef __cplusplus
}
#endif

#endif //LIONKEY_PINOUT_H