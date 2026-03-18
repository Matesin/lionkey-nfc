//
// Created by Maty Martan on 11.01.2026.
//

#ifndef LIONKEY_NFC_H
#define LIONKEY_NFC_H

// TODO: Add doxygen

/*! Macros ------------------------------------------------------------*/
#define NDEF_SIZE           2048      /*!< Max NDEF size emulated. Range: 0005h - 7FFFh    */
#define T4T_CLA_00          0x00      /*!< CLA value for type 4 command                    */
#define T4T_INS_SELECT      0xA4      /*!< INS value for select command                    */
#define T4T_INS_READ        0xB0      /*!< INS value for reabbinary command                */
#define T4T_INS_UPDATE      0xD6      /*!< INS value for update command                    */
#define FID_CC              0xE103    /*!< File ID number for CCFile                       */
#define FID_NDEF            0x0001    /*!< File ID number for NDEF file                    */

#define APDU_WRONG_SIZE     1

/* Status Words */
#define NFC_SW_OK                        0x9000U
#define NFC_SW_FILE_NOT_FOUND            0x6A82U
#define NFC_SW_FUNC_NOT_SUPPORTED        0x6800U
#define NFC_SW_WRONG_LENGTH              0x6700U
#define NFC_SW_WRONG_PARAMS              0x6B00U
#define NFC_SW_COND_NOT_SATISFIED        0x6985U
#define NFC_SW_INS_NOT_SUPPORTED         0x6D00U
#define NFC_SW_CLA_NOT_SUPPORTED         0x6E00U
#define NFC_SW_NOT_ENOUGH_MEMORY         0x6F00U
#define NFC_SW_SECURITY_STATUS_NOT_SAT   0x6982U
#define NFC_SW_NOT_FOUND                 0x6A82U
#define NFC_SW_WRONG_DATA                0x6A80U
#define NFC_SW_INS_UNKNOWN               0x6D00U
#define NFC_SW_CLA_UNKNOWN               0x6E00U
#define NFC_SW_CONDITIONS                0x6985U
#define NFC_SW_FILE_SELECTED             0x6A84U

/* NFC states */
#define NFC_NOTINIT             0 /*! NFC state: not initialized */
#define NFC_START_DISCOVERY     1 /*!< NFC state: start discovery */
#define NFC_DISCOVERY           2 /*!< NFC state: discovery */
#define NFC_CE_ACTIVE           3 /*!< NFC state: CE active */

#define TX_BUF_SIZE             512
#include <stdbool.h>
#include <stdint.h>


/*! Enums -------------------------------------------------------------*/

enum States
{
    STATE_IDLE                      = 0,    /*!< Emulated Tag state idle                  */
    STATE_APP_SELECTED              = 1,    /*!< Emulated Tag state application selected  */
    STATE_CC_SELECTED               = 2,    /*!< Emulated Tag state CCFile selected       */
    STATE_FID_SELECTED              = 3,    /*!< Emulated Tag state FileID selected       */
};

enum Files
{
    FILE_NONE = 0,
    FILE_CC   = 1,
    FILE_NDEF = 2
};

typedef enum
{
    CE_STATE_IDLE,
    CE_STATE_WAIT_RX,
    CE_STATE_PROCESS_RX,
    CE_STATE_WAIT_TX,
    CE_STATE_ERROR_RECOVERY
}ce_state_t;

typedef struct
{
    uint8_t state;
    uint8_t selected_file;

    const uint8_t *cc_file;
    uint16_t cc_file_len;

    uint8_t *ndef_file;
    uint16_t ndef_file_len;

    uint16_t fid_cc;
    uint16_t fid_ndef;

    bool ndef_write_allowed;
} t4t_context_t;

/*! Funtion prototypes -------------------------------------------------------------*/
void nfc_init(void);
void app_nfc_task(void);


#endif //LIONKEY_NFC_H