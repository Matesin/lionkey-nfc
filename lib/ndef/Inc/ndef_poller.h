
/**
  ******************************************************************************
  * @file           : ndef_poller.h
  * @brief          : Provides NDEF methods and definitions to access NFC Forum Tags
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */


#ifndef NDEF_POLLER_H
#define NDEF_POLLER_H

/*
 ******************************************************************************
 * INCLUDES
 ******************************************************************************
 */
#include "ndef_message.h"
#include "rfal_isoDep.h"
#include "rfal_t4t.h"
#include "rfal_utils.h"


/*
 ******************************************************************************
 * GLOBAL DEFINES
 ******************************************************************************
 */

#define NDEF_CC_BUF_LEN             17U                                                /*!< CC buffer len. Max length = 17 in case of T4T v3             */
#define NDEF_NFCV_SUPPORTED_CMD_LEN  4U                                                /*!< Ext sys info supported commands list length                  */
#define NDEF_NFCV_UID_LEN            8U                                                /*!< NFC-V UID length                                             */

#define NDEF_SHORT_VFIELD_MAX_LEN  254U                                                /*!< Max V-field length for 1-byte Lengh encoding                 */
#define NDEF_TERMINATOR_TLV_LEN      1U                                                /*!< Terminator TLV size                                          */
#define NDEF_TERMINATOR_TLV_T     0xFEU                                                /*!< Terminator TLV T=FEh                                         */

#define NDEF_T2T_READ_RESP_SIZE     16U                                                /*!< Size of the READ response i.e. four blocks                   */
#define NDEF_T2T_MAX_RSVD_AREAS      3U                                                /*!< Number of reserved areas including 1 Dyn Lock area           */

#define NDEF_T3T_BLOCK_SIZE         16U                                                /*!< size for a block in t3t                                      */
#define NDEF_T3T_MAX_NB_BLOCKS       4U                                                /*!< size for a block in t3t                                      */
#define NDEF_T3T_BLOCK_NUM_MAX_SIZE  3U                                                /*!< Maximun size for a block number                              */
#define NDEF_T3T_MAX_RX_SIZE      ((NDEF_T3T_BLOCK_SIZE*NDEF_T3T_MAX_NB_BLOCKS) + 13U) /*!< size for a CHECK Response 13 bytes (LEN+07h+NFCID2+Status+Nos) + (block size x Max Nob)                                                */
#define NDEF_T3T_MAX_TX_SIZE      (((NDEF_T3T_BLOCK_SIZE + NDEF_T3T_BLOCK_NUM_MAX_SIZE) * NDEF_T3T_MAX_NB_BLOCKS) + 14U) \
                                                                                       /*!< size for an UPDATE command, 11 bytes (LEN+08h+NFCID2+Nos) + 2 bytes for 1 SC + 1 byte for NoB + (block size + block num Len) x Max NoB */

#define NDEF_T5T_TxRx_BUFF_HEADER_SIZE        1U                                       /*!< Request Flags/Responses Flags size                           */
#define NDEF_T5T_TxRx_BUFF_FOOTER_SIZE        2U                                       /*!< CRC size                                                     */

#define NDEF_T5T_TxRx_BUFF_SIZE               \
          (32U +  NDEF_T5T_TxRx_BUFF_HEADER_SIZE + NDEF_T5T_TxRx_BUFF_FOOTER_SIZE)     /*!< T5T working buffer size                                      */

/*
 ******************************************************************************
 * GLOBAL MACROS
 ******************************************************************************
 */

#define ndefBytes2Uint16(hiB, loB)          ((uint16_t)((((uint32_t)(hiB)) << 8U) | ((uint32_t)(loB))))                                                  /*!< convert 2 bytes to a u16 */

#define ndefMajorVersion(V)                 ((uint8_t)((V) >>  4U))    /*!< Get major version */
#define ndefMinorVersion(V)                 ((uint8_t)((V) & 0xFU))    /*!< Get minor version */


/*
 ******************************************************************************
 * GLOBAL TYPES
 ******************************************************************************
 */


/*! NDEF device type */
typedef enum {
    NDEF_DEV_NONE          = 0x00U,                            /*!< Device type undef                                  */
    NDEF_DEV_T1T           = 0x01U,                            /*!< Device type T1T                                    */
    NDEF_DEV_T2T           = 0x02U,                            /*!< Device type T2T                                    */
    NDEF_DEV_T3T           = 0x03U,                            /*!< Device type T3T                                    */
    NDEF_DEV_T4T           = 0x04U,                            /*!< Device type T4AT/T4BT                              */
    NDEF_DEV_T5T           = 0x05U,                            /*!< Device type T5T                                    */
} ndefDeviceType;

/*! NDEF states  */
typedef enum {
    NDEF_STATE_INVALID     = 0x00U,                            /*!< Invalid state (e.g. no CC)                         */
    NDEF_STATE_INITIALIZED = 0x01U,                            /*!< State Initialized (no NDEF message)                */
    NDEF_STATE_READWRITE   = 0x02U,                            /*!< Valid NDEF found. Read/Write capability            */
    NDEF_STATE_READONLY    = 0x03U,                            /*!< Valid NDEF found. Read only                        */
} ndefState;

/*! NDEF Information */
typedef struct {
    uint8_t                  majorVersion;                     /*!< Major version                                      */
    uint8_t                  minorVersion;                     /*!< Minor version                                      */
    uint32_t                 areaLen;                          /*!< Area Length for NDEF storage                       */
    uint32_t                 areaAvalableSpaceLen;             /*!< Remaining Space in case a propTLV is present       */
    uint32_t                 messageLen;                       /*!< NDEF message Length                                */
    ndefState                state;                            /*!< Tag state e.g. NDEF_STATE_INITIALIZED              */
} ndefInfo;

/*! T4T Capability Container  */
typedef struct {
    uint16_t                 ccLen;                            /*!< CCFILE Length                                      */
    uint8_t                  vNo;                              /*!< Mapping version                                    */
    uint16_t                 mLe;                              /*!< Max data size that can be read using a ReadBinary  */
    uint16_t                 mLc;                              /*!< Max data size that can be sent using a single cmd  */
    uint8_t                  fileId[2];                        /*!< NDEF File Identifier                               */
    uint32_t                 fileSize;                         /*!< NDEF File Size                                     */
    uint8_t                  readAccess;                       /*!< NDEF File READ access condition                    */
    uint8_t                  writeAccess;                      /*!< NDEF File WRITE access condition                   */
} ndefCapabilityContainerT4T;

/*! Generic Capability Container  */
typedef union {
    ndefCapabilityContainerT4T   t4t;                          /*!< T4T Capability Container                           */
} ndefCapabilityContainer;

/*! NDEF T4T sub context structure */
typedef struct {
    uint8_t                      curMLe;                       /*!< Current MLe. Default Fh until CC file is read      */
    uint8_t                      curMLc;                       /*!< Current MLc. Default Dh until CC file is read      */
    bool                         mv1Flag;                      /*!< Mapping version 1 flag                             */
    rfalIsoDepApduBufFormat      cApduBuf;                     /*!< Command-APDU buffer                                */
    rfalIsoDepApduBufFormat      rApduBuf;                     /*!< Response-APDU buffer                               */
    rfalT4tRApduParam            respAPDU;                     /*!< Response-APDU params                               */
    rfalIsoDepBufFormat          tmpBuf;                       /*!< I-Block temporary buffer                           */
    uint16_t                     rApduBodyLen;                 /*!< Response Body Length                               */
    uint32_t                     FWT;                          /*!< Frame Waiting Time (1/fc)                          */
    uint32_t                     dFWT;                         /*!< Delta Frame Waiting Time (1/fc)                    */
    uint16_t                     FSx;                          /*!< Frame Size Device/Card (FSD or FSC)                */
    uint8_t                      DID;                          /*!< Device ID                                          */
} ndefT4TContext;

/*! NDEF context structure */
typedef struct {
    ndefDeviceType               type;                         /*!< NDEF Device type                                   */
    ndefDevice                   device;                       /*!< NDEF Device                                        */
    ndefState                    state;                        /*!< Tag state e.g. NDEF_STATE_INITIALIZED              */
    ndefCapabilityContainer      cc;                           /*!< Capability Container                               */
    uint32_t                     messageLen;                   /*!< NDEF message length                                */
    uint32_t                     messageOffset;                /*!< NDEF message offset                                */
    uint32_t                     areaLen;                      /*!< Area Length for NDEF storage                       */
    uint8_t                      ccBuf[NDEF_CC_BUF_LEN];       /*!< buffer for CC                                      */
    const struct ndefPollerWrapperStruct*
                                 ndefPollWrapper;              /*!< pointer to array of function for wrapper           */
    union {
        ndefT4TContext t4t;                                    /*!< T4T context                                        */
    } subCtx;                                                  /*!< Sub-context union                                  */
} ndefContext;

/*! Wrapper structure to hold the function pointers on each tag type */
typedef struct ndefPollerWrapperStruct
{
    ReturnCode (* pollerContextInitialization)(ndefContext *ctx, const ndefDevice *dev);                                                  /*!< ContextInitialization function pointer                 */
    ReturnCode (* pollerNdefDetect)(ndefContext *ctx, ndefInfo *info);                                                                    /*!< NdefDetect function pointer                            */
    ReturnCode (* pollerReadBytes)(ndefContext *ctx, uint32_t offset, uint32_t len, uint8_t *buf, uint32_t *rcvdLen);                     /*!< Read function pointer                                  */
    ReturnCode (* pollerReadRawMessage)(ndefContext *ctx, uint8_t *buf, uint32_t bufLen, uint32_t *rcvdLen, bool single);                 /*!< ReadRawMessage function pointer                        */
#if NDEF_FEATURE_FULL_API
    ReturnCode (* pollerWriteBytes)(ndefContext *ctx, uint32_t offset, const uint8_t *buf, uint32_t len, bool pad, bool writeTerminator); /*!< Write function pointer                                 */
    ReturnCode (* pollerWriteRawMessage)(ndefContext *ctx, const uint8_t *buf, uint32_t bufLen);                                          /*!< WriteRawMessage function pointer                       */
    ReturnCode (* pollerTagFormat)(ndefContext *ctx, const ndefCapabilityContainer *cc, uint32_t options);                                /*!< TagFormat function pointer                             */
    ReturnCode (* pollerWriteRawMessageLen)(ndefContext *ctx, uint32_t rawMessageLen, bool writeTerminator);                              /*!< WriteRawMessageLen function pointer                    */
    ReturnCode (* pollerCheckPresence)(ndefContext *ctx);                                                                                 /*!< CheckPresence function pointer                         */
    ReturnCode (* pollerCheckAvailableSpace)(const ndefContext *ctx, uint32_t messageLen);                                                /*!< CheckAvailableSpace function pointer                   */
    ReturnCode (* pollerBeginWriteMessage)(ndefContext *ctx, uint32_t messageLen);                                                        /*!< BeginWriteMessage function pointer                     */
    ReturnCode (* pollerEndWriteMessage)(ndefContext *ctx, uint32_t messageLen, bool writeTerminator);                                    /*!< EndWriteMessage function pointer                       */
    ReturnCode (* pollerSetReadOnly)(ndefContext *ctx);                                                                                   /*!< SetReadOnly function pointer                           */
#endif /* NDEF_FEATURE_FULL_API */
} ndefPollerWrapper;


/*
 ******************************************************************************
 * GLOBAL FUNCTION PROTOTYPES
 ******************************************************************************
 */


/*!
 *****************************************************************************
 * \brief Return the device type
 *
 * This funtion returns the device type from the context
 *
 * \param[in] dev: ndef Device
 *
 * \return the device type
 *****************************************************************************
 */
ndefDeviceType ndefGetDeviceType(const ndefDevice *dev);


/*!
 *****************************************************************************
 * \brief Handle NDEF context activation
 *
 * This method performs the initialization of the NDEF context.
 * It must be called after a successful
 * anti-collision procedure and prior to any NDEF procedures such as NDEF
 * detection procedure.
 *
 * \param[in]   ctx    : ndef Context
 * \param[in]   dev    : ndef Device
 *
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerContextInitialization(ndefContext *ctx, const ndefDevice *dev);


/*!
 *****************************************************************************
 * \brief NDEF Detection procedure
 *
 * This method performs the NDEF Detection procedure
 *
 * \param[in]   ctx    : ndef Context
 * \param[out]  info   : ndef Information (optional parameter, NULL may be used when no NDEF Information is needed)
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : Detection failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerNdefDetect(ndefContext *ctx, ndefInfo *info);


/*!
 *****************************************************************************
 * \brief Read data
 *
 * This method reads arbitrary length data
 *
 * \param[in]   ctx    : ndef Context
 * \param[in]   offset : file offset of where to start reading data
 * \param[in]   len    : requested length
 * \param[out]  buf    : buffer to place the data read from the tag
 * \param[out]  rcvdLen: received length
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : read failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerReadBytes(ndefContext *ctx, uint32_t offset, uint32_t len, uint8_t *buf, uint32_t *rcvdLen);


/*!
 *****************************************************************************
 * \brief  Write data
 *
 * This method writes arbitrary length data from the current selected file
 *
 * \param[in]   ctx    : ndef Context
 * \param[in]   offset : file offset of where to start writing data
 * \param[in]   buf    : data to write
 * \param[in]   len    : buf length
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : read failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerWriteBytes(ndefContext *ctx, uint32_t offset, const uint8_t *buf, uint32_t len);


/*!
 *****************************************************************************
 * \brief Read raw NDEF message
 *
 * This method reads a raw NDEF message.
 * Prior to NDEF Read procedure, a successful ndefPollerNdefDetect()
 * has to be performed.
 *
 *
 * \param[in]   ctx    : ndef Context
 * \param[out]  buf    : buffer to place the NDEF message
 * \param[in]   bufLen : buffer length
 * \param[out]  rcvdLen: received length
 * \param[in]   single : performs the procedure as part of a single NDEF read operation. "true" can be used when migrating from previous version of this API as only SINGLE NDEF READ was supported. "false" can be used to force the reading of the NDEF length (e.g. for TNEP).
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : read failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerReadRawMessage(ndefContext *ctx, uint8_t *buf, uint32_t bufLen, uint32_t *rcvdLen, bool single);


/*!
 *****************************************************************************
 * \brief Write raw NDEF message
 *
 * This method writes a raw NDEF message.
 * Prior to NDEF Write procedure, a successful ndefPollerNdefDetect()
 * has to be performed.
 *
 * \param[in]   ctx    : ndef Context
 * \param[in]   buf    : raw message buffer
 * \param[in]   bufLen : buffer length
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : write failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerWriteRawMessage(ndefContext *ctx, const uint8_t *buf, uint32_t bufLen);


/*!
 *****************************************************************************
 * \brief Format Tag
 *
 * This method formats a tag to make it ready for NDEF storage.
 * cc and options parameters usage is described in each technology method
 * (ndefT[2345]TPollerTagFormat)
 *
 * \param[in]   ctx     : ndef Context
 * \param[in]   cc      : Capability Container
 * \param[in]   options : specific flags
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : write failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerTagFormat(ndefContext *ctx, const ndefCapabilityContainer *cc, uint32_t options);


/*!
 *****************************************************************************
 * \brief Write NDEF message length
 *
 * This method writes the NLEN field
 *
 * \param[in]   ctx          : ndef Context
 * \param[in]   rawMessageLen: len
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : write failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerWriteRawMessageLen(ndefContext *ctx, uint32_t rawMessageLen);


 /*!
 *****************************************************************************
 * \brief Write an NDEF message
 *
 * Write the NDEF message to the tag
 *
 * \param[in] ctx:     NDEF Context
 * \param[in] message: Message to write
 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_REQUEST      : write failed
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerWriteMessage(ndefContext *ctx, const ndefMessage *message);


/*!
 *****************************************************************************
 * \brief Check Presence
 *
 * This method checks whether an NFC tag is still present in the operating field
 *
 * \param[in]   ctx    : ndef Context

 *
 * \return ERR_WRONG_STATE  : Library not initialized or mode not set
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_PROTO        : Protocol error
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerCheckPresence(ndefContext *ctx);


/*!
 *****************************************************************************
 * \brief Check Available Space
 *
 * This method checks whether a NFC tag has enough space to write a message of a given length
 *
 * \param[in]   ctx       : ndef Context
 * \param[in]   messageLen: message length
 *
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_NOMEM        : not enough space
 * \return ERR_NONE         : Enough space for message of messageLen length
 *****************************************************************************
 */
ReturnCode ndefPollerCheckAvailableSpace(const ndefContext *ctx, uint32_t messageLen);


/*!
 *****************************************************************************
 * \brief Begin Write Message
 *
 * This method sets the L-field to 0 (T1T, T2T, T4T, T5T) or set the WriteFlag (T3T) and sets the message offset to the proper value according to messageLen
 *
 * \param[in]   ctx       : ndef Context
 * \param[in]   messageLen: message length
 *
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_NOMEM        : not enough space
 * \return ERR_NONE         : Enough space for message of messageLen length
 *****************************************************************************
 */
ReturnCode ndefPollerBeginWriteMessage(ndefContext *ctx, uint32_t messageLen);


/*!
 *****************************************************************************
 * \brief End Write Message
 *
 * This method updates the L-field value after the message has been written and resets the WriteFlag (for T3T only)
 *
 * \param[in]   ctx       : ndef Context
 * \param[in]   messageLen: message length
 *
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_NOMEM        : not enough space
 * \return ERR_NONE         : Enough space for message of messageLen length
 *****************************************************************************
 */
ReturnCode ndefPollerEndWriteMessage(ndefContext *ctx, uint32_t messageLen);


/*!
 *****************************************************************************
 * \brief Set Read Only
 *
 * This method performs the transition from the READ/WRITE state to the READ-ONLY state
 *
 * \param[in]   ctx       : ndef Context
 *
 * \return ERR_PARAM        : Invalid parameter
 * \return ERR_NONE         : No error
 *****************************************************************************
 */
ReturnCode ndefPollerSetReadOnly(ndefContext *ctx);


#endif /* NDEF_POLLER_H */

/**
  * @}
  *
  */
