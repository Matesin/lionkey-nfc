//
// Created by Maty Martan on 03.03.2026.
//

#ifndef LIONKEY_ERRNO_H
#define LIONKEY_ERRNO_H

#include <stddef.h>

#define ERR_NONE 0
#define ERR_PARAM -1
#define ERR_PROTO -2
#define ERR_INTERNAL -3
#define ERR_NOT_IMPLEMENTED -4
#define ERR_NOMEM -5
#define ERR_REQUEST -6
#define ERR_WRONG_STATE -7
#define ERR_NOTSUPP -8
#define ERR_BUSY -9
#define ERR_TIMEOUT -10
#define ERR_SYSTEM -11

#ifndef ERR_NONE
  #define ERR_NONE            RFAL_ERR_NONE
  #define ERR_PARAM           RFAL_ERR_PARAM
  #define ERR_PROTO           RFAL_ERR_PROTO
  #define ERR_INTERNAL        RFAL_ERR_INTERNAL
  #define ERR_NOT_IMPLEMENTED RFAL_ERR_NOT_IMPLEMENTED
  #define ERR_NOMEM           RFAL_ERR_NOMEM
  #define ERR_REQUEST         RFAL_ERR_REQUEST
  #define ERR_WRONG_STATE     RFAL_ERR_WRONG_STATE
  #define ERR_NOTSUPP         RFAL_ERR_NOTSUPP
#endif

static const char* rfalErrorToStr(int code);
void errorToString(int errorCode);

#endif //LIONKEY_ERRNO_H