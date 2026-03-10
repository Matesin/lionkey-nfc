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

extern SPI_HandleTypeDef hspi2;
static rfalNfcDiscoverParam discParam;

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

        if( err != RFAL_ERR_NONE )
        {
            return false;
        }
        return true;
    }
    return false;
}

void nfc_notify(rfalNfcState st)
{
    // debug_log("NFC state changed: %d" nl, st);
}


