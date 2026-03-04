// lib/ndef/Src/errno.c
//
// Created by Maty Martan on 03.03.2026.
//

#include "ndef_errno.h"
#include "utils.h"

void errorToString(int errorCode)
{
    if (errorCode == ERR_NONE) {
        debug_log("No error\n");
        return;
    }

    debug_log(red("NDEF ERROR: %d - "), errorCode);
    debug_log(red("%s") nl, rfalErrorToStr(errorCode));
}

static const char* rfalErrorToStr(const int code)
{
    {
    /* Table indexed by numeric error code.
     * Unknown/unused slots are NULL => handled below.
     */
    static const char* const msg[] = {
        [0]  = "No error occurred",
        [1]  = "Not enough memory",
        [2]  = "Device or resource busy",
        [3]  = "Generic IO error",
        [4]  = "Timeout",
        [5]  = "Invalid request / can't execute now",
        [6]  = "No message of desired type",
        [7]  = "Parameter error",
        [8]  = "System error",
        [9]  = "Framing error",
        [10] = "Overrun: lost one or more received bytes",
        [11] = "Protocol error",
        [12] = "Internal error",
        [13] = "Call again",
        [14] = "Memory corruption",
        [15] = "Not implemented",
        [16] = "PC corrupt / illegal operation (noise/spike?)",
        [17] = "Error sending",
        [18] = "Error detected but to be ignored",
        [19] = "Semantic error (unexpected cmd)",
        [20] = "Syntax error (unknown cmd)",
        [21] = "CRC error",
        [22] = "Transponder not found",
        [23] = "Transponder not unique (collision)",
        [24] = "Operation not supported",
        [25] = "Write error",
        [26] = "FIFO over/underflow",
        [27] = "Parity error",
        [28] = "Transfer already finished",
        [29] = "RF collision error",
        [30] = "HW overrun: lost one or more received bytes",
        [31] = "Device requested release",
        [32] = "Device requested sleep",
        [33] = "Wrong state",
        [34] = "Blocking procedure reached max reruns",
        [35] = "Disabled configuration",
        [36] = "HW mismatch",
        [37] = "Link loss / other field didn't behave as expected",
        [38] = "Invalid or uninitialized handle",

        /* 39 intentionally unused */

        [40] = "Incomplete byte received",
        [41] = "Incomplete byte received (1 bit)",
        [42] = "Incomplete byte received (2 bits)",
        [43] = "Incomplete byte received (3 bits)",
        [44] = "Incomplete byte received (4 bits)",
        [45] = "Incomplete byte received (5 bits)",
        [46] = "Incomplete byte received (6 bits)",
        [47] = "Incomplete byte received (7 bits)",
    };

    if (code < 0 || code >= (int)(sizeof(msg) / sizeof(msg[0])) || msg[code] == NULL) {
        return "Unknown error";
    }
    return msg[code];
}
}

