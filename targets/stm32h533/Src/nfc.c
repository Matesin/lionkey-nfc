//
// Created by Maty Martan on 11.01.2026.
//

#include "nfc.h"

#include "ctap.h"
#include "ctap_nfc.h"
#include "demo_ce.h"
#include "main.h"
#include "ndef_errno.h"
#include "nfc_test.h"
#include "rfal_nfc.h"
#include "spi.h"
#include "utils.h"
#include "rfal_nfca.h"

uint32_t nfc_user_presence_timer;

extern ctap_state_t app_ctap;

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
static uint8_t InformationBlock[] = {   0x00, 0x0F,                                       /* CCLEN      */
                                        0x20,                                             /* T4T_VNo    */
                                        0x10, 0x00,                                       /* MLe        */
                                        0x10, 0x00,                                       /* MLc        */
                                        0x04,                                             /* T          */
                                        0x06,                                             /* L          */
                                        (FID_NDEF & 0xFF00) >> 8, (FID_NDEF & 0x00FF),    /* V1         */
                                        (NDEF_SIZE & 0xFF00) >> 8, (NDEF_SIZE & 0x00FF),  /* V2         */
                                        0x00,                                             /* V3         */
                                        0x00                                              /* V4         */
                                    };


static uint8_t        ndefFile[] = {    0x00, 0x10, 0xD1, 0x01, 0x0C, 0x55, 0x01, 0x6C, 0x69, 0x6F, 0x6E, 0x6B, 0x65, 0x79, 0x2E, 0x64, 0x65, 0x76 };
static rfalNfcDiscoverParam discParam;

static rfalNfcState prev_rf_state = -1;

static ce_state_t ce_state = CE_STATE_IDLE;

static uint8_t select_state = NFC_DISCOVERY;
static t4t_context_t ce_ctx;

static uint8_t  *rx_data  = NULL;
static uint16_t *rcv_len  = NULL;
static uint8_t   tx_buf[TX_BUF_SIZE];
static uint16_t  tx_len = 0;

static bool nfc_init_params(void);
static void nfc_notify(rfalNfcState st);
static void init_context(t4t_context_t *ctx);
static bool nfc_ce_task(void);
static bool nfc_start_rx(void);
static bool nfc_start_tx(uint8_t *tx_data, uint16_t tx_data_len);

void nfc_init(void)
{
    debug_log("initializing NFC..." nl);
    if (!nfc_init_params()) {
        debug_log(red("Failed to initialize NFC") nl);
        return;
    }
    #ifdef NFC_DEMO_CE
    demoCeInit(NULL);
    #else
    init_context(&ce_ctx);
    #endif
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

static void nfc_notify(rfalNfcState st)
{
    // don't log state unless it has changed
    if (prev_rf_state == st) {
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
    prev_rf_state = st;
}

void app_nfc_task(void)
{
    #ifdef NFC_DEMO_CE
        demoTask();
    #else

    rfalNfcWorker();

    switch (select_state)
    {
    case NFC_START_DISCOVERY:
        init_context(&ce_ctx); // reinitialise the context
        select_state = NFC_DISCOVERY;
        break;

    case NFC_DISCOVERY:
        if (rfalNfcIsDevActivated(rfalNfcGetState()))
        {
            init_context(&ce_ctx);
            select_state = NFC_CE_ACTIVE;
            // Upon detecting RF field, start the NFC-powered timer
            ctap_nfc_start_user_presence_timer(&app_ctap.nfc_timer);
        }
        break;

    case NFC_CE_ACTIVE:
        if (nfc_ce_task())
        {
            debug_log("NFC: session ended" nl);
            select_state = NFC_START_DISCOVERY;
            //user not present, set the user presence flag to false
            ctap_nfc_stop_user_presence_timer(&app_ctap.nfc_timer);
        }
        //check the timer, toggle user presence value if expired
        ctap_nfc_is_user_presence_timer_expired(&app_ctap.nfc_timer);
        break;

    case NFC_NOTINIT:
    default:
        break;
    }
#endif
}

static bool nfc_ce_task(void)
{
    ReturnCode err;

    switch (rfalNfcGetState())
    {
    case RFAL_NFC_STATE_START_DISCOVERY:
        /* Reinitialize context for a new session */
        init_context(&ce_ctx); 
        return true;
        
    case RFAL_NFC_STATE_ACTIVATED:
        if (ce_state == CE_STATE_IDLE)
        {
            debug_log("CE: start waiting for command" nl);
            nfc_start_rx();
        }
        break;
        
    case RFAL_NFC_STATE_DATAEXCHANGE_DONE:
    case RFAL_NFC_STATE_DATAEXCHANGE:
    case RFAL_NFC_STATE_LISTEN_SLEEP:
        break;
    default:
        return false;
    }
    switch (ce_state)
    {
        case CE_STATE_IDLE:
            return false;

        case CE_STATE_WAIT_RX:
            err = rfalNfcDataExchangeGetStatus();

            if (err == RFAL_ERR_BUSY)
            {
                return false;
            }

            if (err == RFAL_ERR_SLEEP_REQ)
            {
                debug_log("CE: peer requested sleep" nl);
                ce_state = CE_STATE_IDLE;
                return false;
            }

            if (err != RFAL_ERR_NONE)
            {
                debug_log("CE RX failed: %d" nl, err);
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            if ((rx_data == NULL) || (rcv_len == NULL))
            {
                debug_log("CE RX pointers invalid" nl);
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            if (*rcv_len == 0U)
            {
                debug_log("CE RX empty APDU" nl);
                ce_state = CE_STATE_IDLE;
                return false;
            }

            debug_log("CE: APDU received (%u bytes)" nl, *rcv_len);
            debug_log("Received APDU content: %s" nl, hex2Str(rx_data, *rcv_len));
            ce_state = CE_STATE_PROCESS_RX;
            return false;

        case CE_STATE_PROCESS_RX:
            tx_len = nfc_parse_and_respond(&ce_ctx, rx_data, *rcv_len, tx_buf, sizeof(tx_buf));

            if (tx_len == NFC_PARSE_WRONG_SIZE)
            {
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            debug_log("CE: APDU processed, response len = %u" nl, tx_len);
            debug_log("Sent APDU content: %s" nl, hex2Str(tx_buf, tx_len));
            if (!nfc_start_tx(tx_buf, tx_len))
            {
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            }
            return false;

        case CE_STATE_WAIT_TX:
            err = rfalNfcDataExchangeGetStatus();

            if (err == RFAL_ERR_BUSY)
            {
                return false;
            }

            if (err == RFAL_ERR_SLEEP_REQ)
            {
                debug_log("CE: sleep requested after TX" nl);
                ce_state = CE_STATE_IDLE;
                return false;
            }

            if (err != RFAL_ERR_NONE)
            {
                debug_log("CE TX failed: %d" nl, err);
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            if ((rx_data != NULL) && (rcv_len != NULL) && (*rcv_len > 0U))
            {
                debug_log("CE: next APDU received (%u bytes)" nl, *rcv_len);
                debug_log("Received APDU content: %s" nl, hex2Str(rx_data, *rcv_len));
                ce_state = CE_STATE_PROCESS_RX;
            }
            else
            {
                /*
                 * Exchange completed but no next APDU is available.
                 * Re-arm the first receive path.
                 */
                ce_state = CE_STATE_IDLE;
                nfc_start_rx();
            }
            return false;


        case CE_STATE_ERROR_RECOVERY:
            rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            return false;
        default:
            debug_log(red("ERROR: CE: unknown state"nl));
            ce_state = CE_STATE_ERROR_RECOVERY;
            return false;
    }
}

static void init_context(t4t_context_t *ctx)
{
    ctx->selected_file = FILE_NONE;

    ctx->selected_app = APP_NONE;

    ctx->cc_file = InformationBlock;
    ctx->cc_file_len = sizeof(InformationBlock);

    ctx->ndef_file = ndefFile;
    ctx->ndef_file_len = sizeof(ndefFile);

    ctx->fid_cc = FID_CC;
    ctx->fid_ndef = FID_NDEF;
    ctx->ndef_write_allowed = true;

    ce_state = CE_STATE_IDLE;
    /* reset transaction state */
    rx_data = NULL;
    rcv_len = NULL;
    tx_len = 0;
}

static bool nfc_start_rx(void)
{
    rx_data = NULL;
    rcv_len = NULL;
    /* Receive the command from the reader */
    const ReturnCode err = rfalNfcDataExchangeStart(NULL, 0, &rx_data, &rcv_len, RFAL_FWT_NONE);
    if (err != RFAL_ERR_NONE)
    {
        debug_log("CE start RX failed: %d" nl, err);
        ce_state = CE_STATE_ERROR_RECOVERY;
        return false;
    }
    debug_log("CE: start RX successful" nl);
    ce_state = CE_STATE_WAIT_RX;
    return true;
}
static bool nfc_start_tx(uint8_t *tx_data, uint16_t tx_data_len)
{
    const ReturnCode err = rfalNfcDataExchangeStart(tx_data, tx_data_len, &rx_data, &rcv_len, RFAL_FWT_NONE);
    if (err != RFAL_ERR_NONE)
    {
        debug_log("CE start TX failed: %d" nl, err);
        ce_state = CE_STATE_ERROR_RECOVERY;
        return false;
    }
    debug_log("CE: start TX successful" nl);
    ce_state = CE_STATE_WAIT_TX;
    return true;
}

bool nfc_is_user_presence_timer_expired(void)
{
    return false;
    // const uint32_t current_time = HAL_GetTick();
    // if (current_time - nfc_user_presence_timer >= NFC_USER_PRESENCE_TIMER_THRESHOLD_MS) {
    //     debug_log(red("CE: user presence timer expired") nl);
    //     return true;
    // }
    // return false;
}
