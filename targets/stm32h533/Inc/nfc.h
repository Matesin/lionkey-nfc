//
// Created by Maty Martan on 11.01.2026.
//

#ifndef LIONKEY_NFC_H
#define LIONKEY_NFC_H
#include "rfal_nfc.h"

/* NFC-A CE config */
/* 4-byte UIDs with first byte 0x08 would need random number for the subsequent 3 bytes.
 * 4-byte UIDs with first byte 0x*F are Fixed number, not unique
 * 7-byte UIDs need a manufacturer ID and need to assure uniqueness of the rest.*/
static uint8_t NFCID[]     = {0x5F, 'L', 'N', 'K'};    /* =_LNK, 5F 53 54 4D NFCID1 / UID (4 bytes) */
static uint8_t SENS_RES[]  = {0x44, 0x00};             /* SENS_RES / ATQA for 4-byte UID            */
static uint8_t SEL_RES     = 0x20U;                     /* SEL_RES / SAK */

void nfc_init(void);
void nfc_notify(rfalNfcState st);
#endif //LIONKEY_NFC_H