/**
 * @file ctap_nfc.c
 * @brief CTAP-over-NFC APDU parsing and applet dispatch for Type 4 Tag emulation.
 *
 * @author Matyas Martan
 *
 * This module implements:
 * - ISO7816 APDU parsing (short and extended forms),
 * - FIDO applet selection and CTAP command transport over NFC
*      (see https://fidoalliance.org/specs/fido-v2.3-ps-20260226/fido-client-to-authenticator-protocol-v2.3-ps-20260226.html),
 * - NDEF applet command handling (SELECT / READ / UPDATE),
 * - APDU response formatting including status words and short-response chaining.
 *
 * The transport layer (ISO-DEP block chaining, retransmissions, timing) is handled
 * by RFAL. This module focuses on APDU-level behavior and CTAP payload routing.
 */

/************/
/* INCLUDES */
/************/
#include <string.h>
#include "terminal.h"
#include "ctap_nfc.h"
#include "ctap.h"
#include "utils.h"
#include "nfc_utils.h"

/********************/
/* GLOBAL VARIABLES */
/********************/
extern ctap_state_t app_ctap;
bool isKeyGen = false;

/**************************/
/* LOCAL STATIC VARIABLES */
/**************************/
static uint8_t nfc_ctap_response_buffer[CTAPHID_MAX_PAYLOAD_LENGTH];

static ctap_response_t nfc_ctap_response = {
    .data_max_size = sizeof(nfc_ctap_response_buffer) - 1,
    .data = &nfc_ctap_response_buffer[1]
};
/**
 * @brief handle a received CTAP request using the CTAP API. Use the tx_buffer to send the result of the request.
 * Start APDU chaining in case short APDU was used for the request and the response is larger than LE.
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] data_in raw data received from the NFC
 * @param [in] data_in_len length of the received data
 * @param [out] tx_buf TX buffer
*  @param [out] tx_buf_len length of the TX buffer
 * @return response length
 */
static uint16_t fido_handle_ctap(t4t_context_t *ctx, const uint8_t *data_in, size_t data_in_len, uint8_t *tx_buf, uint16_t tx_buf_len);

static uint16_t fido_handle_ctap(t4t_context_t *ctx,
                                 const uint8_t *data_in, size_t data_in_len,
                                 uint8_t *tx_buf, uint16_t tx_buf_len)
{
    if (data_in == NULL || data_in_len == 0U) {
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
    }

    uint8_t        ctap_cmd  = data_in[0];
    const uint8_t *cbor_in   = data_in + 1;
    const size_t   cbor_len  = data_in_len - 1;

    debug_log("NFC: CTAP cmd=0x%02X cbor_len=%u" nl,
              ctap_cmd, (unsigned)cbor_len);

    isKeyGen = (ctap_cmd == CTAP_CMD_MAKE_CREDENTIAL);
    nfc_ctap_response.length   = 0;
    nfc_ctap_response_buffer[0] = ctap_request(&app_ctap, ctap_cmd,
                                        cbor_len, cbor_in, &nfc_ctap_response);
    isKeyGen = false;

    debug_log("NFC: CTAP response length=%u" nl, (unsigned)nfc_ctap_response.length);

    size_t   full_len = 1U + nfc_ctap_response.length;   /* status + CBOR */

    if (ctx->extended) {
        /* extended, don't chain */
        if (full_len > ctx->resp_len_expexted) {
            error_log(red("NFC CTAP: response length %u exceeds expected length %u for extended APDU") nl, full_len, (unsigned)ctx->resp_len_expexted);
            return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
        }
        ctx->extended = false;
        ctx->chain_len = 0U;
        return nfc_build_response(nfc_ctap_response_buffer, full_len, NFC_SW_OK, tx_buf, tx_buf_len);
    }

    /* short, check need for chaining */
    if (full_len <= ctx->resp_len_expexted) {
        ctx->chain_len = 0U;
        ctx->chain_offset = 0U;
        return nfc_build_response(nfc_ctap_response_buffer, full_len, NFC_SW_OK, tx_buf, tx_buf_len);
    }

    if (full_len > sizeof(ctx->chain_buf))
    {
        /* Protect against oversized response */
        debug_log(red("NFC CTAP: response too large for chain buffer (%u)")nl,
                  (unsigned)full_len);
        ctx->chain_len = 0U;
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
    }

    const uint16_t first_chunk = (full_len <= ctx->resp_len_expexted) ? (uint16_t)full_len : ctx->resp_len_expexted;
    memcpy(ctx->chain_buf, nfc_ctap_response_buffer, full_len);
    ctx->chain_offset = first_chunk;
    ctx->chain_len    = (uint16_t)(full_len - first_chunk);
    ctx->chaining_out = true;
    debug_log("Response larger than Le, chaining: total_len=%u, sent=%u, remaining=%u" nl,
              (unsigned)full_len, (unsigned)nfc_ctap_response.length, (unsigned)ctx->chain_len);

    uint8_t remaining = (ctx->chain_len > 0xFFU) ? 0xFFU
                                                  : (uint8_t)ctx->chain_len;
    uint16_t chain_sw = (uint16_t)(NFC_SW_CHAIN | remaining);

    return nfc_build_response(ctx->chain_buf, first_chunk, chain_sw, tx_buf, tx_buf_len);
}

uint16_t fido_handle_ctap_chain_out(t4t_context_t *ctx,
                                         const nfc_apdu_t *apdu,
                                         uint8_t *tx_buf, uint16_t tx_buf_len)
{
    /* GET RESPONSE must be 80 C0 00 00 Le */
    if ((apdu->ins != NFC_INS_GET_RESPONSE) || (apdu->p1 != 0x00U) || (apdu->p2 != 0x00U) || ctx->chaining_out == false)
    {
        debug_log(red("Invalid APDU for GET RESPONSE: INS=0x%02X P1=0x%02X P2=0x%02X") nl, apdu->ins, apdu->p1, apdu->p2);
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_PARAMS);
    }

    /* no chain, return SW error */
    if (ctx->chain_len == 0U)
    {
        return nfc_put_sw(tx_buf, NFC_SW_COND_NOT_SATISFIED);
    }

    const uint16_t le      = (apdu->le == 0U) ? NFC_APDU_SHORT_MAX_LEN: apdu->le;
    const uint16_t to_send = (ctx->chain_len <= le) ? ctx->chain_len : le;

    /* get position */
    uint8_t *chunk = &ctx->chain_buf[ctx->chain_offset];

    /* if this chunk is the last one, send OK status word */
    if ((ctx->chain_len - to_send) == 0U)
    {
        /* Last chunk */
        reset_chain(ctx);
        return nfc_build_response(chunk, to_send, NFC_SW_OK, tx_buf, tx_buf_len);
    }

    ctx->chain_offset += to_send;
    ctx->chain_len    -= to_send;

    const uint8_t remaining = (ctx->chain_len >= NFC_APDU_SHORT_MAX_LEN) ? 0xFFU : (uint8_t)ctx->chain_len;
    const uint16_t chain_sw = (uint16_t)(NFC_SW_CHAIN | remaining);

    return nfc_build_response(chunk, to_send, chain_sw, tx_buf, tx_buf_len);
}

uint16_t fido_handle_ctap_request_extended(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len)
{

    debug_log("Handling extended APDU request: CLA=0x%02X INS=0x%02X P1=0x%02X P2=0x%02X Lc=%u Le=%u" nl,
              apdu->cla, apdu->ins, apdu->p1, apdu->p2, (unsigned)apdu->lc, (unsigned)apdu->le);
    if ((apdu->lc == 0U) || (apdu->data == NULL))
    {
        debug_log(red("NFC CTAP: missing data in APDU") nl);
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_DATA);
    }

    ctx->resp_len_expexted = ((apdu->le == 0) || (apdu->has_le == false)) ? NFC_APDU_EXTENDED_MAX_LEN : apdu->le;
    ctx->extended = true;

    size_t data_in_len = (size_t)(apdu->lc != 0 ? apdu->lc : NFC_APDU_EXTENDED_MAX_LEN);

    return fido_handle_ctap(ctx, apdu->data, data_in_len, tx_buf, tx_buf_len);
}

uint16_t fido_handle_ctap_request_short(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len)
{
    debug_log("Handling short APDU request: CLA=0x%02X INS=0x%02X P1=0x%02X P2=0x%02X Lc=%u Le=%u" nl,
              apdu->cla, apdu->ins, apdu->p1, apdu->p2, (unsigned)apdu->lc, (unsigned)apdu->le);

    if (ctx->chaining_out)
    {
        debug_log(yellow("Received APDU while chaining in progress, treating as chain command") nl);
        ctx->resp_len_expexted = ((apdu->le == 0 ) || (apdu->extended == false))? NFC_APDU_SHORT_MAX_LEN : apdu->le;
        return fido_handle_ctap_chain_in(ctx, apdu, tx_buf, tx_buf_len);
    }

    if ((apdu->lc == 0U) || (apdu->data == NULL))
    {
        debug_log(red("NFC CTAP: missing data in APDU") nl);
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_DATA);
    }

    ctx->resp_len_expexted = ((apdu->le == 0 ) || (apdu->extended == false))? NFC_APDU_SHORT_MAX_LEN : apdu->le;
    ctx->extended = false;

    size_t data_in_len = (size_t)(apdu->lc != 0 ? apdu->lc : NFC_APDU_EXTENDED_MAX_LEN);

    return fido_handle_ctap(ctx, apdu->data, data_in_len, tx_buf, tx_buf_len);
}

uint16_t fido_handle_ctap_chain_in(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len)
{
    debug_log("Handling APDU chain: CLA=0x%02X INS=0x%02X P1=0x%02X P2=0x%02X Lc=%u Le=%u" nl,
              apdu->cla, apdu->ins, apdu->p1, apdu->p2, (unsigned)apdu->lc, (unsigned)apdu->le);
    // first apdu chain command, start chaining
    if (apdu->cla == NFC_CLA_CTAP_CHAIN)
    {
        if (apdu->lc > sizeof(ctx->chain_buf)) {
            reset_chain(ctx);
            return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
        }
        memcpy(ctx->chain_buf, apdu->data, apdu->lc);
        ctx->chain_len = (uint16_t)apdu->lc;
        ctx->chain_offset = 0U;
        ctx->chaining_in = true;
        return nfc_put_sw(tx_buf, NFC_SW_OK);
    }

    if (apdu->cla != NFC_CLA_CTAP)
    {
        return nfc_put_sw(tx_buf, NFC_SW_NOT_FOUND);
    }

    // subsequent apdu chain command, append to chain buffer
    if ((size_t)ctx->chain_len + apdu->lc > sizeof(ctx->chain_buf))
    {
        // protect against overflow
        debug_log(red("NFC CTAP: chain buffer overflow") nl);
        reset_chain(ctx);
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
    }
    memcpy(&ctx->chain_buf[ctx->chain_len], apdu->data, apdu->lc);
    ctx->chain_len += (uint16_t) apdu->lc;

    if (!apdu->has_le)
    {
        // more chaining to come
        return nfc_put_sw(tx_buf, NFC_SW_OK);
    }
    // last chain command, process the full request
    debug_log("Last APDU chain command received, processing full request: total_len=%u" nl, (unsigned)ctx->chain_len);
    ctx->chaining_in = false;
    ctx->resp_len_expexted = (apdu->le == 0U) ? NFC_APDU_SHORT_MAX_LEN : (uint16_t)apdu->le;
    return fido_handle_ctap(ctx, ctx->chain_buf, ctx->chain_len, tx_buf, tx_buf_len);
}

uint16_t fido_handle_ctap_deselect(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf)
{
    if (apdu->cla != NFC_CLA_CTAP || apdu->ins != NFC_INS_DESELECT)
    {
        return nfc_put_sw(tx_buf, NFC_SW_NOT_FOUND);
    }
    ctx->selected_app = APP_NONE;
    reset_chain(ctx);
    return nfc_put_sw(tx_buf, NFC_SW_OK);
}

void ctap_nfc_start_user_presence_timer(nfc_user_presence_timer_t* t)
{
    if (t == NULL) {
        error_log(red("NFC: ERROR: Invalid timer structure") nl);
        return;
    }
    debug_log(cyan("CE: start user presence timer") nl);
    t->begin_timestamp = ctap_get_current_time();
    t->threshold = CTAP_MAX_USER_PRESENCE_TIME_LIMIT_NFC;
    app_ctap.nfc_user_present = true;
    app_ctap.nfc_ctap_in_use = true;
}

bool ctap_nfc_is_user_presence_timer_expired(const nfc_user_presence_timer_t* t)
{
    //always return false when ctap is not in use since there is nothing to expire
    if (app_ctap.nfc_ctap_in_use == false) return false;

    if (t == NULL) {
        error_log(red("NFC: ERROR: Invalid timer structure") nl);
        return true; // treat as expired if invalid
    }

    const uint32_t current_time = ctap_get_current_time();

    if ((current_time - t->begin_timestamp) >= t->threshold) {
        return true;
    }
    return false;
}

void ctap_nfc_stop_user_presence_timer(nfc_user_presence_timer_t* t)
{
    if (t == NULL)
    {
        error_log(red("NFC: ERROR: Invalid timer structure") nl);
        return;
    }
    debug_log(cyan("CE: stop user presence timer") nl);
    app_ctap.nfc_user_present = false;
    app_ctap.nfc_ctap_in_use = false;
}