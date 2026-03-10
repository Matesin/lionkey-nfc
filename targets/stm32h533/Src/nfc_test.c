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

static void demoCE( void );
ReturnCode  demoTransceiveBlocking( uint8_t *txBuf, uint16_t txBufSize, uint8_t **rxData, uint16_t **rcvLen, uint32_t fwt );

uint8_t state;


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
    state = DEMO_ST_DISCOVERY;
    while (1)
    {
        demoTask();
    }
}

void demoTask(void)
{
    static rfalNfcDevice *nfcDevice;

    rfalNfcWorker();                                    /* Run RFAL worker periodically */

    switch( state )
    {
        case DEMO_ST_START_DISCOVERY:
          state    = DEMO_ST_DISCOVERY;
          break;

        case DEMO_ST_DISCOVERY:

            if( rfalNfcIsDevActivated( rfalNfcGetState() ) )
            {
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
            err = demoTransceiveBlocking( NULL, 0, &rxData, &rcvLen, RFAL_FWT_NONE);
            break;

        case RFAL_NFC_STATE_DATAEXCHANGE:
        case RFAL_NFC_STATE_DATAEXCHANGE_DONE:

            txLen = demoCeT4T( rxData, *rcvLen, txBuf, sizeof(txBuf) );
            err   = demoTransceiveBlocking( txBuf, txLen, &rxData, &rcvLen, RFAL_FWT_NONE );
            break;

        case RFAL_NFC_STATE_START_DISCOVERY:
            return;

        case RFAL_NFC_STATE_LISTEN_SLEEP:
            err = demoTransceiveBlocking( NULL, 0, &rxData, &rcvLen, RFAL_FWT_NONE );
            break;
        default:
            break;
        }
        // errorToString(err);
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
