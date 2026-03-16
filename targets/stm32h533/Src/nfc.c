//
// Created by Maty Martan on 11.01.2026.
//

#include "nfc.h"

#include "ctap_nfc.h"
#include "demo_ce.h"
#include "main.h"
#include "ndef_errno.h"
#include "nfc_test.h"
#include "rfal_nfc.h"
#include "spi.h"
#include "utils.h"
#include "rfal_nfca.h"

/* NFC-A CE config */
/* 4-byte UIDs with first byte 0x08 would need random number for the subsequent 3 bytes.
 * 4-byte UIDs with first byte 0x*F are Fixed number, not unique
 * 7-byte UIDs need a manufacturer ID and need to assure uniqueness of the rest.*/
static uint8_t NFCID[]     = {0x5F, 'L', 'N', 'K'};    /* =_LNK, 5F  4C  4E  4B NFCID1 / UID (4 bytes) - static value */
static uint8_t SENS_RES[]  = {0x44, 0x00};             /* SENS_RES / ATQA for 4-byte UID            */
static uint8_t SEL_RES     = 0x20U;                     /* SEL_RES / SAK */

/**
  * Ver : Indicates the NDEF mapping version <BR>
  * Nbr : Indicates the number of blocks that can be read <BR>
  * Nbw : Indicates the number of blocks that can be written <BR>
  * NmaxB : Indicates the maximum number of blocks available for NDEF data <BR>
  * WriteFlag : Indicates whether a previous NDEF write procedure has finished or not <BR>
  * RWFlag : Indicates data can be updated or not <BR>
  * Ln : Is the size of the actual stored NDEF data in bytes <BR>
  * Checksum : allows the Reader/Writer to check whether the Attribute Data are correct <BR>
  */
static uint8_t InformationBlock[] = { 0x10,                                             /* Ver        */
                                      0x08,                                             /* Nbr        */
                                      0x08,                                             /* Nbw        */
                                      0x00, 0x0F,                                       /* NmaxB      */
                                      0x00, 0x00, 0x00, 0x00,                           /* RFU        */
                                      0x00,                                             /* WriteFlag  */
                                      0x01,                                             /* RWFlag     */
                                      0x00, 0x00, 0x15,                                 /* Ln         */
                                      0x00, 0x45                                        /* Checksum   */
                                      };


static uint8_t        ndefFile[NDEF_SIZE];  /*!< Buffer to store NDEF File                 */
static rfalNfcDiscoverParam discParam;

rfalNfcState prev_state = -1;

uint8_t select_state = NFC_DISCOVERY;
int8_t selected_index = -1;
t4t_context_t ce_ctx;


static bool nfc_init_params(void);
static void nfc_notify(rfalNfcState st);
static void init_context(t4t_context_t *ctx);
static void nfc_ce_task(void);

void nfc_init(void)
{
    debug_log("initializing NFC..." nl);
    if (!nfc_init_params()) {
        debug_log(red("Failed to initialize NFC") nl);
        return;
    }
    demoCeInit(NULL);
    init_context(&ce_ctx);
    // run_nfc_tests();
    debug_log("NFC initialized" nl);
}

static bool nfc_init_params(void)
{
    ReturnCode err = rfalNfcInitialize();
    if( err == RFAL_ERR_NONE )
    {
        rfalNfcDefaultDiscParams( &discParam );

        discParam.devLimit      = 1U;

        discParam.notifyCb             = nfc_notify;
        discParam.totalDuration        = 60000U;

        /* Set configuration for NFC-A CE */
        memcpy( discParam.lmConfigPA.SENS_RES, SENS_RES, RFAL_LM_SENS_RES_LEN );     /* Set SENS_RES / ATQA */
        memcpy( discParam.lmConfigPA.nfcid, NFCID, RFAL_LM_NFCID_LEN_04 );           /* Set NFCID / UID */
        discParam.lmConfigPA.nfcidLen = RFAL_LM_NFCID_LEN_04;/* Set NFCID length to 4 bytes */
        discParam.lmConfigPA.SEL_RES  = SEL_RES;                                     /* Set SEL_RES / SAK */

        discParam.isoDepFS = RFAL_ISODEP_FSXI_256;                                    /* Set ISO-DEP Frame Size to 256 bytes */
        discParam.nfcDepLR = RFAL_NFCDEP_LR_254;

        discParam.techs2Find = RFAL_NFC_LISTEN_TECH_A;

        err = rfalNfcDiscover( &discParam );

        return (err == RFAL_ERR_NONE);
    }
    return false;
}



uint16_t nfc_parse_and_respond(uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf, uint16_t txBufLen )
{
    nfc_apdu_t apdu;
    apdu_parse_status_t err;

    if (txBuf == NULL || txBufLen < 2) {
        platformErrorHandle();
        return 0;
    }

    err = nfc_parse_apdu(rxData, rxDataLen, &apdu);

    if (err != APDU_PARSE_OK) {
        return nfc_put_sw(txBuf, NFC_SW_COND_NOT_SATISFIED);
    }

    switch(rxData[1])
    {
    case T4T_INS_SELECT:
        return nfc_handle_select(&ce_ctx, &apdu, rxData);

    case T4T_INS_READ:
        return nfc_handle_read(&ce_ctx, &apdu, txBuf, txBufLen);

    case T4T_INS_UPDATE:
        return nfc_handle_update(&ce_ctx, &apdu, rxData);

    default:
        /* MISRA 16.4: No empty case allowed */
        break;
    }
    return nfc_put_sw(txBuf, NFC_SW_CLA_NOT_SUPPORTED);
}

static void nfc_notify(rfalNfcState st)
{
    // don't log state unless it has changed
    if (prev_state == st) {
        return;
    }
    switch(st)
    {
        case RFAL_NFC_STATE_IDLE:
            debug_log("NFC: idle" nl);
            break;
        case RFAL_NFC_STATE_ACTIVATED:
            debug_log("NFC: device activated (CE mode)" nl);
            break;
        case RFAL_NFC_STATE_LISTEN_COLAVOIDANCE:
            debug_log("NFC: collision avoidance" nl);
            break;
        default:
            /* MISRA 16.4: No empty default case allowed */
            break;
    }
    prev_state = st;
}

void app_nfc_task(void)
{
    #if NFC_DEMO_CE
        demoTask();
    #else

        rfalNfcWorker();
        switch (select_state)
        {
        case NFC_START_DISCOVERY:
            select_state = NFC_DISCOVERY;
            break;
        case NFC_DISCOVERY:
            if (rfalNfcIsDevActivated(rfalNfcGetState()))
            {
                select_state = NFC_CE_ACTIVE;
            }
            break;
        case NFC_CE_ACTIVE:
            nfc_ce_task();
            break;
        case NFC_NOTINIT:
        default:
            /* MISRA 16.4: No empty case allowed */
            break;
        }

    // if ((select_state == NFC_CE_ACTIVE) && (rfalNfcDataExchangeGetStatus() != RFAL_ERR_BUSY))
    // {
    //     rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
    //     debug_log(red("NFC: session ended") nl);
    //     select_state = NFC_START_DISCOVERY;
    // }
#endif
}

static void nfc_ce_task(void)
{
    static uint8_t  *rx_data  = NULL;
    static uint16_t *rcv_len  = NULL;
    static uint8_t   nfc_resp_buf[512];

    switch (rfalNfcGetState())
    {
    case RFAL_NFC_STATE_ACTIVATED:
        rfalNfcDataExchangeStart(NULL, 0, &rx_data, &rcv_len, RFAL_FWT_NONE);
        break;
    case RFAL_NFC_STATE_DATAEXCHANGE_DONE:
        {
            uint16_t tx_len = 0;

            if (rx_data != NULL && rcv_len != NULL && *rcv_len >= 4) {
                tx_len = nfc_parse_and_respond(
                    rx_data, *rcv_len,
                    nfc_resp_buf, sizeof(nfc_resp_buf)
                );
            }

            rfalNfcDataExchangeStart(
                (tx_len > 0) ? nfc_resp_buf : NULL,
                tx_len,
                &rx_data,
                &rcv_len,
                RFAL_FWT_NONE
            );
            break;
        }

    case RFAL_NFC_STATE_LISTEN_SLEEP:
        rfalNfcDataExchangeStart(NULL, 0, &rx_data, &rcv_len, RFAL_FWT_NONE);
        break;

    case RFAL_NFC_STATE_DATAEXCHANGE:
    case RFAL_NFC_STATE_START_DISCOVERY:
    case RFAL_NFC_STATE_IDLE:
    default:
        break;
    }
}
static void init_context(t4t_context_t *ctx)
{
    ctx->state = STATE_IDLE;
    ctx->selected_file = FILE_NONE;

    ctx->cc_file = InformationBlock;
    ctx->cc_file_len = sizeof(InformationBlock);
    ctx->ndef_file = ndefFile;
    ctx->ndef_file_len = sizeof(ndefFile);
    ctx->fid_cc = FID_CC;
    ctx->fid_ndef = FID_NDEF;
    ctx->ndef_write_allowed = true;
}

uint16_t nfc_put_sw(uint8_t *buf, uint16_t sw )
{
    buf[0] = (uint8_t)(sw >> 8);
    buf[1] = (uint8_t)(sw & 0xFF);
    return 2;
}

