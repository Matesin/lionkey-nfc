//
// Created by Maty Martan on 11.03.2026.
//

#ifndef LIONKEY_CTAP_NFC_H
#define LIONKEY_CTAP_NFC_H

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ctaphid.h"
#include "nfc.h"

//TODO: Add doxygen

/* Instruction byte values */
#define NFC_INS_SELECT          0xA4U
#define NFC_INS_CTAP            0x10U
#define NFC_INS_DESELECT        0x12U
#define NFC_INS_GET_RESPONSE    0xC0U

#define NFC_INS_CTAP_CONTROL       0x12U

/* Class byte values */
#define NFC_CLA_ISO         0x00
#define NFC_CLA_CTAP        0x80

#define NFC_PARSE_WRONG_SIZE 1U

/* read 2 bytes in big endian format from a buffer and return a 2-byte number */
static inline uint16_t read_16be(const uint8_t *buf){ return ((uint16_t)buf[0] << 8) | buf[1];}

/* APDU structure (as per https://www.cardlogix.com/glossary/apdu-application-protocol-data-unit-smart-card/) */
typedef struct {
    uint8_t  cla;           /* class */
    uint8_t  ins;           /* instruction */
    uint8_t  p1;            /* parameter 1 */
    uint8_t  p2;            /* parameter 2 */
    uint16_t lc;            /* data length */
    const uint8_t *data;    /* CTAP cmd + CBOR */
    uint16_t le;            /* exp resp len */
} nfc_apdu_t;

typedef enum
{
    APDU_PARSE_OK = 0,
    APDU_ERR_NULL,
    APDU_ERR_TOO_SHORT,
    APDU_ERR_MALFORMED,
    APDU_ERR_UNSUPPORTED_CASE,
    APDU_ERR_OTHER
} apdu_parse_status_t;

apdu_parse_status_t nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out);
void nfc_process_apdu(const nfc_apdu_t *apdu, uint8_t *resp_buf, size_t resp_buf_size, size_t *resp_len, uint16_t *sw);
uint16_t nfc_handle_select(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp);
uint16_t nfc_handle_deselect(t4t_context_t *ctx, uint8_t *rsp);
uint16_t nfc_handle_read(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp, uint16_t rsp_len);
uint16_t nfc_handle_update(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp);
uint16_t nfc_parse_and_respond(t4t_context_t *ctx, uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf, uint16_t txBufLen );
uint16_t nfc_put_sw(uint8_t *buf, uint16_t sw );
size_t nfc_build_response(const uint8_t *data, size_t data_len, uint16_t sw, uint8_t *out, size_t out_size);
#endif //LIONKEY_CTAP_NFC_H