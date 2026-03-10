//
// Created by Maty Martan on 25.02.2026.
//

#include "nfc_test.h"

#include <string.h>

#include "demo_ce.h"
#include "utils.h"
#include "main.h"
#include "ndef_errno.h"
#include "rfal_nfc.h"
#include "spi.h"
#include "rfal_platform.h"
#include "rfal_utils.h"

static rfalNfcDiscoverParam discParam;
static bool                 multiSel;
static void demoNotif( rfalNfcState st );
static bool demoInit(void);
static void demoCE( void );
ReturnCode  demoTransceiveBlocking( uint8_t *txBuf, uint16_t txBufSize, uint8_t **rxData, uint16_t **rcvLen, uint32_t fwt );
static void demoTask(void);

uint8_t state;

static uint8_t ceNFCF_nfcid2[]     = {0x02, 0xFE, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
#if RFAL_FEATURE_NFC_DEP
static uint8_t ceNFCF_SC[]         = {0x12, 0xFC};
static uint8_t ceNFCF_SENSF_RES[]  = {0x01,                                                       /* SENSF_RES                                */
                                      0x02, 0xFE, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,             /* NFCID2                                   */
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x00,             /* PAD0, PAD01, MRTIcheck, MRTIupdate, PAD2 */
                                      0x00, 0x00 };
#endif

static uint8_t NFCID3[] = {0x01, 0xFE, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
static uint8_t GB[] = {0x46, 0x66, 0x6d, 0x01, 0x01, 0x11, 0x02, 0x02, 0x07, 0x80, 0x03, 0x02, 0x00, 0x03, 0x04, 0x01, 0x32, 0x07, 0x01, 0x03};

/* NFC-A CE config */
/* 4-byte UIDs with first byte 0x08 would need random number for the subsequent 3 bytes.
 * 4-byte UIDs with first byte 0x*F are Fixed number, not unique, use for this demo
 * 7-byte UIDs need a manufacturer ID and need to assure uniqueness of the rest.*/
static uint8_t ceNFCA_NFCID[]     = {0x5F, 'S', 'T', 'M'};    /* =_STM, 5F 53 54 4D NFCID1 / UID (4 bytes) */
static uint8_t ceNFCA_SENS_RES[]  = {0x02, 0x00};             /* SENS_RES / ATQA for 4-byte UID            */
static uint8_t ceNFCA_SEL_RES     = 0x20;                     /* SEL_RES / SAK */

static void demoNotif( rfalNfcState st )
{
    uint8_t       devCnt;
    rfalNfcDevice *dev;

    if( st == RFAL_NFC_STATE_WAKEUP_MODE )
    {
        platformLog("Wake Up mode started \r\n");
    }
    else if( st == RFAL_NFC_STATE_POLL_TECHDETECT )
    {
        if( discParam.wakeupEnabled )
        {
            platformLog("Wake Up mode terminated. Polling for devices \r\n");
        }
    }
    else if( st == RFAL_NFC_STATE_POLL_SELECT )
    {
        /* Check if in case of multiple devices, selection is already attempted */
        if( (!multiSel) )
        {
            multiSel = true;
            /* Multiple devices were found, activate first of them */
            rfalNfcGetDevicesFound( &dev, &devCnt );
            rfalNfcSelect( 0 );

            platformLog("Multiple Tags detected: %d \r\n", devCnt);
        }
        else
        {
            rfalNfcDeactivate( RFAL_NFC_DEACTIVATE_DISCOVERY );
        }
    }
    else if( st == RFAL_NFC_STATE_START_DISCOVERY )
    {
        /* Clear mutiple device selection flag */
        multiSel = false;
    }
}

void run_nfc_tests(void) {
    debug_log("running NFC tests..." nl);
    // spi_ping_nfc_test();
    // spi_loopback_test();
    // nfc_init_test();
    nfc_test_worker();
    debug_log("NFC tests completed" nl);
}

void spi_loopback_test(void) {
    debug_log("starting SPI loopback test..." nl);


    uint8_t ret = 0;
    for (int i = 0; i < 100; i++) {
        ret = spi_read_reg(ST25R_READ_REG(i));
        debug_log("read register: 0x40%.2X, returned %2X" nl, i, ret);
        HAL_Delay(100);
    }


}

void spi_ping_nfc_test(void) {
    uint8_t id = st25_read_reg(0x3F); // IC identity register address per errata :contentReference[oaicite:8]{index=8}
    debug_log("ST25R3916 IC ID: 0x%02X" nl, id);
}

void nfc_test_worker(void)
{
    debug_log("Initialising NFC" nl);
    demoInit();
    debug_log("NFC initialised" nl);
    while (1)
    {
        demoTask();
    }
}

static void demoTask(void)
{
    static rfalNfcDevice *nfcDevice;

    rfalNfcWorker();                                    /* Run RFAL worker periodically */

    switch( state )
    {
        case DEMO_ST_START_DISCOVERY:
          multiSel = false;
          state    = DEMO_ST_DISCOVERY;
          break;

        case DEMO_ST_DISCOVERY:

            if( rfalNfcIsDevActivated( rfalNfcGetState() ) )
            {
                debug_log("activated"nl);
                rfalNfcGetActiveDevice( &nfcDevice );
                demoCE( );
                rfalNfcDeactivate( RFAL_NFC_DEACTIVATE_DISCOVERY );
                state = DEMO_ST_START_DISCOVERY;
            }
            break;

        /*******************************************************************************/
        case DEMO_ST_NOTINIT:
        default:
            break;
    }
}

static bool demoInit(void)
{
    ReturnCode err = rfalNfcInitialize();
    if( err == RFAL_ERR_NONE )
    {
        rfalNfcDefaultDiscParams( &discParam );
        
        discParam.devLimit      = 1U;
        
        memcpy( &discParam.nfcid3, NFCID3, sizeof(NFCID3) );
        memcpy( &discParam.GB, GB, sizeof(GB) );
        discParam.GBLen         = sizeof(GB);
        discParam.p2pNfcaPrio   = true;

        discParam.notifyCb             = demoNotif;
        discParam.totalDuration        = 1000U;
        discParam.techs2Find           = RFAL_NFC_TECH_NONE;          /* For the demo, enable the NFC Technologies based on RFAL Feature switches */


#if RFAL_FEATURE_NFCA
        discParam.techs2Find          |= RFAL_NFC_POLL_TECH_A;
#endif /* RFAL_FEATURE_NFCA */

#if DEMO_CARD_EMULATION_ONLY
        discParam.totalDuration        = 60000U;              /* 60 seconds */
        discParam.techs2Find           = RFAL_NFC_TECH_NONE;  /* Overwrite any previous poller modes */
#endif /* DEMO_CARD_EMULATION_ONLY */

#if RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE
        demoCeInit( ceNFCF_nfcid2 );
    
#if RFAL_SUPPORT_MODE_LISTEN_NFCA && RFAL_FEATURE_NFCA
        /* Set configuration for NFC-A CE */
        memcpy( discParam.lmConfigPA.SENS_RES, ceNFCA_SENS_RES, RFAL_LM_SENS_RES_LEN );     /* Set SENS_RES / ATQA */
        memcpy( discParam.lmConfigPA.nfcid, ceNFCA_NFCID, RFAL_LM_NFCID_LEN_04 );           /* Set NFCID / UID */
        discParam.lmConfigPA.nfcidLen = RFAL_LM_NFCID_LEN_04;                                  /* Set NFCID length to 7 bytes */
        discParam.lmConfigPA.SEL_RES  = ceNFCA_SEL_RES;                                        /* Set SEL_RES / SAK */

        discParam.techs2Find |= RFAL_NFC_LISTEN_TECH_A;
#endif /* RFAL_SUPPORT_MODE_LISTEN_NFCA */

#if RFAL_SUPPORT_MODE_LISTEN_NFCF && RFAL_FEATURE_NFCF
        /* Set configuration for NFC-F CE */
        memcpy( discParam.lmConfigPF.SC, ceNFCF_SC, RFAL_LM_SENSF_SC_LEN );                 /* Set System Code */
        memcpy( &ceNFCF_SENSF_RES[RFAL_NFCF_CMD_LEN], ceNFCF_nfcid2, RFAL_NFCID2_LEN );     /* Load NFCID2 on SENSF_RES */
        memcpy( discParam.lmConfigPF.SENSF_RES, ceNFCF_SENSF_RES, RFAL_LM_SENSF_RES_LEN );  /* Set SENSF_RES / Poll Response */

        discParam.techs2Find |= RFAL_NFC_LISTEN_TECH_F;
#endif /* RFAL_SUPPORT_MODE_LISTEN_NFCF */
#endif /* RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE */

        err = rfalNfcDiscover( &discParam );
        if( err != RFAL_ERR_NONE )
        {
            return false;
        }
        
        state = DEMO_ST_START_DISCOVERY;
        return true;
    }
    return false;
}

static void demoCE( void )
{
#if RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE

    ReturnCode err = RFAL_ERR_INTERNAL;
    uint8_t *rxData;
    uint16_t *rcvLen;
    uint8_t  txBuf[150];
    uint16_t txLen;

    do
    {
        rfalNfcWorker();

        switch( rfalNfcGetState() )
        {
        case RFAL_NFC_STATE_ACTIVATED:
            err = demoTransceiveBlocking( NULL, 0, &rxData, &rcvLen, 0);
            break;

        case RFAL_NFC_STATE_DATAEXCHANGE:
        case RFAL_NFC_STATE_DATAEXCHANGE_DONE:

            txLen = demoCeT4T( rxData, *rcvLen, txBuf, sizeof(txBuf) );
            err   = demoTransceiveBlocking( txBuf, txLen, &rxData, &rcvLen, RFAL_FWT_NONE );
            break;

        case RFAL_NFC_STATE_START_DISCOVERY:
            return;

        case RFAL_NFC_STATE_LISTEN_SLEEP:
        default:
            break;
        }
    }
    while( (err == RFAL_ERR_NONE) || (err == RFAL_ERR_SLEEP_REQ) );

#else
    NO_WARNING(nfcDev);
#endif /* RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE */
}

/*!
 *****************************************************************************
 * \brief Demo Blocking Transceive
 *
 * Helper function to send data in a blocking manner via the rfalNfc module
 *
 * \warning A protocol transceive handles long timeouts (several seconds),
 * transmission errors and retransmissions which may lead to a long period of
 * time where the MCU/CPU is blocked in this method.
 * This is a demo implementation, for a non-blocking usage example please
 * refer to the Examples available with RFAL
 *
 * \param[in]  txBuf      : data to be transmitted
 * \param[in]  txBufSize  : size of the data to be transmited
 * \param[out] rxData     : location where the received data has been placed
 * \param[out] rcvLen     : number of data bytes received
 * \param[in]  fwt        : FWT to be used (only for RF frame interface,
 *                                          otherwise use RFAL_FWT_NONE)
 *
 *
 *  \return RFAL_ERR_PARAM   : Invalid parameters
 *  \return RFAL_ERR_TIMEOUT : Timeout error
 *  \return RFAL_ERR_FRAMING : Framing error detected
 *  \return RFAL_ERR_PROTO   : Protocol error detected
 *  \return RFAL_ERR_NONE    : No error, activation successful
 *
 *****************************************************************************
 */
ReturnCode demoTransceiveBlocking( uint8_t *txBuf, uint16_t txBufSize, uint8_t **rxData, uint16_t **rcvLen, uint32_t fwt )
{
    ReturnCode err = rfalNfcDataExchangeStart(txBuf, txBufSize, rxData, rcvLen, fwt);
    if( err == RFAL_ERR_NONE )
    {
        do{
            rfalNfcWorker();
            err = rfalNfcDataExchangeGetStatus();
        }
        while( err == RFAL_ERR_BUSY );
    }
    return err;
}
