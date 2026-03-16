//
// Created by Maty Martan on 25.02.2026.
//

#include "nfc.h"
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

ReturnCode  demoTransceiveBlocking( uint8_t *txBuf, uint16_t txBufSize, uint8_t **rxData, uint16_t **rcvLen, uint32_t fwt );
static bool demoCE_task(void);

uint8_t state = DEMO_ST_DISCOVERY;

rfalNfcDevice *nfcDevice;
uint8_t *nfcId = 0;
bool exchange_pending = false;

#ifndef NFC_DEMO_BLOCKING
static ce_state_t ce_state = CE_STATE_IDLE;

static uint8_t  *ceRxData = NULL;
static uint16_t *ceRcvLen = NULL;
static uint8_t   ceTxBuf[150];
static uint16_t  ceTxLen = 0;

static void demoCE_reset(void)
{
    ce_state = CE_STATE_IDLE;
    ceRxData   = NULL;
    ceRcvLen   = NULL;
    ceTxLen    = 0;
}

static bool demoCE_startRx(void)
{
    ReturnCode err;

    err = rfalNfcDataExchangeStart(NULL, 0, &ceRxData, &ceRcvLen, RFAL_FWT_NONE);
    if (err == RFAL_ERR_NONE)
    {
        debug_log("CE: start RX successful" nl);
        ce_state = CE_STATE_WAIT_RX;
        return true;
    }

    debug_log("CE start RX failed: %d" nl, err);
    ce_state = CE_STATE_ERROR_RECOVERY;
    return false;
}

static bool demoCE_startTx(uint8_t *txData, uint16_t txLen)
{
    ReturnCode err;

    err = rfalNfcDataExchangeStart(txData, txLen, &ceRxData, &ceRcvLen, RFAL_FWT_NONE);
    if (err == RFAL_ERR_NONE)
    {
        ce_state = CE_STATE_WAIT_TX;
        return true;
    }

    debug_log("CE start TX failed: %d" nl, err);
    ce_state = CE_STATE_ERROR_RECOVERY;
    return false;
}
#endif


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
    demoCeInit(NULL);
    while (1)
    {
        demoTask();
    }
}

void demoTask(void)
{
    rfalNfcWorker();

#ifdef NFC_DEMO_BLOCKING

    switch (state)
    {
    case DEMO_ST_START_DISCOVERY:
        state = DEMO_ST_DISCOVERY;
        break;

    case DEMO_ST_DISCOVERY:
        if (rfalNfcIsDevActivated(rfalNfcGetState()))
        {
            demoCE_task();
            rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            state = DEMO_ST_START_DISCOVERY;
        }
        break;

    case DEMO_ST_NOTINIT:
    default:
        break;
    }

#else

    switch (state)
    {
    case DEMO_ST_START_DISCOVERY:
        demoCE_reset();
        state = DEMO_ST_DISCOVERY;
        break;

    case DEMO_ST_DISCOVERY:
        if (rfalNfcIsDevActivated(rfalNfcGetState()))
        {
            debug_log("NFC activated" nl);
            demoCE_reset();
            state = DEMO_ST_CE_ACTIVE;
        }
        break;

    case DEMO_ST_CE_ACTIVE:
        if (demoCE_task())
        {
            debug_log("session ended" nl);
            state = DEMO_ST_START_DISCOVERY;
        }
        break;

    case DEMO_ST_NOTINIT:
    default:
        break;
    }

#endif
}

static bool demoCE_task(void)
{

#if RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE
#ifdef NFC_DEMO_BLOCKING
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
            return false;

        case RFAL_NFC_STATE_LISTEN_SLEEP:
            err = demoTransceiveBlocking(NULL, 0, &rxData, &rcvLen, RFAL_FWT_NONE);
            break;
        default:
            break;
        }
        // errorToString(err);
    }
    while( (err == RFAL_ERR_NONE) || (err == RFAL_ERR_SLEEP_REQ) );
    return true;

#else

    ReturnCode err;
    rfalNfcState rfState = rfalNfcGetState();

    debug_log("rf state: %d" nl, rfState);
    switch (rfState)
    {
        case RFAL_NFC_STATE_START_DISCOVERY:
            demoCE_reset();
            return true;

        case RFAL_NFC_STATE_ACTIVATED:
            if (ce_state == CE_STATE_IDLE)
            {
                debug_log("CE: start waiting for command" nl);
                demoCE_startRx();
            }
            else if (ce_state == CE_STATE_ERROR_RECOVERY)
            {
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            }
            break;

        case RFAL_NFC_STATE_DATAEXCHANGE:
        case RFAL_NFC_STATE_DATAEXCHANGE_DONE:
            break;
        case RFAL_NFC_STATE_LISTEN_SLEEP:
        default:
            return false;
    }
    debug_log("ce state: %d" nl, ce_state);
    switch (ce_state)
    {
        case CE_STATE_IDLE:
            debug_log("CE: idle -> start RX" nl);
            demoCE_startRx();
            return false;

        case CE_STATE_WAIT_RX:
            err = rfalNfcDataExchangeGetStatus();

            if (err == RFAL_ERR_BUSY)
            {
                return false;
            }

            if (err != RFAL_ERR_NONE)
            {
                debug_log("CE RX failed: %d" nl, err);
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            if ((ceRxData == NULL) || (ceRcvLen == NULL))
            {
                debug_log("CE RX pointers invalid" nl);
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            ce_state = CE_STATE_PROCESS_RX;
            return false;

        case CE_STATE_PROCESS_RX:
            ceTxLen = demoCeT4T(ceRxData, *ceRcvLen, ceTxBuf, sizeof(ceTxBuf));
            debug_log("CE: command processed, response len = %u" nl, ceTxLen);

            if (!demoCE_startTx(ceTxBuf, ceTxLen))
            {
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            }
            return false;

        case CE_STATE_WAIT_TX:
            err = rfalNfcDataExchangeGetStatus();

            if (err == RFAL_ERR_BUSY)
            {
                return false;
            }

            if (err != RFAL_ERR_NONE)
            {
                debug_log("CE TX failed: %d" nl, err);
                ce_state = CE_STATE_ERROR_RECOVERY;
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
                return false;
            }

            debug_log("CE: response sent, wait next command" nl);
            if (!demoCE_startRx())
            {
                rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            }
            return false;

        case CE_STATE_ERROR_RECOVERY:
            rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
            return false;

        default:
            demoCE_reset();
            return false;
    }
#endif
#else
    return true;
#endif
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
            debug_log("busy"nl);
        }
        while( err == RFAL_ERR_BUSY );
    }
    return err;
}
