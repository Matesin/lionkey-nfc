//
// Created by Maty Martan on 24.05.2026.
//

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ctap_nfc.h"
#include "nfc.h"
#include "nfc_utils.h"
#include "terminal.h"
#include "utils.h"

uint16_t ndef_handle_select(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp)
{
    uint16_t fid;

    /* SELECT for T4T should carry data */
    if (apdu->data == NULL)
    {
        return nfc_put_sw(rsp, NFC_SW_WRONG_LENGTH);
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

uint16_t ndef_handle_read(const t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp, uint16_t rsp_len)
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

uint16_t ndef_handle_update(const t4t_context_t *ctx,
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