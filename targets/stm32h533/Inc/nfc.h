//
// Created by Maty Martan on 11.01.2026.
//

#ifndef LIONKEY_NFC_H
#define LIONKEY_NFC_H
#include "rfal_nfc.h"

/*! Macros ------------------------------------------------------------*/
#define NDEF_SIZE           2048      /*!< Max NDEF size emulated. Range: 0005h - 7FFFh    */
#define T4T_CLA_00          0x00      /*!< CLA value for type 4 command                    */
#define T4T_INS_SELECT      0xA4      /*!< INS value for select command                    */
#define T4T_INS_READ        0xB0      /*!< INS value for reabbinary command                */
#define T4T_INS_UPDATE      0xD6      /*!< INS value for update command                    */
#define FID_CC              0xE103    /*!< File ID number for CCFile                       */
#define FID_NDEF            0x0001    /*!< File ID number for NDEF file                    */

/*! Enums -------------------------------------------------------------*/

enum States
{
    STATE_IDLE                      = 0,    /*!< Emulated Tag state idle                  */
    STATE_APP_SELECTED              = 1,    /*!< Emulated Tag state application selected  */
    STATE_CC_SELECTED               = 2,    /*!< Emulated Tag state CCFile selected       */
    STATE_FID_SELECTED              = 3,    /*!< Emulated Tag state FileID selected       */
};

/*! Funtion prototypes -------------------------------------------------------------*/
void nfc_init(void);
void nfc_notify(rfalNfcState st);
void nfc_task(void);
uint16_t nfc_parse_and_respond(uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf, uint16_t txBufLen );
uint16_t nfc_select_response(uint8_t *cmdData, uint8_t *rspData );

#endif //LIONKEY_NFC_H