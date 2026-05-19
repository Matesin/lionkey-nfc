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

/********************/
/* GLOBAL VARIABLES */
/********************/
extern ctap_state_t app_ctap;
bool isKeyGen = false;

/**************************/
/* LOCAL STATIC VARIABLES */
/**************************/

/*
 * FIDO applet AID (11.3.3. Applet selection)
 */
static const uint8_t FIDO_AID[]  = { 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01 };
/*
 * FIDO version to be returned upon FIDO_AID is received (11.3.3 Applet selection)
 *
 * If the authenticator ONLY implements CTAP2, the device SHALL respond with "FIDO_2_0", or 0x4649444f5f325f30.
 */
static const uint8_t FIDO_VERSION[] = {'F', 'I', 'D', 'O', '_', '2', '_', '0'}; // FIDO_2_0
/*
 * NFC Forum NDEF Tag Application AID
 */
static const uint8_t NDEF_AID[]  = { 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01 };

static uint8_t nfc_ctap_response_buffer[CTAPHID_MAX_PAYLOAD_LENGTH];

static ctap_response_t nfc_ctap_response = {
    .data_max_size = sizeof(nfc_ctap_response_buffer) - 1,
    .data = &nfc_ctap_response_buffer[1]
};

/******************************/
/* STATIC FUNCTION PROTOTYPES */
/******************************/
/**
 * @brief read 2 bytes in big endian format from a buffer and return a 2-byte number
 *
 * @param buf buffer containing the 2 bytes to read as a big-endian uint16_t
 * @return the 2-byte number read from the buffer in big-endian format
 */
static inline uint16_t read_16be(const uint8_t *buf){ return ((uint16_t)buf[0] << 8) | buf[1];}
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

/**
 * @brief handle a received extended CTAP APDU, update T4T context. Pass the request to fido_handle_ctap for processing and building the response.
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @param [out] tx_buf_len length of the TX buffer
 * @return response length
 */
static uint16_t fido_handle_ctap_request_extended(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

/**
 * @brief handle a received short CTAP APDU, update T4T context. Pass the request to fido_handle_ctap for processing and building the response.
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @param [out] tx_buf_len length of the TX buffer
 *
 * @return total length of the response (payload + SW) if success
 * @return 2-byte SW if fail
 */
static uint16_t fido_handle_ctap_request_short(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

/**
 * @brief handle chaining out. Send the next chunk of the response from the chain buffer in T4T context. Return fail if there is no chaining.
 *
 * @cite ISO/IEC 7816-4:2020 section 5.2.3
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @param [out] tx_buf_len length of the TX buffer
 *
 * @return total response length (payload + SW) if success
 * @return 2-byte SW if fail (i.e., if there is no chaining in progress or if the APDU is malformed for chaining)
 */
static uint16_t fido_handle_ctap_chain_out(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @param [out] tx_buf_len length of the TX buffer
 *
 * @return total response length (payload + SW) if success
 */
static uint16_t fido_handle_ctap_chain_in(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @return total response length (SW only -> 2B at all times)
 */
static uint16_t fido_handle_ctap_deselect(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf);

/**
 *
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] rsp response buffer
 * @return total response length (in this case status word only, meaning two bytes)
 */
static uint16_t ndef_handle_select(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] rsp response buffer
 * @param [out] rsp_len length of the response
 * @return total response length
 */
static uint16_t ndef_handle_read(const t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp, uint16_t rsp_len);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @return total response length (in this case status word only, meaning two bytes)
 */
static uint16_t ndef_handle_update(const t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf);

/**
 *
 * @param ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 */
static void reset_chain(t4t_context_t *ctx);

/********************/
/* GLOBAL FUNCTIONS */
/********************/
apdu_parse_status_t nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out) {
    /* check for valid structure */
    if ((raw == NULL) || (out == NULL))
    {
        debug_log(red("APDU parsing error: invalid input") nl);
        return APDU_ERR_MALFORMED;
    }

    if (raw_len < 4U)
    {
        return  APDU_ERR_TOO_SHORT;
    }

    out->cla  = raw[0];
    out->ins  = raw[1];
    out->p1   = raw[2];
    out->p2   = raw[3];
    out->data = NULL;
    out->lc   = 0U;
    out->le   = 0U;
    out->extended = false;
    out->has_le = false;

    /* Short APDU */
    /* Case 1S */
    if (raw_len == 4U)
    {
        return APDU_PARSE_OK;
    }

    if (raw[4] != 0x00U)
    {
        const size_t b1 = (size_t)raw[4];

        /* Case 2S */
        if (raw_len == 5U)
        {
            out->le = b1;
            out->has_le = true;
            return APDU_PARSE_OK;
        }

        /* Case 3S */
        if (raw_len == (5U + b1))
        {
            out->lc = b1;
            out->data = &raw[5];
            return APDU_PARSE_OK;
        }

        /* Case 4S */
        if (raw_len == (6U + b1))
        {
            out->lc = b1;
            out->data = &raw[5];
            out->le = raw[5U + b1]; // Le = 0 stands for 256
            out->has_le = true;
            return APDU_PARSE_OK;
        }
        debug_log(red("APDU parsing error: invalid short APDU length") nl);
        return APDU_ERR_MALFORMED;
    }

    /* Extended APDU */
    if (raw_len < 7)
    {
        debug_log(red("APDU parsing error: invalid extended APDU length") nl);
        return APDU_ERR_MALFORMED;
    }

    const uint16_t ext = read_16be(&raw[5]);

    out->extended = true;

    /* Case 2E: CLA INS P1 P2 00 Le1 Le2 */
    if (raw_len == 7U)
    {
        out->le = (size_t) ext; // ext = 0 stands for 65536
        out->has_le = true;
        return APDU_PARSE_OK;
    }

    /* For 3E and 4E ext is Lc, hence Lc = 0 is not valid here */
    if (ext == 0U)
    {
        return APDU_ERR_MALFORMED;
    }

    /* Case 3E */
    if (raw_len == (7U + (size_t)ext))
    {
        out->lc = (size_t) ext;
        if (out->lc < raw_len - 7U)
        {
            debug_log(red("APDU parsing error: extended APDU length mismatch") nl);
            return APDU_ERR_MALFORMED;
        }
        out->data = &raw[7];
        return APDU_PARSE_OK;
    }

    /* Case 4E */
    if (raw_len == (9U + (size_t)ext))
    {
        const uint16_t le16 = read_16be(&raw[7U + (size_t)ext]);
        out->lc = (size_t) ext;
        if (out->lc < raw_len - 9U)
        {
            debug_log(red("APDU parsing error: extended APDU length mismatch") nl);
            return APDU_ERR_MALFORMED;
        }
        out->data = &raw[7];
        out->le = le16; // Le = 0 stands for 65536
        out->has_le = true;
        return APDU_PARSE_OK;
    }
    debug_log(red("APDU parsing error: invalid extended APDU length")nl);
    return APDU_ERR_MALFORMED;
}

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

static uint16_t fido_handle_ctap_chain_out(t4t_context_t *ctx,
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

static uint16_t fido_handle_ctap_request_extended(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len)
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

static uint16_t fido_handle_ctap_request_short(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len)
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

static uint16_t fido_handle_ctap_chain_in(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len)
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

static uint16_t fido_handle_ctap_deselect(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf)
{
    if (apdu->cla != NFC_CLA_CTAP || apdu->ins != NFC_INS_DESELECT)
    {
        return nfc_put_sw(tx_buf, NFC_SW_NOT_FOUND);
    }
    ctx->selected_app = APP_NONE;
    reset_chain(ctx);
    return nfc_put_sw(tx_buf, NFC_SW_OK);
}

static uint16_t ndef_handle_select(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp)
{
    uint16_t fid;

    /* SELECT for T4T should carry data */
    if (apdu->data == NULL)
    {
        return nfc_put_sw(rsp, NFC_SW_WRONG_LENGTH);
    }

    if ((apdu->p1 == 0x04U) && (apdu->p2 == 0x00U))
    {
        if ((apdu->lc == sizeof(NDEF_AID)) &&
            (memcmp(apdu->data, NDEF_AID, sizeof(NDEF_AID))) == 0)
        {
            ctx->selected_app  = APP_NDEF;
            ctx->selected_file = FILE_NONE;
            return nfc_put_sw(rsp, NFC_SW_OK);
        }

        ctx->selected_file = FILE_NONE;
        return nfc_put_sw(rsp, NFC_SW_FILE_NOT_FOUND);
    }

    /* Select by file ID */
    if ((apdu->p1 == 0x00U) && (apdu->p2 == 0x0CU))
    {
        if ((ctx->selected_app < APP_NDEF) || (apdu->lc != 2U))
        {
            return nfc_put_sw(rsp, NFC_SW_FILE_NOT_FOUND);
        }

        fid = ((uint16_t)apdu->data[0] << 8) | apdu->data[1];

        if (fid == ctx->fid_cc)
        {
            ctx->selected_file = FILE_CC;
            return nfc_put_sw(rsp, NFC_SW_OK);
        }

        if (fid == ctx->fid_ndef)
        {
            ctx->selected_file = FILE_NDEF;
            return nfc_put_sw(rsp, NFC_SW_OK);
        }

        ctx->selected_file = FILE_NONE;
        return nfc_put_sw(rsp, NFC_SW_FILE_NOT_FOUND);
    }

    return nfc_put_sw(rsp, NFC_SW_FUNC_NOT_SUPPORTED);
}

static uint16_t ndef_handle_read(const t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp, uint16_t rsp_len)
{
    const uint8_t *src = NULL;
    uint16_t src_len = 0U;
    uint16_t offset = ((uint16_t)apdu->p1 << 8) | apdu->p2;
    uint16_t to_read = apdu->le;

    if (rsp_len < 2)
    {
        return nfc_put_sw(rsp, NFC_SW_WRONG_LENGTH);
    }

    switch (ctx->selected_file) {
    case FILE_CC:
        debug_log(magenta("selected CC file")nl);
        src     = ctx->cc_file;
        src_len = ctx->cc_file_len;
        break;
    case FILE_NDEF:
        debug_log(magenta("selected NDEF file")nl);
        src     = ctx->ndef_file;
        src_len = ctx->ndef_file_len;
        break;
    default:
        return nfc_put_sw(rsp, NFC_SW_COND_NOT_SATISFIED);
    }

    if (offset > src_len)
    {
        return nfc_put_sw(rsp, NFC_SW_WRONG_PARAMS);
    }

    if ((uint32_t)offset + to_read > src_len)
    {
        to_read = (uint16_t)(src_len - offset);
    }

    if (rsp_len < (uint16_t)(to_read + 2U))
    {
        return nfc_put_sw(rsp, NFC_SW_NOT_ENOUGH_MEMORY);
    }
    debug_log(magenta("Response: offset=%u to_read=%u src=%u src_len=%u rsp_len=%u")nl, offset, to_read, *src, src_len, rsp_len);

    return nfc_build_response(&src[offset], to_read, NFC_SW_OK, rsp, rsp_len);
}

static uint16_t ndef_handle_update(const t4t_context_t *ctx,
                                   const nfc_apdu_t *apdu,
                                   uint8_t *tx_buf)
{
    uint16_t offset = ((uint16_t)apdu->p1 << 8) | apdu->p2;

    if (!ctx->ndef_write_allowed) {
        return nfc_put_sw(tx_buf, NFC_SW_SECURITY_STATUS_NOT_SAT);
    }
    if (apdu->data == NULL || apdu->lc == 0U) {
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
    }
    if (ctx->selected_file != FILE_NDEF) {
        return nfc_put_sw(tx_buf, NFC_SW_COND_NOT_SATISFIED);
    }
    if ((uint32_t)offset + apdu->lc > ctx->ndef_file_len) {
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_PARAMS);
    }

    memcpy(&ctx->ndef_file[offset], apdu->data, apdu->lc);
    return nfc_put_sw(tx_buf, NFC_SW_OK);
}


uint16_t nfc_parse_and_respond(t4t_context_t *ctx, uint8_t *rx_data, uint16_t rx_data_len, uint8_t *tx_buf, uint16_t tx_buf_len )
{
    nfc_apdu_t apdu;
    apdu_parse_status_t err;

    if ((ctx == NULL) || (tx_buf == NULL) || (tx_buf_len < 2U)) {
        error_log(red("NFC: ERROR: Invalid response buffer") nl);
        return NFC_PARSE_WRONG_SIZE;
    }

    /* parse APDU */
    err = nfc_parse_apdu(rx_data, rx_data_len, &apdu);
    if (err != APDU_PARSE_OK)
    {
        error_log(red("NFC: ERROR: Failed to parse APDU (Error code: %u)") nl, err);
        return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
    } else debug_log(green("NFC: APDU parsed successfully") nl);

    /* handle AID */
    if ((apdu.cla == NFC_CLA_ISO) &&
        (apdu.ins == NFC_INS_SELECT) &&
        (apdu.p1  == 0x04U) &&
        (apdu.p2  == 0x00U))
    {
        if ((apdu.data) == NULL || (apdu.lc == 0U)) {
            return nfc_put_sw(tx_buf, NFC_SW_WRONG_LENGTH);
        }

        /* FIDO selected (as per 11.3.3. Applet selection) */
        if ((apdu.lc == sizeof(FIDO_AID)) &&
            (memcmp(apdu.data, FIDO_AID, sizeof(FIDO_AID))) == 0)
        {
            ctx->selected_app  = APP_FIDO;
            ctx->selected_file = FILE_NONE;
            reset_chain(ctx); // reset any previous chaining state
            debug_log(blue("NFC: FIDO AID selected") nl);
            app_ctap.nfc_timer.nfc_ctap_in_use = true;
            /* return FIDO version*/
            return nfc_build_response(FIDO_VERSION, sizeof(FIDO_VERSION), NFC_SW_OK, tx_buf, tx_buf_len);
        }

        /* NDEF selected, proceed with handshake */
        if ((apdu.lc == sizeof(NDEF_AID)) &&
            (memcmp(apdu.data, NDEF_AID, sizeof(NDEF_AID))) == 0)
        {
            ctx->selected_app  = APP_NDEF;
            ctx->selected_file = FILE_NONE;
            reset_chain(ctx);
            debug_log(green("NFC: NDEF AID selected") nl);
            return nfc_put_sw(tx_buf, NFC_SW_OK);
        }

        /* Unknown request */
        ctx->selected_app  = APP_NONE;
        ctx->selected_file = FILE_NONE;
        return nfc_put_sw(tx_buf, NFC_SW_FILE_NOT_FOUND);
    }

    /* Decide on action based on the currently selected applet */
    switch (ctx->selected_app)
    {
    case APP_FIDO:
        /* first chunk of chaining in incoming */
            if (((apdu.cla == NFC_CLA_CTAP_CHAIN) && (apdu.ins == NFC_INS_CTAP)) ||
                (ctx->chaining_in == true))
            {
                return fido_handle_ctap_chain_in(ctx, &apdu, tx_buf, tx_buf_len);
            }
            if (apdu.cla == NFC_CLA_CTAP)
            {
                switch (apdu.ins)
                {
                    case NFC_INS_DESELECT:
                        return fido_handle_ctap_deselect(ctx, &apdu, tx_buf);
                    case NFC_INS_CTAP:
                        if (apdu.extended == true)
                        {
                            return fido_handle_ctap_request_extended(ctx, &apdu, tx_buf, tx_buf_len);
                        }
                        return fido_handle_ctap_request_short(ctx, &apdu, tx_buf, tx_buf_len);
                    case NFC_INS_GET_RESPONSE:
                        return fido_handle_ctap_chain_out(ctx, &apdu, tx_buf, tx_buf_len);
                    default:
                        return nfc_put_sw(tx_buf, NFC_SW_INS_NOT_SUPPORTED);
                }
            }

            return nfc_put_sw(tx_buf, NFC_SW_CLA_NOT_SUPPORTED);

        case APP_NDEF:
            switch (apdu.ins)
            {
                case T4T_INS_SELECT:
                    debug_log(yellow("NDEF: SELECT")nl);
                    return ndef_handle_select(ctx, &apdu, tx_buf);
                case T4T_INS_READ:
                    debug_log(yellow("NDEF: READ")nl);
                    return ndef_handle_read(ctx, &apdu, tx_buf, tx_buf_len);
                case T4T_INS_UPDATE:
                    debug_log(yellow("NDEF: UPDATE")nl);
                    return ndef_handle_update(ctx, &apdu, tx_buf);
                default:
                    return nfc_put_sw(tx_buf, NFC_SW_INS_NOT_SUPPORTED);
            }
        default:
            return nfc_put_sw(tx_buf, NFC_SW_COND_NOT_SATISFIED);
    }
}

uint16_t nfc_put_sw(uint8_t *buf, uint16_t sw)
{
    if (buf == NULL)
    {
        return 0U;
    }
    buf[0] = (uint8_t)(sw >> 8);
    buf[1] = (uint8_t)(sw & 0xFF);
    return 2U;
}

size_t nfc_build_response(
    const uint8_t *data, size_t data_len,
    uint16_t sw,
    uint8_t *out, size_t out_size
) {
    if ((out == NULL) || (data == NULL))
    {
        return 0U;
    }

    if (data_len > (out_size - 2U))
    {
        return 0U;
    }
    if (data_len > 0U)
    {
        (void)memmove(out, data, data_len);
    }
    out[data_len]     = (sw >> 8U) & 0xFF;
    out[data_len + 1U] =  sw       & 0xFF;
    return data_len + 2U;
}

void ctap_nfc_start_user_presence_timer(nfc_user_presence_timer_t* t)
{
    if (t == NULL) {
        error_log(red("NFC: ERROR: Invalid timer structure") nl);
        return;
    }
    debug_log(cyan("CE: start user presence timer") nl);
    t->nfc_user_present = true;
    t->begin_timestamp = ctap_get_current_time();
    t->threshold = CTAP_MAX_USER_PRESENCE_TIME_LIMIT_NFC;
}

bool ctap_nfc_is_user_presence_timer_expired(const nfc_user_presence_timer_t* t)
{
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
    t->nfc_ctap_in_use = false;
    t->nfc_user_present = false;
}

static void reset_chain(t4t_context_t *ctx)
{
    ctx->chain_len = 0U;
    ctx->chain_offset = 0U;
    ctx->chaining_in = false;
    ctx->chaining_out = false;
}