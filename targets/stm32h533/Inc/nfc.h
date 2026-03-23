/**
*
 * @file nfc.h
 *
 * @author Maty Martan
 *
 * @brief Provides types and methods for the NFC peripheral used in LionKey
 *
 */

#ifndef LIONKEY_NFC_H
#define LIONKEY_NFC_H

/* INCLUDES */
#include <stdbool.h>
#include <stdint.h>

#include "ctaphid.h"

/* GLOBAL DEFINES */
/*! NFC specific -------------------------------------------------------*/
#define NDEF_SIZE           2048                            /*!< Max NDEF size emulated. Range: 0005h - 7FFFh    */
#define T4T_CLA_00          0x00                            /*!< CLA value for type 4 command                    */
#define T4T_INS_SELECT      0xA4                            /*!< INS value for select command                    */
#define T4T_INS_READ        0xB0                            /*!< INS value for reabbinary command                */
#define T4T_INS_UPDATE      0xD6                            /*!< INS value for update command                    */

#define FID_CC              0xE103    /*!< File ID number for CCFile                       */
#define FID_NDEF            0x0001    /*!< File ID number for NDEF file                    */

#define APDU_WRONG_SIZE     1

/*! Status Words -----------------------------------------------------*/
#define NFC_SW_OK                        0x9000U            /*!< Success */
#define NFC_SW_CHAIN                     0x6100U            /*!< Success with more data available (for chaining) */
#define NFC_SW_FILE_NOT_FOUND            0x6A82U            /*!< File not found */
#define NFC_SW_FUNC_NOT_SUPPORTED        0x6800U            /*!< Function not supported */
#define NFC_SW_WRONG_LENGTH              0x6700U            /*!< Wrong length in the APDU command */
#define NFC_SW_WRONG_PARAMS              0x6B00U            /*!< Wrong parameters in the APDU command */
#define NFC_SW_COND_NOT_SATISFIED        0x6985U            /*!< Conditions of use not satisfied */
#define NFC_SW_INS_NOT_SUPPORTED         0x6D00U            /*!< Instruction code not supported */
#define NFC_SW_CLA_NOT_SUPPORTED         0x6E00U            /*!< Class not supported (Class code unknown) */
#define NFC_SW_NOT_ENOUGH_MEMORY         0x6F00U            /*!< Not enough memory in the device to process the command */
#define NFC_SW_SECURITY_STATUS_NOT_SAT   0x6982U            /*!< Security status not satisfied (e.g., write not allowed) */
#define NFC_SW_NOT_FOUND                 0x6A82U            /*!< Not found (e.g., file not found) */
#define NFC_SW_WRONG_DATA                0x6A80U            /*!< Wrong data (e.g., invalid command data) */
#define NFC_SW_FILE_SELECTED             0x6A84U            /*!< File already selected (e.g., trying to select a file that's already selected) */

#define TX_BUF_SIZE             512

/* GLOBAL DEFINES */
/*! Enums ------------------------------------------------------------*/

/*! NDEF files available  */
enum Files
{
    FILE_NONE = 0,              /*!< No file selected, default state   */
    FILE_CC   = 1,              /*!< NFC Context File   */
    FILE_NDEF = 2               /*!< NDEF file, only one implemented in LionKey */
};

/*! Applets available */
enum Apps
{
    APP_NONE = 0,               /*!< No app selected, default state   */
    APP_NDEF,                   /*!< NDEF applet, only one implemented in LionKey */
    APP_FIDO                    /*!< FIDO applet */
};

/*! Card Emulation State */
typedef enum
{
    CE_STATE_IDLE,              /*!< CE state: idle, waiting for discovery */
    CE_STATE_WAIT_RX,           /*!< CE state: waiting for an APDU command from reader (listening) */
    CE_STATE_PROCESS_RX,        /*!< CE state: processing the received APDU command and preparing respone */
    CE_STATE_WAIT_TX,           /*!< CE state: waiting for the response to be sent to reader */
    CE_STATE_ERROR_RECOVERY     /*!< CE state: error recovery, e.g. after a failed transmission or reception */
} ce_state_t;

typedef enum
{
    NFC_NOTINIT = 0U,        /*! Not initialized */
    NFC_START_DISCOVERY,     /*! Discovery started, waiting for a reader to come into range */
    NFC_DISCOVERY,           /*! Discovery in progress, waiting for a reader to come into range */
    NFC_CE_ACTIVE            /*! Card Emulation active, reader in range and can send APDU commands */
} nfc_state_t;


/*! Type 4 Tag Context struct */
/*! Refer to https://docs.nordicsemi.com/bundle/ncs-3.0.2/page/nrfxlib/nfc/doc/type_4_tag.html for more details */
typedef struct
{
    uint8_t selected_file;      /*!< CE context: currectly selected file */

    uint8_t selected_app;       /*!< CE context: currently selected applet */

    const uint8_t *cc_file;     /*!< CE context: contents of the CC file (to be initialised in the init function)*/
    uint16_t cc_file_len;       /*!< CE context: length of the CC file */

    uint8_t *ndef_file;         /*!< CE context: contents of the NDEF file (to be initialised in the init function)*/
    uint16_t ndef_file_len;     /*!< CE context: length of the NDEF file */

    uint16_t fid_cc;            /*!< CE context: file ID of the CC file (to be initialised in the init function) */
    uint16_t fid_ndef;          /*!< CE context: file ID of the NDEF file (to be initialised in the init function) */

    bool ndef_write_allowed;    /*!< CE context: flag indicating whether the NDEF file is writable (to be implemented) */

    /*! Chaining helpers */
    uint8_t  chain_buf[CTAPHID_MAX_PAYLOAD_LENGTH]; /*!< CE context: buffer for building responses larger than Le */
    uint16_t chain_offset;                          /*!< CE context: offset in the chain buffer for the current response being built (for responses larger than Le) */
    uint16_t chain_len;                             /*!< CE context: remaining length of the response to be sent in the chain buffer (for responses larger than Le) */
} t4t_context_t;

typedef struct
{
    nfc_state_t state;
    ce_state_t ce_state;
    t4t_context_t ce_ctx;
    uint8_t *rx_data;
    uint16_t *rcv_len;
    uint8_t tx_buf[TX_BUF_SIZE];
    uint16_t tx_len;
} nfc_runtime_t;

/*! APDU structure (as per https://www.cardlogix.com/glossary/apdu-application-protocol-data-unit-smart-card/) */
typedef struct {
    uint8_t  cla;           /* class */
    uint8_t  ins;           /* instruction */
    uint8_t  p1;            /* parameter 1 */
    uint8_t  p2;            /* parameter 2 */
    uint16_t lc;            /* data length */
    const uint8_t *data;    /* CTAP cmd + CBOR */
    uint16_t le;            /* exp resp len */
    /* helper variables for apdu parsing and response chaining */
    bool has_le;            /* flag indicating whether the APDU has an Le field (i.e., is case 2 or 4) */
    bool extended;          /* flag indicating whether the APDU is an extended APDU (i.e., has 3-byte Lc and Le fields) */
} nfc_apdu_t;

/*! APDU parsing status */
typedef enum
{
    APDU_PARSE_OK = 0,
    APDU_ERR_NULL,
    APDU_ERR_TOO_SHORT,
    APDU_ERR_MALFORMED,
    APDU_ERR_UNSUPPORTED_CASE,
    APDU_ERR_OTHER
} apdu_parse_status_t;

/* FUNCTION PROTOTYPES */
/**
 *
 * @brief NFC Initialization
 *
 * Initializes the NFC peripheral and states necessary for Type 4 Tag emulation.
 *
 */
void nfc_init(void);

/**
 *
 * @brief App NFC Task
 *
 * The working task that's called periodically in the application main loop to handle NFC events,
 * such as discovery, reception of APDU commands, and sending responses.
 */
void app_nfc_task(void);
#endif //LIONKEY_NFC_H