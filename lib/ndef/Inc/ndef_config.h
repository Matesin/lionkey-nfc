//
// Created by Maty Martan on 02.03.2026.
//

#ifndef LIONKEY_NDEF_CONFIG_H
#define LIONKEY_NDEF_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#include "rfal_nfc.h"
#include "rfal_t4t.h"

#define ndefDevice                rfalNfcDevice           /*!< NDEF Device         */
#define NDEF_FEATURE_T4T          RFAL_FEATURE_T4T        /*!< T4T Support control */


#define NDEF_FEATURE_FULL_API                  true       /*!< Support Write, Format, Check Presence, set Read-only in addition to the Read feature */

#define NDEF_TYPE_EMPTY_SUPPORT                true       /*!< Support Empty type                          */
#define NDEF_TYPE_FLAT_SUPPORT                 true       /*!< Support Flat type                           */
#define NDEF_TYPE_RTD_DEVICE_INFO_SUPPORT      true       /*!< Support RTD Device Information type         */
#define NDEF_TYPE_RTD_TEXT_SUPPORT             true       /*!< Support RTD Text type                       */
#define NDEF_TYPE_RTD_URI_SUPPORT              true       /*!< Support RTD URI type                        */
#define NDEF_TYPE_RTD_AAR_SUPPORT              false       /*!< Support RTD Android Application Record type */
#define NDEF_TYPE_RTD_WLC_SUPPORT              false       /*!< Support RTD WLC Types                       */
#define NDEF_TYPE_RTD_WPCWLC_SUPPORT           false       /*!< Support RTD WPC WLC type                    */
#define NDEF_TYPE_RTD_TNEP_SUPPORT             false       /*!< Support RTD TNEP Types                      */
#define NDEF_TYPE_MEDIA_SUPPORT                false       /*!< Support Media type                         */
#define NDEF_TYPE_BLUETOOTH_SUPPORT            false       /*!< Support Bluetooth types                    */
#define NDEF_TYPE_VCARD_SUPPORT                false       /*!< Support vCard type                          */
#define NDEF_TYPE_WIFI_SUPPORT                 false       /*!< Support Wifi type                          */

#endif //LIONKEY_NDEF_CONFIG_H