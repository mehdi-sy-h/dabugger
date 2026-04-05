#include "leb.h"

ULEBReadResult read_uleb128(const uint8_t *const begin) {
	const uint8_t *data = begin;
	ULEBReadResult result = {0};
	do {
		result.value |= (uint64_t)(*data & 0x7f)
						<< (result.size_in_bytes++ * 7);
	} while ((*(data++) & 0x80) != 0);
	return result;
}
