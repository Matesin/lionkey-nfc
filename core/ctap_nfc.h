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
#define NFC_INS_SELECT          0xA4
#define NFC_INS_CTAP            0x10
#define NFC_INS_DESELECT        0x12
#define NFC_INS_GET_RESPONSE    0xC0U


#define CTAP_MAKE_CREDENTIAL       0x01
#define CTAP_GET_ASSERTION         0x02
#define CTAP_GET_NEXT_ASSERTION    0x08
#define CTAP_GET_INFO              0x04
#define CTAP_GET_PIN               0x06
#define CTAP_RESET                 0x07


/* Class byte values */
#define NFC_CLA_ISO         0x00
#define NFC_CLA_CTAP        0x80
#define NFC_PARSE_WRONG_SIZE 1U

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