#ifndef _DABUGGER_LEB_H
#define _DABUGGER_LEB_H

#include <stddef.h>
#include <stdint.h>

typedef struct ULEBReadResult {
	uint64_t value;
	size_t size_in_bytes;
} ULEBReadResult;

/* TODO: Type for ULEB128 values? */

/* Read ULEB128 value from a binary stream */
extern ULEBReadResult read_uleb128(const uint8_t *const begin);

#endif /* _DABUGGER_LEB_H */
