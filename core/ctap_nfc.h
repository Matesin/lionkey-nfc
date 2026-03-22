//
// Created by Maty Martan on 11.03.2026.
//

#ifndef LIONKEY_CTAP_NFC_H
#define LIONKEY_CTAP_NFC_H

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "nfc.h"

//TODO: Add doxygen

/* Instruction byte values */
#define NFC_INS_SELECT                          0xA4U
#define NFC_INS_CTAP                            0x10U
#define NFC_INS_DESELECT                        0x12U
#define NFC_INS_GET_RESPONSE                    0xC0U
#define NFC_INS_CTAP_CONTROL                    0x12U

#define CTAP_MAX_USER_PRESENCE_TIME_LIMIT_NFC   (12U * 1000U) // 12s (in ms)

/* Class byte values */
#define NFC_CLA_ISO                             0x00
#define NFC_CLA_CTAP                            0x80

#define NFC_PARSE_WRONG_SIZE                    1U

#define NFC_APDU_SHORT_MAX_LEN                  256U
#define NFC_APDU_EXTENDED_MAX_LEN               65536U

extern bool nfc_user_present;

/* APDU structure (as per https://www.cardlogix.com/glossary/apdu-application-protocol-data-unit-smart-card/) */
typedef struct {
    uint8_t  cla;           /* class */
    uint8_t  ins;           /* instruction */
    uint8_t  p1;            /* parameter 1 */
    uint8_t  p2;            /* parameter 2 */
    uint16_t lc;            /* data length */
    const uint8_t *data;    /* CTAP cmd + CBOR */
    uint16_t le;            /* exp resp len */
    /* helper variables for apdu parsing and response chaining */
    bool has_le;
    bool extended;
} nfc_apdu_t;

/* APDU parsing status */
typedef enum
{
    APDU_PARSE_OK = 0,
    APDU_ERR_NULL,
    APDU_ERR_TOO_SHORT,
    APDU_ERR_MALFORMED,
    APDU_ERR_UNSUPPORTED_CASE,
    APDU_ERR_OTHER
} apdu_parse_status_t;

/*
 * 5. (Evidence of User Interaction - NFC)
 * For authenticators without a method to collect  a user gesture
 * inside the authenticator boundary other than through a power on gesture,
 * the act of a user placing an NFC authenticator into the NFC reader’s field is considered
 * a user gesture that establishes user presence and provides evidence of user interaction.
 * This powers-up the authenticator, who then starts an NFC powered-up timer,
 * and sets an NFC userPresent flag to true.
 * There is an associated NFC user presence maximum time limit of two minutes (120 seconds)".
 * This is handled by the nfc_user_presence_timer handlers.
 */
typedef struct
{
    bool nfc_user_present;
    bool nfc_ctap_in_use;
    uint32_t threshold;
    uint32_t begin_timestamp;
} nfc_user_presence_timer_t;

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
apdu_parse_status_t nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out);
/**
 *
 * @param buf [in/out] output buffer to write the 2-byte status word (SW1, SW2)
 * @param sw [in] status words to put (2 bytes)
 * @return length of the output (always 2 in this case for 2 bytes)
 */
uint16_t nfc_put_sw(uint8_t *buf, uint16_t sw );
/**
 *
 * @param data [in] data to write to the response buffer
 * @param data_len [in] length of the data to write
 * @param sw [in] status word (SW1, SW2) to append to the response
 * @param out [out] output buffer
 * @param out_size [in] size of the output buffer
 * @return length of the output (in bytes)
 */
size_t nfc_build_response(const uint8_t *data, size_t data_len, uint16_t sw, uint8_t *out, size_t out_size);
/**
 *
 * @param ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param rx_data raw data received from the NFC
 * @param rx_data_len length of the received data
 * @param tx_buf response buffer
 * @param tx_buf_len length of the response buffer
 * @return length of the response (in bytes)
 */
uint16_t nfc_parse_and_respond(t4t_context_t *ctx, uint8_t *rx_data, uint16_t rx_data_len, uint8_t *tx_buf, uint16_t tx_buf_len );

void ctap_nfc_start_user_presence_timer(nfc_user_presence_timer_t* t);
void ctap_nfc_stop_user_presence_timer(nfc_user_presence_timer_t* t);
bool ctap_nfc_is_user_presence_timer_expired(nfc_user_presence_timer_t* t);
#endif //LIONKEY_CTAP_NFC_H