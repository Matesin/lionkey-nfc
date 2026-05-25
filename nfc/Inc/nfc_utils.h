//
// Created by Maty Martan on 24.05.2026.
//
#include "nfc.h"

#ifndef LIONKEY_NFC_UTILS_H
#define LIONKEY_NFC_UTILS_H
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
 * @param ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 */
void reset_chain(t4t_context_t *ctx);

#endif //LIONKEY_UTILS_H