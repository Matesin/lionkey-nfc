//
// Created by Maty Martan on 11.03.2026.
//
#include <string.h>
#include "terminal.h"
#include "ctap_nfc.h"
#include "ctap.h"

#include "utils.h"

static const uint8_t FIDO_AID[] = NFC_FIDO_AID;
static bool app_selected = false;
extern ctap_state_t app_ctap;

static uint8_t nfc_ctap_response_buffer[CTAPHID_MAX_PAYLOAD_LENGTH];

static ctap_response_t nfc_ctap_response = {
    .data_max_size = sizeof(nfc_ctap_response_buffer) - 1,
    .data = &nfc_ctap_response_buffer[1]
};

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
bool nfc_parse_apdu(const uint8_t *raw, size_t raw_len, nfc_apdu_t *out) {
    if (raw_len < 4) return false;

    out->cla  = raw[0];
    out->ins  = raw[1];
    out->p1   = raw[2];
    out->p2   = raw[3];
    out->data = NULL;
    out->lc   = 0;
    out->le   = 0;

    if (raw_len == 4) return true;

    /* extended length APDU: Lc = 0x00 + 2 bytes */
    if (raw[4] == 0x00 && raw_len >= 7) {
        out->lc = (raw[5] << 8) | raw[6];
        if (raw_len < (size_t)(7 + out->lc)) return false;
        out->data = (uint8_t *)&raw[7];
        if (raw_len > (size_t)(7 + out->lc + 2)) return false;
    } else {
        /* short APDU */
        out->lc = raw[4];
        if (raw_len < (size_t)(5 + out->lc)) return false;
        out->data = (uint8_t *)&raw[5];
    }

    return true;
}

/**
 * @brief Receives a parsed APDU, processes it according to the INS and CLA,
 * and prepares the response data and status word
 *
 * @param apdu      parsed APDU structure
 * @param resp_buf  reponse buffer
 * @param resp_len  [out] length of the response data (excluding SW1, SW2)
 * @param sw        [out] status word (NFC_SW_OK etc.)
 *
 */
void nfc_process_apdu(
    const nfc_apdu_t *apdu,
    uint8_t *resp_buf,
    size_t resp_buf_size,
    size_t *resp_len,
    uint16_t *sw
) {
    *resp_len = 0;
    *sw = NFC_SW_OK;


    if (apdu->ins == NFC_INS_SELECT && apdu->cla == NFC_CLA_ISO) {
        if (apdu->lc == NFC_FIDO_AID_LEN &&
            memcmp(apdu->data, FIDO_AID, NFC_FIDO_AID_LEN) == 0)
        {
            app_selected = true;
            debug_log("NFC: FIDO AID selected" nl);
            *sw = NFC_SW_OK;
        } else {
            app_selected = false;
            *sw = NFC_SW_NOT_FOUND;
        }
        return;
    }

    /* DESELECT */
    if (apdu->ins == NFC_INS_DESELECT) {
        app_selected = false;
        *sw = NFC_SW_OK;
        return;
    }

    /* CTAP command */
    if (apdu->ins == NFC_INS_CTAP && apdu->cla == NFC_CLA_FIDO) {
        if (!app_selected) {
            *sw = NFC_SW_CONDITIONS;
            return;
        }
        if (apdu->lc == 0 || apdu->data == NULL) {
            *sw = NFC_SW_WRONG_DATA;
            return;
        }

        /* apdu->data[0]    = CTAP command byte (0x01 makeCredential atd.) */
        /* apdu->data[1..]  = CBOR payload                                  */
        uint8_t ctap_cmd    = apdu->data[0];
        const uint8_t *cbor = apdu->data + 1;
        size_t cbor_len     = apdu->lc - 1;

        debug_log("NFC: CTAP cmd=0x%02X cbor_len=%u" nl, ctap_cmd, (unsigned)cbor_len);

        nfc_ctap_response.length = 0;
        nfc_ctap_response_buffer[0] = ctap_request(&app_ctap, ctap_cmd, cbor_len, cbor, &nfc_ctap_response);
        *resp_len = 1 + nfc_ctap_response.length;
        if (*resp_len > resp_buf_size) {
            *sw = NFC_SW_WRONG_LENGTH;
            *resp_len = 0;
            debug_log(red("NFC CTAP ERROR: wrong response length %u") nl, *resp_len);
            return;
        }
        memcpy(resp_buf, nfc_ctap_response_buffer, *resp_len);

        *sw = NFC_SW_OK;
        return;
    }

    *sw = NFC_SW_INS_UNKNOWN;
}

/**
 * @brief Builds the final APDU response by concatenating the response data and appending the status word
 *
 * @param [in] data response data bytes (e.g. CBOR-encoded CTAP response)
 * @param [in] data_len length of the response data
 * @param [in] sw status word to append (e.g. NFC_SW_OK)
 * @param [in/out] out output buffer to write the final APDU response (data + SW1, SW2)
 * @param [in/out] out_size size of the output buffer
 *
 * @return total length of the APDU response written to out, or 0 if out_size is insufficient
 */
size_t nfc_build_response(
    const uint8_t *data, size_t data_len,
    uint16_t sw,
    uint8_t *out, size_t out_size
) {
    if (out_size < data_len + 2) return 0;
    if (data_len > 0) memcpy(out, data, data_len);
    out[data_len]     = (sw >> 8) & 0xFF;
    out[data_len + 1] =  sw       & 0xFF;
    return data_len + 2;
}
