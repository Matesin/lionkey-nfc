/**
 * @file ctap_nfc.h
 *
 * @author Maty Martan
 *
 * @brief Provides methods and structures for NFC application layer for the CTAP protocol
 *
 * @note This file is compliant with FIDO specifications and is implemented
 * based on the following document:
 * https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#nfc
 *
 * Whenever there is an index associated with either a define, type or a prototype, it points to that respective
 * part of the FIDO specification mentioned above. Otherwise, a link to a different specification is provided.
 */

#ifndef LIONKEY_CTAP_NFC_H
#define LIONKEY_CTAP_NFC_H

/* INCLUDES */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "nfc.h"

/* GLOBAL DEFINES */

/* Instruction byte values */
#define NFC_INS_SELECT                          0xA4U
#define NFC_INS_CTAP                            0x10U
#define NFC_INS_DESELECT                        0x12U
#define NFC_INS_GET_RESPONSE                    0xC0U
#define NFC_INS_CTAP_CONTROL                    0x12U

/* Maximal time of user presence (5.) */
#define CTAP_MAX_USER_PRESENCE_TIME_LIMIT_NFC   (12U * 1000U) // 12s (in ms)

/* Class byte values */
#define NFC_CLA_ISO                             0x00U
#define NFC_CLA_CTAP                            0x80U
#define NFC_CLA_CTAP_CHAIN                      0x90U

#define NFC_PARSE_WRONG_SIZE                    1U

#define NFC_APDU_SHORT_MAX_LEN                  256U
#define NFC_APDU_EXTENDED_MAX_LEN               65536U

extern bool nfc_user_present;

/* GLOBAL TYPES */

/*!
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
    bool nfc_user_present;      /*!< NFC User Present Flag (FIDO defined) */
    bool nfc_ctap_in_use;       /*!< Flag indicating whether the CTAP session is currently active (i.e., whether we are currently processing a CTAP request) */
    uint32_t threshold;         /*!< NFC User Presence Timer Threshold (in systicks) - defined upon startup */
    uint32_t begin_timestamp;   /*!< NFC User Presence Timer Begin Timestamp (in systicks) - set when the timer is started */
} nfc_user_presence_timer_t;

/* GLOBAL FUNCTION PROTOTYPES */

/**
 * @brief NFC Parse APDU
 *
 * Parses raw APDU bytes into the nfc_apdu_t structure
 *
 * @param raw APDU bytes received from the NFC reader
 * @param raw_len length of the raw APDU data
 * @param out [out] parsed APDU structure
 *
 * @return APDU_PARSE_OK if parsing was successful
 * @return APDU_ERR_TOO_SHORT if the length of the received message is less than 4
 * @return APDU_ERR_MALFORMED otherwise
 */
apdu_parse_status_t nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out);

/**
 * @brief NFC Put SW
 *
 * Puts a status word into an output buffer in big-endian format (SW1, SW2)
 *
 * @param buf [in/out] output buffer to write the 2-byte status word (SW1, SW2)
 * @param sw [in] status words to put (2 bytes)
 *
 * @return length of the output (always 2 in this case for 2 bytes)
 */
uint16_t nfc_put_sw(uint8_t *buf, uint16_t sw );

/**
 *
 * @brief NFC Build Response
 *
 * Builds the final response to be sent via NFC
 *
 * @param data [in] data to write to the response buffer
 * @param data_len [in] length of the data to write
 * @param sw [in] status word (SW1, SW2) to append to the response
 * @param out [out] output buffer
 * @param out_size [in] size of the output buffer
 *
 * @return length of the output (in bytes)
 */
size_t nfc_build_response(const uint8_t *data, size_t data_len, uint16_t sw, uint8_t *out, size_t out_size);

/**
 *
 * @brief NFC Parse And Respond
 *
 * Parses the received raw data from the input buffer and responds with the appropriate response based on the APDU command and the current state of the T4T context.
 *
 * @param ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param rx_data raw data received from the NFC
 * @param rx_data_len length of the received data
 * @param tx_buf response buffer
 * @param tx_buf_len length of the response buffer
 *
 * @return length of the response (in bytes)
 */
uint16_t nfc_parse_and_respond(t4t_context_t *ctx, uint8_t *rx_data, uint16_t rx_data_len, uint8_t *tx_buf, uint16_t tx_buf_len );

/**
 *
 * @brief CTAP NFC Start User Presence Timer
 *
 * Starts the NFC user presence timer by setting the nfc_user_present flag to true and the timer's timestamp to the current time
 *
 * @param t pointer to the NFC user presence timer structure
 */
void ctap_nfc_start_user_presence_timer(nfc_user_presence_timer_t* t);

/**
 *
 * @brief CTAP NFC Stop User Presence Timer
 *
 * Stops the NFC user presence timer by setting the nfc_user_present flag to false and nfc_ctap_in_use to false
 *
 * @param t pointer to the NFC user presence timer structure
 */
void ctap_nfc_stop_user_presence_timer(nfc_user_presence_timer_t* t);

/**
 *
 * @brief CTAP NFC Is User presence Timer Expired
 *
 * Checks if the NFC user presence timer has expired by comparing the current time with the begin_timestamp and the timer's threshold
 *
 * @param t pointer to the NFC user presence timer structure
 * @return true if expired
 * @return false if not expired
 */
bool ctap_nfc_is_user_presence_timer_expired(const nfc_user_presence_timer_t* t);
#endif //LIONKEY_CTAP_NFC_H