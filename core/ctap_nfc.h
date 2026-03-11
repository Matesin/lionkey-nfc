//
// Created by Maty Martan on 11.03.2026.
//

#ifndef LIONKEY_CTAP_NFC_H
#define LIONKEY_CTAP_NFC_H

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ctaphid.h"

//TODO: Add doxygen

/* NFC-A CE config */
#define NFC_FIDO_AID        { 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01 }
#define NFC_FIDO_AID_LEN    8

/* Instruction byte values */
#define NFC_INS_SELECT      0xA4
#define NFC_INS_CTAP        0x10
#define NFC_INS_DESELECT    0x12

/* Class byte values */
#define NFC_CLA_ISO         0x00
#define NFC_CLA_FIDO        0x80

/* Status Words */
#define NFC_SW_OK           0x9000
#define NFC_SW_NOT_FOUND    0x6A82
#define NFC_SW_WRONG_DATA   0x6A80
#define NFC_SW_INS_UNKNOWN  0x6D00
#define NFC_SW_CLA_UNKNOWN  0x6E00
#define NFC_SW_CONDITIONS   0x6985
#define NFC_SW_WRONG_LENGTH 0x6700

typedef struct {
    uint8_t  cla;
    uint8_t  ins;
    uint8_t  p1;
    uint8_t  p2;
    uint16_t lc;        /* data length */
    uint8_t *data;      /* CTAP cmd + CBOR */
    uint16_t le;
} nfc_apdu_t;

typedef struct {
    uint8_t  *buf;
    size_t    len;
    uint16_t  sw;       /* status word (9000, 6A82 ...) */
} nfc_rapdu_t;

bool nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out);
size_t nfc_build_response(const uint8_t *data, size_t data_len, uint16_t sw, uint8_t *out_buf, size_t out_buf_size);
void nfc_process_apdu(const nfc_apdu_t *apdu, uint8_t *resp_buf, size_t resp_buf_size, size_t *resp_len, uint16_t *sw);


#endif //LIONKEY_CTAP_NFC_H