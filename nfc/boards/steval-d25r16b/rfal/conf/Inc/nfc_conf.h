//
// Created by Maty Martan on 27.02.2026.
//
#pragma once

#ifndef LIONKEY_NFC_CONF_H
#define LIONKEY_NFC_CONF_H

#define RFAL_FEATURE_NFC_RF_BUF_LEN             258U       /*!< RF buffer length used by RFAL NFC layer                           */

extern SPI_HandleTypeDef hspi2;

#define RFAL_FEATURE_NFCA   true
#define RFAL_FEATURE_NFCE   false
#define RFAL_FEATURE_NFCB   false
#define RFAL_FEATURE_NFCF   false
#define RFAL_FEATURE_NFCV   false
#define RFAL_FEATURE_ST25TB false
#define RFAL_FEATURE_NFC_DEP false

#define RFAL_FEATURE_T1T false
#define RFAL_FEATURE_T2T false
#define RFAL_FEATURE_T4T true

#ifdef __cplusplus
extern "C" {
#endif

#endif //LIONKEY_NFC_CONF_H