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

/* Maximal time of user presence (5.) */
#define CTAP_MAX_USER_PRESENCE_TIME_LIMIT_NFC   (12U * 1000U) // 12s (in ms)
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
    uint32_t threshold;         /*!< NFC User Presence Timer Threshold (in systicks) - defined upon startup */
    uint32_t begin_timestamp;   /*!< NFC User Presence Timer Begin Timestamp (in systicks) - set when the timer is started */
} nfc_user_presence_timer_t;

/* GLOBAL FUNCTION PROTOTYPES */
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

/**
 * @brief handle a received extended CTAP APDU, update T4T context. Pass the request to fido_handle_ctap for processing and building the response.
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @param [out] tx_buf_len length of the TX buffer
 * @return response length
 */
uint16_t fido_handle_ctap_request_extended(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

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
uint16_t fido_handle_ctap_request_short(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

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
uint16_t fido_handle_ctap_chain_out(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @param [out] tx_buf_len length of the TX buffer
 *
 * @return total response length (payload + SW) if success
 */
uint16_t fido_handle_ctap_chain_in(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf, uint16_t tx_buf_len);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @return total response length (SW only -> 2B at all times)
 */
uint16_t fido_handle_ctap_deselect(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf);

#endif //LIONKEY_CTAP_NFC_H