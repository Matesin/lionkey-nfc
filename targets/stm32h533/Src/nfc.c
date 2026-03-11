//
// Created by Maty Martan on 11.01.2026.
//

#include "nfc.h"
#include "main.h"
#include "nfc_test.h"
#include "rfal_nfc.h"
#include "spi.h"
#include "utils.h"
#include "rfal_nfca.h"

/* NFC-A CE config */
/* 4-byte UIDs with first byte 0x08 would need random number for the subsequent 3 bytes.
 * 4-byte UIDs with first byte 0x*F are Fixed number, not unique
 * 7-byte UIDs need a manufacturer ID and need to assure uniqueness of the rest.*/
static uint8_t NFCID[]     = {0x5F, 'L', 'N', 'K'};    /* =_LNK, 5F  4C  4E  4B NFCID1 / UID (4 bytes) - static value */
static uint8_t SENS_RES[]  = {0x44, 0x00};             /* SENS_RES / ATQA for 4-byte UID            */
static uint8_t SEL_RES     = 0x20U;                     /* SEL_RES / SAK */

static rfalNfcDiscoverParam discParam;

uint8_t select_state;

static bool cmdCompare(uint8_t *cmd, uint8_t *find, uint16_t len);
static inline bool nfc_init_params(void);

void nfc_init(void)
{
    debug_log("initializing NFC..." nl);
    if (!nfc_init_params()) {
        debug_log(red("Failed to initialize NFC") nl);
        return;
    }
    debug_log("NFC initialized" nl);
    debug_log("Running NFC tests..." nl);
    run_nfc_tests();
}

static bool nfc_init_params(void)
{
    ReturnCode err = rfalNfcInitialize();
    if( err == RFAL_ERR_NONE )
    {
        rfalNfcDefaultDiscParams( &discParam );

        discParam.devLimit      = 1U;

        discParam.notifyCb             = nfc_notify;
        discParam.totalDuration        = 1000U;

        /* Set configuration for NFC-A CE */
        memcpy( discParam.lmConfigPA.SENS_RES, SENS_RES, RFAL_LM_SENS_RES_LEN );     /* Set SENS_RES / ATQA */
        memcpy( discParam.lmConfigPA.nfcid, NFCID, RFAL_LM_NFCID_LEN_04 );           /* Set NFCID / UID */
        discParam.lmConfigPA.nfcidLen = RFAL_LM_NFCID_LEN_04;/* Set NFCID length to 4 bytes */
        discParam.lmConfigPA.SEL_RES  = SEL_RES;                                     /* Set SEL_RES / SAK */

        discParam.isoDepFS = RFAL_ISODEP_FSXI_256;                                    /* Set ISO-DEP Frame Size to 256 bytes */
        discParam.nfcDepLR = RFAL_NFCDEP_LR_254;

        discParam.techs2Find = RFAL_NFC_LISTEN_TECH_A;

        err = rfalNfcDiscover( &discParam );

        return (err == RFAL_ERR_NONE);
    }
    return false;
}

void nfc_task(void)
{

}

static bool cmdCompare(uint8_t *cmd, uint8_t *find, uint16_t len)
{
    for(int i = 0; i < 20; i++)
    {
        if(!memcmp(&cmd[i],find, len))
        {
            return true;
        }
    }
    return false;
}
//
// uint16_t nfc_parse_and_respond(uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf, uint16_t txBufLen )
// {
//     {
//         if( (txBuf == NULL) || (txBufLen < 2) )
//         {
//             platformErrorHandle();  /* Must ensure appropriate buffer */
//             return 0;
//         }
//
//
//         if( (rxData != NULL) && (rxDataLen >= 4) )
//         {
//             if(rxData[0] == T4T_CLA_00)
//             {
//                 switch(rxData[1])
//                 {
//                 case T4T_INS_SELECT:
//                     return demoCeT4TSelect(rxData, txBuf);
//
//                 case T4T_INS_READ:
//                     return demoCeT4TRead(rxData, txBuf, txBufLen);
//
//                 case T4T_INS_UPDATE:
//                     return demoCeT4TUpdate(rxData, txBuf);
//
//                 default:
//                     break;
//                 }
//             }
//         }
//
//         /* Function not supported ...  */
//         txBuf[0] = ((char)0x68);
//         txBuf[1] = ((char)0x00);
//         return 2;
//     }
// }
//
// uint16_t nfc_select_response(uint8_t *cmdData, uint8_t *rspData )
// {
//     {
//         bool success = false;
//         /*
//          * Cmd: CLA(1) | INS(1) | P1(1) | P2(1) | Lc(1) | Data(n) | [Le(1)]
//          * Rsp: [FCI(n)] | SW12
//          *
//          * Select App by Name NDEF:       00 A4 04 00 07 D2 76 00 00 85 01 01 00
//          * Select App by Name NDEF 4 ST:  00 A4 04 00 07 A0 00 00 00 03 00 00 00
//          * Select CC FID:                 00 A4 00 0C 02 xx xx
//          * Select NDEF FID:               00 A4 00 0C 02 xx xx
//          */
//
//         uint8_t aid[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
//         uint8_t fidCC[] = {FID_CC >> 8 , FID_CC & 0xFF};
//         uint8_t fidNDEF[] = {FID_NDEF >> 8, FID_NDEF & 0xFF};
//         uint8_t selectFileId[] = {0xA4, 0x00, 0x0C, 0x02, 0x00, 0x01 };
//
//
//         if(cmdCompare( cmdData, aid, sizeof(aid)))
//         { /* Select Appli */
//             select_state = STATE_APP_SELECTED;
//             success = true;
//         }
//         else if((select_state >= STATE_APP_SELECTED) && cmdCompare(cmdData, fidCC, sizeof(fidCC)))
//         { /* Select CC */
//             select_state = STATE_CC_SELECTED;
//             nSelectedIdx = 0;
//             success = true;
//         }
//         else if((select_state >= STATE_APP_SELECTED) && (cmdCompare(cmdData,fidNDEF,sizeof(fidNDEF)) || cmdCompare(cmdData,selectFileId,sizeof(selectFileId))))
//         { /* Select NDEF */
//             select_state = STATE_FID_SELECTED;
//             nSelectedIdx = 1;
//             success = true;
//         }
//         else
//         {
//             select_state = STATE_IDLE;
//         }
//
//         rspData[0] = (success ? (char)0x90 : 0x6A);
//         rspData[1] = (success ? (char)0x00 : 0x82);
//
//         return 2;
//     }
// }

void nfc_notify(rfalNfcState st)
{
    // debug_log("NFC state changed: %d" nl, st);
}


