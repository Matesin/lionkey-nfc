/**
 * @file nfc.c
 * @brief NFC Type 4 Tag (T4T) card emulation via RFAL, handling ISO-DEP and APDU exchange.
 *
 * @author Matyas Martan
 *
 * This module implements:
 * - NFC-A card emulation (CE mode) using RFAL library,
 * - TX/RX state machine with error recovery,
 * - ISO-DEP block framing and adaptation layer (rfal_nfc.c),
 * - User presence detection and timing for CTAP,
 * - Integration with NDEF and FIDO applet handling via ctap_nfc module (ctap_nfc.c).
 *
 * NFC State Machine Flow:
 * NFC_DISCOVERY -> (PCD activates PICC) -> NFC_CE_ACTIVE -> *CE STATE MACHINE* -> NFC_START_DISCOVERY -> NFC_DISCOVERY
 *
 * CE State Machine Flow:
 * Idle (CE_STATE_IDLE)
    The card emulation layer is activated but no exchange has been initiated yet.
    Initializes context. Transitions to CE_STATE_WAIT_RX when communication is
    initialized.
 * Wait RX (CE_STATE_WAIT_RX)
    The device is listening for an incoming APDU from the PCD. The RFAL exchange
    status is polled until a complete frame is received. Upon receiving a complete frame,
    it transitions to PROCESS_RX. In case sleep is requested by the PCD, the state
    machine goes to IDLE. Otherwise, if an unexpected error occurs (i.e. an incomplete
    frame is received), it transitions to ERROR_RECOVERY.
 * Process RX (CE_STATE_PROCESS_RX)
    The received APDU is parsed and a response is constructed. Upon success, transmission
    is initiated and the state advances to WAIT_TX.
 * Wait TX (CE_STATE_WAIT_TX)
    The device waits for the transmission of the response to complete, after which the
    state returns to WAIT_RX to await the next command.
 * Error Recovery (CE_STATE_ERROR_RECOVERY)
    An unrecoverable protocol error has occurred. The session is terminated, and the
    primary state machine transitions to NFC_START_DISCOVERY to reinitialize the
    context for a new session.
 */

/************/
/* INCLUDES */
/************/
#include "nfc.h"

#include "ctap.h"
#include "ctap_nfc.h"
#include "demo_ce.h"
#include "nfc_test.h"
#include "rfal_nfc.h"
#include "utils.h"
#include "rfal_nfca.h"
#include "eval_utils.h"

/********************/
/* GLOBAL VARIABLES */
/********************/
extern ctap_state_t app_ctap;

/*******************/
/* LOCAL VARIABLES */
/*******************/

/* NFC-A config:
 * 4-byte UIDs with first byte 0x08 would need random number for the subsequent 3 bytes.
 * 4-byte UIDs with first byte 0x*F are Fixed number, not unique
 * 7-byte UIDs need a manufacturer ID and need to assure uniqueness of the rest.*/
static const uint8_t NFCID[]     = {0x5F, 'L', 'N', 'K'};    /* =_LNK, 5F  4C  4E  4B NFCID1 / UID (4 bytes) - first byte is fixed identifier, not random */
static const uint8_t SENS_RES[]  = {0x44, 0x00};             /* SENS_RES / ATQA for 4-byte UID            */
static const uint8_t SEL_RES     = 0x20U;                    /* SEL_RES / SAK - 0x20 propagate ISO-DEP support*/

#if LIONKEY_DEBUG_LEVEL > 1
static eval_timer_t nfc_timer;
#endif
/**
  * Ver : Indicates the NDEF mapping version <BR>
  * Nbr : Indicates the number of blocks that can be read <BR>
  * Nbw : Indicates the number of blocks that can be written <BR>
  * NmaxB : Indicates the maximum number of blocks available for NDEF data <BR>
  * WriteFlag : Indicates whether a previous NDEF write procedure has finished or not <BR>
  * RWFlag : Indicates data can be updated or not <BR>
  * Ln : Is the size of the actual stored NDEF data in bytes <BR>
  * Checksum : allows the PCD to check whether the Attribute Data are correct <BR>
  */
static const uint8_t CC_FILE[] = {  0x00, 0x0F,                                       /* CCLEN      */
                                    0x20,                                             /* T4T_VNo    */
                                    0x00, 0xFF,                                       /* MLe        */
                                    0x00, 0xFF,                                       /* MLc        */
                                    0x04,                                             /* T          */
                                    0x06,                                             /* L          */
                                    (FID_NDEF & 0xFF00) >> 8, (FID_NDEF & 0x00FF),    /* V1         */
                                    (NDEF_SIZE & 0xFF00) >> 8, (NDEF_SIZE & 0x00FF),  /* V2         */
                                    0x00,                                             /* V3         */
                                    0x00                                              /* V4         */
};

static const uint8_t NDEF_FILE[] = { 0x00, 0x10, 0xD1, 0x01, 0x0C, 0x55, 0x01, 0x6C, 0x69, 0x6F, 0x6E, 0x6B, 0x65, 0x79, 0x2E, 0x64, 0x65, 0x76 };

static nfc_runtime_t nfc_runtime =
{
    .state = NFC_DISCOVERY,
    .ce_state = CE_STATE_IDLE,
    .rx_data = NULL,
    .rcv_len = NULL,
    .tx_len = 0U
};

static rfalNfcDiscoverParam discParam;
static rfalNfcState prev_rf_state = RFAL_NFC_STATE_IDLE;
static bool prev_rf_state_valid = false;

/*****************************/
/* LOCAL FUNCTION PROTOTYPES */
/*****************************/

/**
 * @brief Initialize the NFC parameters and starts the discovery loop using RFAL.
 * Set up the NFC-A CE configuration, including SENS_RES, NFCID, SEL_RES, and ISO-DEP frame size.
 * Register the RFAL NFC state change callback for debugging (if LIONKEY_DEBUG_LEVEL > 1).
 *
 * @return true if initialization succeeded, false otherwise
 */
static bool nfc_init_params(void);
/**
 * @brief Initialize the NFC session context, including the CC file, NDEF file, file IDs, and state variables.
 */
static void nfc_init_session(void);
/**
 * @brief RFAL NFC state change callback for logging state transitions and debugging
 *
 * @param st current RFAL NFC state
 */
static void nfc_notify(rfalNfcState st);

/**
 * @brief Card Emulation task
 *
 * @return true if the NFC session has ended and we should return to discovery mode
 * @return false to continue the session
 */
static bool nfc_ce_task(void);

/**
 * @brief Starts the NFC reception process to receive a C-APDU from the PCD.
 * Sets up the necessary state variables and buffers for reception.
 *
 * @return true if RX started successfully
 * @return false otherwise
 */
static bool nfc_start_rx(void);

/**
 * @brief Start NFC transmission to send a R-APDU
 *
 * @param [in/out] tx_data buffer containing the APDU response to send to the PCD
 * @param [in/out] tx_data_len length of the APDU response data in bytes
 * @return true if TX started successfully
 * @return false otherwise
 */
static bool nfc_start_tx(uint8_t *tx_data, uint16_t tx_data_len);

/**
 * @brief poll transmission status and handle errors should they occur.
 *
 * @return true if the RX process has completed and data is ready to be processed
 * @return false otherwise
 */
static bool nfc_handle_wait_rx(void);

/**
 * @brief poll transmission status and handle errors should they occur.
 *
 * @return true if the received APDU has been processed and a response is ready to be sent
 * @return false if tra
 */
static bool nfc_handle_wait_tx(void);

/**
 * @brief handle the received APDU, parse it, and build the appropriate response. Handle errors should they occur.
 *
 * @return true if the received APDU has been processed and the response has been sent
 * @return false if still in process or an error has occured
 */
static bool nfc_handle_process_rx(void);

/**
 * @brief enter error recovery mode, log the error message, and reset the session to return to discovery mode.
 *
 * @param [in] msg error message to log before entering error recovery
 * @param [in] err error code
 */
static void nfc_enter_error_recovery(const char* msg, ErrorStatus err);

/**
 * @brief utilize RFAL to poll the current transmission status.
 *
 * @param [in] line transmission line to poll for status (RX or TX)
 * @return true if transmission is over and no error has occured
 * @return false if busy or an error has occured
 */
static bool nfc_poll_transmission_status(transmission_line_t line);

/**
 * @brief log the received APDU in hex.
 *
 * @param [in] apdu buffer containing the APDU to log
 * @param [in] apdu_len length of the received APDU
 */
static void nfc_log_received_apdu(uint8_t* apdu, uint16_t apdu_len);

/**
 * @brief log the sent APDU in hex.
 *
 * @param [in] apdu buffer containing the APDU response to log
 * @param [in] apdu_len length of the sent APDU
 */
static void nfc_log_sent_apdu(uint8_t* apdu, uint16_t apdu_len);

/************************/
/* FUNCTION DEFINITIONS */
/************************/
void nfc_init(void)
{
    debug_log("initializing NFC..." nl);
    if (!nfc_init_params()) {
        error_log(red("ERROR: Failed to initialize NFC") nl);
        return;
    }

    #ifdef NFC_DEMO_CE
    demoCeInit(NULL);
    #else
    nfc_init_session();
    #if LIONKEY_DEBUG_LEVEL > 1
    nfc_timer = create_timer();
    #endif
    #endif

    info_log("NFC initialized" nl);
}

static bool nfc_init_params(void)
{
    ReturnCode err = rfalNfcInitialize();
    if( err == RFAL_ERR_NONE )
    {
        rfalNfcDefaultDiscParams( &discParam );

        discParam.devLimit             = NFC_DEV_LIMIT;
        #if LIONKEY_DEBUG_LEVEL > 1
        discParam.notifyCb             = nfc_notify;
        #endif

        discParam.totalDuration        = NFC_DISC_DUR;

        /* Set configuration for NFC-A CE */
        (void) memcpy( discParam.lmConfigPA.SENS_RES, SENS_RES, RFAL_LM_SENS_RES_LEN );     /* Set SENS_RES / ATQA */
        (void) memcpy( discParam.lmConfigPA.nfcid, NFCID, RFAL_LM_NFCID_LEN_04 );           /* Set NFCID / UID */
        discParam.lmConfigPA.nfcidLen = RFAL_LM_NFCID_LEN_04;                               /* Set NFCID length to 4 bytes */
        discParam.lmConfigPA.SEL_RES  = SEL_RES;                                            /* Set SEL_RES / SAK */

        discParam.isoDepFS = RFAL_ISODEP_FSXI_256;                                          /* Set ISO-DEP Frame Size to 256 bytes */
        discParam.nfcDepLR = RFAL_NFCDEP_LR_254;

        discParam.techs2Find = RFAL_NFC_LISTEN_TECH_A;

        err = rfalNfcDiscover( &discParam );

        return (err == RFAL_ERR_NONE);
    }
    return false;
}

static void nfc_init_session(void)
{
    t4t_context_t *ctx = &nfc_runtime.ce_ctx;
    ctx->selected_file = FILE_NONE;

    ctx->selected_app = APP_NONE;

    ctx->cc_file = CC_FILE;
    ctx->cc_file_len = sizeof(CC_FILE);

    ctx->ndef_file = NDEF_FILE;
    ctx->ndef_file_len = sizeof(NDEF_FILE);

    ctx->fid_cc = FID_CC;
    ctx->fid_ndef = FID_NDEF;
    ctx->ndef_write_allowed = true;

    nfc_runtime.ce_state = CE_STATE_IDLE;
    /* reset transaction state */
    nfc_runtime.rx_data = NULL;
    nfc_runtime.rcv_len = NULL;
    nfc_runtime.tx_len = 0;
}

static void nfc_notify(rfalNfcState st)
{
    // don't log state unless it has changed
    if ((prev_rf_state_valid == true) && (prev_rf_state == st)) {
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
    prev_rf_state_valid = true;
}

void app_nfc_task(void)
{
    #ifndef NFC_DEMO_CE

    rfalNfcWorker();

    switch (nfc_runtime.state)
    {
    case NFC_START_DISCOVERY:
        nfc_init_session(); // reinitialise the session
        nfc_runtime.state = NFC_DISCOVERY;
        break;

    case NFC_DISCOVERY:
        if (rfalNfcIsDevActivated(rfalNfcGetState()))
        {
            nfc_init_session();
            nfc_runtime.state = NFC_CE_ACTIVE;
            // Upon detecting RF field, start the NFC-powered timer
            ctap_nfc_start_user_presence_timer(&app_ctap.nfc_timer);
        }
        break;

    case NFC_CE_ACTIVE:
        if (nfc_ce_task())
        {
            debug_log("NFC: session ended" nl);
            nfc_runtime.state = NFC_START_DISCOVERY;
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

    #else
    demoTask();
    #endif
}

static bool nfc_ce_task(void)
{
    switch (rfalNfcGetState())
    {
    case RFAL_NFC_STATE_START_DISCOVERY:
        /* Reinitialize context for a new session */
        nfc_init_session();
        return true;

    case RFAL_NFC_STATE_ACTIVATED:
        if (nfc_runtime.ce_state == CE_STATE_IDLE)
        {
            debug_log("CE: start waiting for command" nl);
            nfc_runtime.ce_ctx.negotiated_inf_len = rfalIsoDepGetMaxInfLen();
            debug_log(yellow("CE: negotiated max frame size: %u" nl), nfc_runtime.ce_ctx.negotiated_inf_len);
            nfc_start_rx();
        }
        break;

    case RFAL_NFC_STATE_DATAEXCHANGE_DONE:
    case RFAL_NFC_STATE_DATAEXCHANGE:
        /* expected during ISO-DEP exchange */
        break;

    case RFAL_NFC_STATE_LISTEN_SLEEP:
        break;
    default:
        return false;
    }

    switch (nfc_runtime.ce_state)
    {
        case CE_STATE_IDLE:
            return false;

        case CE_STATE_WAIT_RX:
            return nfc_handle_wait_rx();

        case CE_STATE_PROCESS_RX:
            return nfc_handle_process_rx();

        case CE_STATE_WAIT_TX:
            return nfc_handle_wait_tx();

        case CE_STATE_ERROR_RECOVERY:
            return false;

        default:
            nfc_enter_error_recovery("Invalid CE state", 0);
            return false;
    }
}

static bool nfc_handle_wait_rx(void)
{

    if (!nfc_poll_transmission_status(RX))
    {
        return false;
    }

    if ((nfc_runtime.rx_data == NULL) || (nfc_runtime.rcv_len == NULL))
    {
        //TODO: error code
        nfc_enter_error_recovery("CE RX pointers invalid", 0);
        return false;
    }

    if (*nfc_runtime.rcv_len == 0U)
    {
        debug_log("CE RX empty APDU" nl);
        nfc_runtime.ce_state = CE_STATE_IDLE;
        return false;
    }

    if (*nfc_runtime.rcv_len > TX_BUF_SIZE) {
        nfc_enter_error_recovery("APDU too large", 0);
        return false;
    }
    #if LIONKEY_DEBUG_LEVEL > 1
    nfc_log_received_apdu(nfc_runtime.rx_data, *nfc_runtime.rcv_len);
    #endif

    nfc_runtime.ce_state = CE_STATE_PROCESS_RX;
    return false;
}

static bool nfc_handle_process_rx(void)
{

    nfc_runtime.tx_len = nfc_parse_and_respond(&nfc_runtime.ce_ctx,
                                                nfc_runtime.rx_data,
                                                *nfc_runtime.rcv_len,
                                                nfc_runtime.tx_buf,
                                                sizeof(nfc_runtime.tx_buf));

    if ((nfc_runtime.tx_len == NFC_PARSE_WRONG_SIZE) || (nfc_runtime.tx_len == 0))
    {
        nfc_enter_error_recovery("APDU response invalid", 0);
        return false;
    }
    nfc_log_sent_apdu(nfc_runtime.tx_buf, nfc_runtime.tx_len);
    if (!nfc_start_tx(nfc_runtime.tx_buf, nfc_runtime.tx_len))
    {
        nfc_enter_error_recovery("Failed to start TX after processing RX", 69);
        rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
    }
    return false;
}

static bool nfc_handle_wait_tx(void)
{

    if (!nfc_poll_transmission_status(TX))
    {
        return false;
    }

    if ((nfc_runtime.rx_data != NULL) && (nfc_runtime.rcv_len != NULL) && (*nfc_runtime.rcv_len > 0U))
    {
        #if LIONKEY_DEBUG_LEVEL > 1
        nfc_log_received_apdu(nfc_runtime.rx_data, *nfc_runtime.rcv_len);
        #endif
        nfc_runtime.ce_state = CE_STATE_PROCESS_RX;
    }
    else
    {
        /* Exchange completed but no next APDU is available, re-arm the first receive path. */
        nfc_runtime.ce_state = CE_STATE_IDLE;
        if (!nfc_start_rx())
        {
          nfc_enter_error_recovery("Failed to start RX after TX", 0);
        }
    }
    nfc_runtime.tx_len = 0;
    return false;
}

static bool nfc_start_rx(void)
{
    nfc_runtime.rx_data = NULL;
    nfc_runtime.rcv_len = NULL;

    /* Receive the command from the PCD */
    const ReturnCode err = rfalNfcDataExchangeStart(NULL, 0, &nfc_runtime.rx_data, &nfc_runtime.rcv_len, RFAL_FWT_NONE);

    if (err != RFAL_ERR_NONE)
    {
        error_log(red("ERROR: CE: start RX failed: %d") nl, err);
        nfc_runtime.ce_state = CE_STATE_ERROR_RECOVERY;
        return false;
    }

    debug_log("CE: start RX successful" nl);
    nfc_runtime.ce_state = CE_STATE_WAIT_RX;
    return true;
}

static bool nfc_start_tx(uint8_t *tx_data, uint16_t tx_data_len)
{
    if (tx_data == NULL)
    {
        error_log(red("ERROR: CE: nfc_start_tx called with NULL data") nl);
        return false;
    }

    const ReturnCode err = rfalNfcDataExchangeStart(tx_data, tx_data_len, &nfc_runtime.rx_data, &nfc_runtime.rcv_len, RFAL_FWT_NONE);

    if (err != RFAL_ERR_NONE)
    {
        error_log(red("ERROR: CE start TX failed: %d") nl, err);
        nfc_runtime.ce_state = CE_STATE_ERROR_RECOVERY;
        return false;
    }

    debug_log("CE: start TX successful" nl);
    nfc_runtime.ce_state = CE_STATE_WAIT_TX;

    return true;
}

static bool nfc_poll_transmission_status(transmission_line_t line)
{
    const ReturnCode err = rfalNfcDataExchangeGetStatus();

    if (err == RFAL_ERR_BUSY)
    {
        return false;
    }

    if (err == RFAL_ERR_SLEEP_REQ)
    {
        debug_log("CE: sleep requested after TX" nl);
        nfc_runtime.ce_state = CE_STATE_IDLE;
        return false;
    }

    if (err != RFAL_ERR_NONE)
    {
        line == TX ? nfc_enter_error_recovery("TX failed", err) : nfc_enter_error_recovery("RX failed", err);
        return false;
    }

    return true;
}

static void nfc_enter_error_recovery(const char* msg, const ErrorStatus err)
{
    error_log(red("ERROR: CE: %s (%d)") nl, msg, err);

    if (err == RFAL_ERR_NOMEM)
    {
        error_log(red("The key ran out of memory, please reset the key") nl);
    }
    nfc_runtime.ce_state = CE_STATE_ERROR_RECOVERY;

    if (err == RFAL_ERR_TIMEOUT || err == RFAL_ERR_PROTO) {
        nfc_start_rx();
        return;
    }

    (void) rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
}

static void nfc_log_received_apdu(uint8_t* apdu, const uint16_t apdu_len)
{
    nfc_timer.start(&nfc_timer);
    if (apdu == NULL)
    {
        error_log(red("ERROR: CE: apdu received is NULL" nl));
        return;
    }
    debug_log(blue("Received APDU (%u bytes):"), apdu_len);
    dump_hex_large(apdu, apdu_len);
}

static void nfc_log_sent_apdu(uint8_t* apdu, const uint16_t apdu_len)
{
    nfc_timer.stop(&nfc_timer);
    if (apdu == NULL)
    {
        error_log(red("ERROR: CE: apdu sent is NULL" nl));
        return;
    }
    nfc_timer.stop(&nfc_timer);
    info_log(cyan("APDU response time: %u ms") nl, (unsigned)nfc_timer.elapsed_ms(&nfc_timer));
    info_log(blue("Sent APDU (%u bytes): "), apdu_len);
    dump_hex_large(apdu, apdu_len);
}
