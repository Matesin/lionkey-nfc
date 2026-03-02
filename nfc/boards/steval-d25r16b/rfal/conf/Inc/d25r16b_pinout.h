//
// Created by Maty Martan on 02.03.2026.
//

#ifndef LIONKEY_PINOUT_H
#define LIONKEY_PINOUT_H

#include "stm32h5xx_hal.h"

#define ST25R_SS_PORT       GPIOB
#define ST25R_SS_PIN        GPIO_PIN_1
// TODO: replace with actual IRQn
#define IRQ_ST25R_EXTI_IRQn EXTI2_IRQn
#define ST25R_INT_PORT      GPIOB
#define ST25R_INT_PIN       GPIO_PIN_2

#endif //LIONKEY_PINOUT_H