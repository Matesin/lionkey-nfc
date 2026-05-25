//
// Created by Maty Martan on 24.05.2026.
//
#include <string.h>


#include <stdint.h>

#include "utils.h"
#include "nfc_utils.h"
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


void reset_chain(t4t_context_t *ctx)
{
    ctx->chain_len = 0U;
    ctx->chain_offset = 0U;
    ctx->chaining_in = false;
    ctx->chaining_out = false;
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

