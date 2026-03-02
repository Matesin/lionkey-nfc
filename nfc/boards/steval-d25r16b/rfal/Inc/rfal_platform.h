//
// Created by Maty Martan on 26.02.2026.
//

#ifndef LIONKEY_RFAL_PLATFORM_H
#define LIONKEY_RFAL_PLATFORM_H

#include "stm32h5xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "main.h"
#include "d25r16b_pinout.h"
#include "nfc_conf.h"
#include "timer.h"

extern volatile uint32_t globalCommProtectCnt;

#ifdef __cplusplus
extern "C" {
}
#endif

/** @addtogroup X-CUBE-NFC6_Applications
 *  @{
 */

/** @addtogroup PollingTagDetect
 *  @{
 */

/** @defgroup PTD_Platform
 *  @brief Demo functions containing the example code
 * @{
 */

/* Exported types ------------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/


/** @defgroup PTD_Platform_Exported_Macro
 *  @{
 */
#define platformProtectST25RComm()                do{ globalCommProtectCnt++;                  \
                                                          __DSB();NVIC_DisableIRQ(IRQ_ST25R_EXTI_IRQn); \
                                                          __DSB();                             \
                                                          __ISB();                             \
                                                        }while(0)                                   /*!< Protect unique access to ST25R communication channel - IRQ disable on single thread environment (MCU) ; Mutex lock on a multi thread environment      */
#define platformUnprotectST25RComm()              do{ globalCommProtectCnt--;             \
                                                          if (globalCommProtectCnt == 0U) \
                                                          {                               \
                                                            NVIC_EnableIRQ(IRQ_ST25R_EXTI_IRQn);   \
                                                          }                               \
                                                        }while(0)                                   /*!< Unprotect unique access to ST25R communication channel - IRQ enable on a single thread environment (MCU) ; Mutex unlock on a multi thread environment */

#define platformProtectST25RIrqStatus()           platformProtectST25RComm()                /*!< Protect unique access to IRQ status var - IRQ disable on single thread environment (MCU) ; Mutex lock on a multi thread environment */
#define platformUnprotectST25RIrqStatus()         platformUnprotectST25RComm()              /*!< Unprotect the IRQ status var - IRQ enable on a single thread environment (MCU) ; Mutex unlock on a multi thread environment         */

#define platformProtectWorker()                                                                     /* Protect RFAL Worker/Task/Process from concurrent execution on multi thread platforms   */
#define platformUnprotectWorker()                                                                   /* Unprotect RFAL Worker/Task/Process from concurrent execution on multi thread platforms */

#define platformIrqST25RSetCallback( cb )
#define platformIrqST25RPinInitialize()

#define platformLedsInitialize()                                                                    /*!< Initializes the pins used as LEDs to outputs*/

#define platformLedOff( port, pin )                   platformGpioClear(port, pin)                  /*!< Turns the given LED Off                     */
#define platformLedOn( port, pin )                    platformGpioSet(port, pin)                    /*!< Turns the given LED On                      */
#define platformLedToogle( port, pin )                platformGpioToogle(port, pin)                 /*!< Toogle the given LED                        */

#define platformGpioSet( port, pin )                  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET)    /*!< Turns the given GPIO High                   */
#define platformGpioClear( port, pin )                HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET)  /*!< Turns the given GPIO Low                    */
#define platformGpioToogle( port, pin )               HAL_GPIO_TogglePin(port, pin)                 /*!< Toogles the given GPIO                      */
#define platformGpioIsHigh( port, pin )               (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) /*!< Checks if the given LED is High             */
#define platformGpioIsLow( port, pin )                (!platformGpioIsHigh(port, pin))              /*!< Checks if the given LED is Low              */

#define platformTimerCreate( t )                      timerCalculateTimer(t)                        /*!< Create a timer with the given time (ms)     */
#define platformTimerIsExpired( timer )               timerIsExpired(timer)                         /*!< Checks if the given timer is expired        */
#define platformTimerDestroy( timer )                                                               /*!< Stop and release the given timer            */
#define platformTimerGetRemaining(t)                  (timerGetRemaining(t))
#define platformDelay( t )                            HAL_Delay( t )                                /*!< Performs a delay for the given time (ms)    */

#define platformGetSysTick()                          HAL_GetTick()                                 /*!< Get System Tick ( 1 tick = 1 ms)            */

#define platformErrorHandle()                         Error_Handler()             /*!< Global error handler or trap                */

#define platformSpiSelect()                           platformGpioClear(ST25R_SS_PORT, ST25R_SS_PIN)/*!< SPI SS\CS: Chip|Slave Select                */
#define platformSpiDeselect()                         platformGpioSet(ST25R_SS_PORT, ST25R_SS_PIN)  /*!< SPI SS\CS: Chip|Slave Deselect              */
#define platformSpiTxRx( txBuf, rxBuf, len )          HAL_SPI_TransmitReceive(&hspi2, txBuf, rxBuf, len, 100) /*!< SPI Transmit and Receive                   */


#define platformLog(...)                              logUsart(__VA_ARGS__)                         /*!< Log  method                                 */

/* Exported functions ------------------------------------------------------- */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


#endif //LIONKEY_RFAL_PLATFORM_H