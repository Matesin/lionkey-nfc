//
// Created by Maty Martan on 11.03.2026.
//
#include <string.h>
#include "terminal.h"
#include "ctap_nfc.h"
#include "ctap.h"
#include "main.h"

#include "utils.h"

static const uint8_t FIDO_AID[] = NFC_FIDO_AID;
static bool app_selected = false;
extern ctap_state_t app_ctap;

static uint8_t nfc_ctap_response_buffer[CTAPHID_MAX_PAYLOAD_LENGTH];

static ctap_response_t nfc_ctap_response = {
    .data_max_size = sizeof(nfc_ctap_response_buffer) - 1,
    .data = &nfc_ctap_response_buffer[1]
};

/**
 * @brief Parses raw APDU bytes into the nfc_apdu_t structure
 *
 * @param raw APDU bytes received from the NFC reader
 * @param raw_len length of the raw APDU data
 * @param out [out] parsed APDU structure
 *
 * @return true if parsing was successful, false if the APDU format is invalid
 *
 */
apdu_parse_status_t nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out) {
    if (raw_len < 4) return  APDU_ERR_TOO_SHORT;

    out->cla  = raw[0];
    out->ins  = raw[1];
    out->p1   = raw[2];
    out->p2   = raw[3];
    out->data = NULL;
    out->lc   = 0U;
    out->le   = 0U;

    if (raw_len == 4) return APDU_PARSE_OK;

    if (raw_len == 5U)
    {
        out->le = raw[4];
        return APDU_PARSE_OK; /* Case 2S */
    }

    /* len >= 6 */
    {
        uint8_t lc = raw[4];

        /* Case 3S: 4 header + 1 Lc + Lc data */
        if (raw_len == (size_t)(5U + lc))
        {
            out->lc = lc;
            out->data = &raw[5];
            return APDU_PARSE_OK;
        }

        /* Case 4S: 4 header + 1 Lc + Lc data + 1 Le */
        if (raw_len == (size_t)(6U + lc))
        {
            out->lc = lc;
            out->data = &raw[5];
            out->le = raw[5U + lc];
            return APDU_PARSE_OK;
        }
    }

    return APDU_ERR_MALFORMED;
}

/**
 * @brief Receives a parsed APDU, processes it according to the INS and CLA,
 * and prepares the response data and status word
 *
 * @param apdu      parsed APDU structure
 * @param resp_buf  reponse buffer
 * @param resp_len  [out] length of the response data (excluding SW1, SW2)
 * @param sw        [out] status word (NFC_SW_OK etc.)
 *
 */
void nfc_process_apdu(
    const nfc_apdu_t *apdu,
    uint8_t *resp_buf,
    size_t resp_buf_size,
    size_t *resp_len,
    uint16_t *sw
) {
    *resp_len = 0;
    *sw = NFC_SW_OK;


    if (apdu->ins == NFC_INS_SELECT && apdu->cla == NFC_CLA_ISO) {
        if (apdu->lc == NFC_FIDO_AID_LEN &&
            memcmp(apdu->data, FIDO_AID, NFC_FIDO_AID_LEN) == 0)
        {
            app_selected = true;
            debug_log("NFC: FIDO AID selected" nl);
            *sw = NFC_SW_OK;
        } else {
            app_selected = false;
            *sw = NFC_SW_NOT_FOUND;
        }
        return;
    }

    /* DESELECT */
    if (apdu->ins == NFC_INS_DESELECT) {
        app_selected = false;
        *sw = NFC_SW_OK;
        return;
    }

    /* CTAP command */
    if (apdu->ins == NFC_INS_CTAP && apdu->cla == NFC_CLA_FIDO) {
        if (!app_selected) {
            *sw = NFC_SW_CONDITIONS;
            return;
        }
        if (apdu->lc == 0 || apdu->data == NULL) {
            *sw = NFC_SW_WRONG_DATA;
            return;
        }

        /* apdu->data[0]    = CTAP command byte (0x01 makeCredential atd.) */
        /* apdu->data[1..]  = CBOR payload                                  */
        uint8_t ctap_cmd    = apdu->data[0];
        const uint8_t *cbor = apdu->data + 1;
        size_t cbor_len     = apdu->lc - 1;

        debug_log("NFC: CTAP cmd=0x%02X cbor_len=%u" nl, ctap_cmd, (unsigned)cbor_len);

        nfc_ctap_response.length = 0;
        nfc_ctap_response_buffer[0] = ctap_request(&app_ctap, ctap_cmd, cbor_len, cbor, &nfc_ctap_response);
        *resp_len = 1 + nfc_ctap_response.length;
        if (*resp_len > resp_buf_size) {
            *sw = NFC_SW_WRONG_LENGTH;
            *resp_len = 0;
            debug_log(red("NFC CTAP ERROR: wrong response length %u") nl, *resp_len);
            return;
        }
        memcpy(resp_buf, nfc_ctap_response_buffer, *resp_len);

        *sw = NFC_SW_OK;
        return;
    }

    *sw = NFC_SW_INS_UNKNOWN;
}
uint16_t nfc_handle_select(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp)
{
    static const uint8_t ndef_aid[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};

    uint16_t fid;

    if ((ctx == NULL) || (apdu == NULL))
    {
        debug_log(red("NFC ERROR: Invalid arguments")nl);
        return 0;
    }

    /* SELECT for T4T should carry data */
    if (apdu->data == NULL)
    {
        return nfc_put_sw(rsp, NFC_SW_WRONG_LENGTH);
    }

    /* Select by DF name (AID) */
    if ((apdu->p1 == 0x04U) && (apdu->p2 == 0x00U))
    {
        if ((apdu->lc == sizeof(ndef_aid)) &&
            (memcmp(apdu->data, ndef_aid, sizeof(ndef_aid)) == 0))
        {
            ctx->state = STATE_APP_SELECTED;
            ctx->selected_file = FILE_NONE;
            return nfc_put_sw(rsp, NFC_SW_OK);
        }

        ctx->state = STATE_IDLE;
        ctx->selected_file = FILE_NONE;
        return nfc_put_sw(rsp, NFC_SW_FILE_NOT_FOUND);
    }

    /* Select by file ID */
    if ((apdu->p1 == 0x00U) && (apdu->p2 == 0x0CU))
    {
        if ((ctx->state < STATE_APP_SELECTED) || (apdu->lc != 2U))
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
uint16_t nfc_handle_read(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp, uint16_t rsp_len)
{
    const uint8_t *src = NULL;
    uint16_t src_len = 0U;
    uint16_t offset = ((uint16_t)apdu->p1 << 8) | apdu->p2;
    uint16_t to_read = apdu->le;

    if (rsp_len < 2)
    {
        // TODO: Better error handling
        return nfc_put_sw(rsp, NFC_SW_WRONG_LENGTH);
    }

    if (ctx->selected_file == FILE_NONE)
    {
        return nfc_put_sw(rsp, NFC_SW_FILE_SELECTED);
    }

    if (ctx->selected_file == FILE_CC)
    {
        src = ctx->cc_file;
        src_len = ctx->cc_file_len;
    }
    else if (ctx->selected_file == FILE_NDEF)
    {
        src = ctx->ndef_file;
        src_len = ctx->ndef_file_len;
    }
    else
    {
        return nfc_put_sw(rsp, NFC_SW_FILE_NOT_FOUND);
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

    if (to_read > 0U)
    {
        memcpy(rsp, &src[offset], to_read);
    }

    rsp[to_read]     = 0x90U;
    rsp[to_read + 1] = 0x00U;
    return (uint16_t)(to_read + 2U);
}

uint16_t nfc_handle_update(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp)
{
    const uint16_t offset = ((uint16_t)apdu->p1 << 8) | apdu->p2;

    if (!ctx->ndef_write_allowed)
    {
        return nfc_put_sw(rsp, NFC_SW_SECURITY_STATUS_NOT_SAT);
    }

    if (apdu->data == NULL)
    {
        return nfc_put_sw(rsp, NFC_SW_WRONG_LENGTH);
    }

    if (ctx->selected_file != FILE_NDEF)
    {
        return nfc_put_sw(rsp, NFC_SW_FILE_NOT_FOUND);
    }


    if ((uint32_t)offset + apdu->lc > ctx->ndef_file_len)
    {
        return nfc_put_sw(rsp, 0x6282U);
    }

    memcpy(&ctx->ndef_file[offset], apdu->data, apdu->lc);

    return nfc_put_sw(rsp, NFC_SW_OK);
}


uint16_t nfc_parse_and_respond(t4t_context_t *ctx, uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf, uint16_t txBufLen )
{
    nfc_apdu_t apdu;
    apdu_parse_status_t err;

    if (txBuf == NULL || txBufLen < 2) {
        Error_Handler();
        return 0;
    }

    err = nfc_parse_apdu(rxData, rxDataLen, &apdu);

    if (err != APDU_PARSE_OK) {
        return nfc_put_sw(txBuf, NFC_SW_COND_NOT_SATISFIED);
    }

    switch(rxData[1])
    {
    case T4T_INS_SELECT:
        return nfc_handle_select(ctx, &apdu, txBuf);

    case T4T_INS_READ:
        return nfc_handle_read(ctx, &apdu, txBuf, txBufLen);

    case T4T_INS_UPDATE:
        return nfc_handle_update(ctx, &apdu, txBuf);

    default:
        /* MISRA 16.4: No empty case allowed */
        break;
    }
    return nfc_put_sw(txBuf, NFC_SW_INS_NOT_SUPPORTED);
}

/**
 * @brief Builds the final APDU response by concatenating the response data and appending the status word
 *
 * @param [in] data response data bytes (e.g. CBOR-encoded CTAP response)
 * @param [in] data_len length of the response data
 * @param [in] sw status word to append (e.g. NFC_SW_OK)
 * @param [in/out] out output buffer to write the final APDU response (data + SW1, SW2)
 * @param [in/out] out_size size of the output buffer
 *
 * @return total length of the APDU response written to out, or 0 if out_size is insufficient
 */
size_t nfc_build_response(
    const uint8_t *data, size_t data_len,
    uint16_t sw,
    uint8_t *out, size_t out_size
) {
    if (out_size < data_len + 2) return 0;
    if (data_len > 0) memcpy(out, data, data_len);
    out[data_len]     = (sw >> 8) & 0xFF;
    out[data_len + 1] =  sw       & 0xFF;
    return data_len + 2;
}
