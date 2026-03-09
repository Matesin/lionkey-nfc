#include "utils.h"

#if LIONKEY_DEBUG_LEVEL > 2

#define MAX_HEX_STR         4
#define MAX_HEX_STR_LENGTH  128
uint8_t hexStrIdx = 0;
char hexStr[MAX_HEX_STR][MAX_HEX_STR_LENGTH];


void dump_hex(const uint8_t *buf, size_t size) {
	debug_log("hex(%" PRIsz "): ", size);
	while (size--) {
		debug_log("%02" wPRIx8, *buf++);
	}
	debug_log(nl);
}

char* hex2Str(unsigned char * data, size_t dataLen)
{
	const char * hex = "0123456789ABCDEF";

	unsigned char * pin  = data;
	char *          pout = hexStr[hexStrIdx];

	uint8_t idx = hexStrIdx;

	if( dataLen > (MAX_HEX_STR_LENGTH/2) )
	{
		dataLen = (MAX_HEX_STR_LENGTH/2) - 1;
	}

	for(uint32_t i = 0; i < dataLen; i++)
	{
		*pout++ = hex[(*pin>>4) & 0x0F];
		*pout++ = hex[(*pin++)  & 0x0F];
	}
	*pout = 0;

	hexStrIdx++;
	hexStrIdx %= MAX_HEX_STR;

	return hexStr[idx];
}

#endif
