//
// Created by Maty Martan on 24.05.2026.
//

#ifndef LIONKEY_NDEF_H
#define LIONKEY_NDEF_H
#include <stdint.h>
#include "nfc.h"
/**
 *
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] rsp response buffer
 * @return total response length (in this case status word only, meaning two bytes)
 */
uint16_t ndef_handle_select(t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] rsp response buffer
 * @param [out] rsp_len length of the response
 * @return total response length
 */
uint16_t ndef_handle_read(const t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *rsp, uint16_t rsp_len);

/**
 *
 * @param [in/out] ctx context of the T4T application, including selected app, files, and chain buffer for responses larger than Le
 * @param [in] apdu parsed received APDU
 * @param [out] tx_buf TX buffer
 * @return total response length (in this case status word only, meaning two bytes)
 */
uint16_t ndef_handle_update(const t4t_context_t *ctx, const nfc_apdu_t *apdu, uint8_t *tx_buf);


#endif //LIONKEY_NDEF_H