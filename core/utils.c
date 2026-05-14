#include "utils.h"

#if LIONKEY_DEBUG_LEVEL > 2

uint8_t hexStrIdx = 0;

void dump_hex(const uint8_t *buf, size_t size) {
	debug_log("hex(%" PRIsz "): ", size);
	while (size--) {
		debug_log("%02" wPRIx8, *buf++);
	}
	debug_log(nl);
}

void dump_hex_large(const uint8_t *buf, size_t size)
{
	while (size--)
	{
		debug_log("%02" wPRIX8 , *buf++);
	}
	debug_log(nl);
}

#endif
