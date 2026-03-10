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
#include "utils.h"
#include "spi.h"

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
#define platformProtectST25RComm()                do{ globalCommProtectCnt++;                   \
                                                          __DSB();                              \
                                                          NVIC_DisableIRQ(IRQ_ST25R_EXTI_IRQn); \
                                                          __DSB();                              \
                                                          __ISB();                              \
                                                        }while(0)                                   /*!< Protect unique access to ST25R communication channel - IRQ disable on single thread environment (MCU) ; Mutex lock on a multi thread environment      */
#define platformUnprotectST25RComm()              do{if (--globalCommProtectCnt == 0U)            \
                                                          {                                       \
                                                            NVIC_EnableIRQ(IRQ_ST25R_EXTI_IRQn);  \
                                                          }                                       \
                                                        }while(0)                                   /*!< Unprotect unique access to ST25R communication channel - IRQ enable on a single thread environment (MCU) ; Mutex unlock on a multi thread environment */

#define platformProtectST25RIrqStatus()           platformProtectST25RComm()                /*!< Protect unique access to IRQ status var - IRQ disable on single thread environment (MCU) ; Mutex lock on a multi thread environment */
#define platformUnprotectST25RIrqStatus()         platformUnprotectST25RComm()              /*!< Unprotect the IRQ status var - IRQ enable on a single thread environment (MCU) ; Mutex unlock on a multi thread environment         */

#define platformProtectWorker()                                                                     /* Protect RFAL Worker/Task/Process from concurrent execution on multi thread platforms   */
#define platformUnprotectWorker()                                                                   /* Unprotect RFAL Worker/Task/Process from concurrent execution on multi thread platforms */

#define platformIrqST25RSetCallback( cb )
#define platformIrqST25RPinInitialize()

#define platformIrqST25R3916Enable()   HAL_NVIC_EnableIRQ(ST25R_EXTI_IRQn)
#define platformIrqST25R3916Disable()  HAL_NVIC_DisableIRQ(ST25R_EXTI_IRQn)

#define platformLedsInitialize()                                                                    /*!< Initializes the pins used as LEDs to outputs*/

#define platformLedOff( port, pin )                   platformGpioClear(port, pin)                  /*!< Turns the given LED Off                     */
#define platformLedOn( port, pin )                    platformGpioSet(port, pin)                    /*!< Turns the given LED On                      */
#define platformLedToggle( port, pin )                platformGpioToogle(port, pin)                 /*!< Toogle the given LED                        */

#define platformGpioSet( port, pin )                  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET)    /*!< Turns the given GPIO High                   */
#define platformGpioClear( port, pin )                HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET)  /*!< Turns the given GPIO Low                    */
#define platformGpioToggle( port, pin )               HAL_GPIO_TogglePin(port, pin)                 /*!< Toggles the given GPIO                      */
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
#define platformSpiTxRx( txBuf, rxBuf, len )          NFC_SPI_SendRcv(txBuf, rxBuf, len)            /*!< SPI Transmit and receive data               */

#define platformLog(...)                              debug_log(__VA_ARGS__)                         /*!< Log  method                                 */
/* Exported functions ------------------------------------------------------- */

/* Private macro -------------------------------------------------------------*/

/*! Buffer size configuration for ISO-DEP and NFC-DEP (bytes) */
#define RFAL_FEATURE_ISO_DEP_IBLOCK_MAX_LEN    256U
#define RFAL_FEATURE_NFC_DEP_BLOCK_MAX_LEN     254U
#define RFAL_FEATURE_NFC_RF_BUF_LEN            258U

#define RFAL_FEATURE_ISO_DEP_APDU_MAX_LEN      512U       /*!< ISO-DEP APDU max length. Please use multiples of I-Block max length       */
#define RFAL_FEATURE_NFC_DEP_PDU_MAX_LEN       512U       /*!< NFC-DEP PDU max length.                                                   */

/* NFC technology feature switches */
/*! Only allow NFC-A because it fits our use case */
#define RFAL_FEATURE_NFCA                      true
#define RFAL_FEATURE_NFCE                      false
#define RFAL_FEATURE_NFCB                      false
#define RFAL_FEATURE_NFCF                      false
#define RFAL_FEATURE_NFCV                      false
#define RFAL_FEATURE_ST25TB                    false
#define RFAL_FEATURE_ST25xV                    false
/* Allow listen mode */
#define RFAL_FEATURE_LISTEN_MODE true

/* Tag type support */
#define RFAL_FEATURE_T1T false
#define RFAL_FEATURE_T2T false
#define RFAL_FEATURE_T4T true

/* Tag behavior */
#define RFAL_FEATURE_LISTEN_MODE               true
/* TODO: toggle this once the tag's behavior is stable */
#define RFAL_FEATURE_WAKEUP_MODE               false
#define RFAL_FEATURE_LOWPOWER_MODE             false

#define RFAL_FEATURE_DYNAMIC_ANALOG_CONFIG     false
#define RFAL_FEATURE_DPO                       false
/* DEP setup */
#define RFAL_FEATURE_NFC_DEP                   false
#define RFAL_FEATURE_ISO_DEP                   true
#define RFAL_FEATURE_ISO_DEP_POLL              false
#define RFAL_FEATURE_ISO_DEP_LISTEN            true


#endif //LIONKEY_RFAL_PLATFORM_H