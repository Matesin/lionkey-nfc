/**
* @file nfc_conf.h
 * @brief RFAL/NFC feature configuration for STEVAL-D25R16B.
 *
 * This header centralizes RFAL (RF Abstraction Layer) and NFC feature
 * configuration used by the project. It selects which NFC technologies
 * are enabled and configures buffer sizes used by the RFAL NFC layer.
 *
 * Usage:
 *  - Include this file in modules that require RFAL/NFC build-time options.
 *  - Modify macros below to enable/disable features or adjust buffer sizes.
 *  - Keep changes consistent with linker/stack constraints on target MCU.
 *
 * Macros:
 *  - RFAL_FEATURE_NFC_RF_BUF_LEN: size (bytes) of the RFAL NFC exchange buffer.
 *  - RFAL_FEATURE_NFCA..RFAL_FEATURE_NFCV: booleans enabling specific NFC technologies.
 *  - RFAL_FEATURE_ST25TB: enable ST25TB (tag-based) support.
 *  - RFAL_FEATURE_NFC_DEP: enable NFC-DEP (peer-to-peer) support.
 *  - RFAL_FEATURE_T1T/T2T/T4T: enable specific tag types (T1T..T4T).
 *
 * Notes:
 *  - Adjust RFAL_FEATURE_NFC_RF_BUF_LEN according to maximum frame/APDU sizes.
 *  - Disable unused features to reduce code size and RAM usage on constrained MCUs.
 */


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIONKEY_NFC_CONF_H
#define LIONKEY_NFC_CONF_H

/* RFAL NFC buffer length used by the NFC layer (bytes) */
#define RFAL_FEATURE_NFC_RF_BUF_LEN             258U       /*!< RF buffer length used by RFAL NFC layer */

#define NFC_SPI_TIMEOUT                         100U

/* External peripheral handles (declare defined elsewhere in the project) */
extern SPI_HandleTypeDef hspi2;

/* NFC technology feature switches (set true/false as needed) */
#define RFAL_FEATURE_NFCA   true
#define RFAL_FEATURE_NFCE   false
#define RFAL_FEATURE_NFCB   false
#define RFAL_FEATURE_NFCF   false
#define RFAL_FEATURE_NFCV   false
#define RFAL_FEATURE_ST25TB false
#define RFAL_FEATURE_NFC_DEP false


/* Tag type support */
#define RFAL_FEATURE_T1T false
#define RFAL_FEATURE_T2T false
#define RFAL_FEATURE_T4T true

#endif /* LIONKEY_NFC_CONF_H */

#ifdef __cplusplus
}
#endif
