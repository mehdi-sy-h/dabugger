#include "reader.h"

#include <string.h>

ReadResult advance_reader(BinaryReader *reader, size_t bytes) {
	ReadResult result = {0};
	if (bytes > reader->remaining) {
		result.status = READ_ERR_OUT_OF_BOUNDS;
		return result;
	}
	reader->cursor += bytes;
	reader->remaining -= bytes;
	result.bytes_consumed = bytes;
	return result;
}

ReadResult read_bytes(BinaryReader *reader, void *out, size_t n) {
	ReadResult result = {0};
	if (n > reader->remaining) {
		result.status = READ_ERR_OUT_OF_BOUNDS;
		return result;
	}
	memcpy(out, reader->cursor, n);
	reader->cursor += n;
	reader->remaining -= n;
	result.bytes_consumed = n;
	return result;
}

ReadResult read_uleb128(BinaryReader *reader, uint64_t *out) {
	ReadResult result = {0};
	uint8_t byte;
	*out = 0;
	do {
		if (result.bytes_consumed == 10) {
			result.status = READ_ERR_ULEB_U64_OVERFLOW;
			return result;
		}

		ReadResult byte_read_result = read_bytes(reader, &byte, 1);

		if (byte_read_result.status != READ_OK) {
			/* TODO: Maybe more specific error instead */
			result.status = byte_read_result.status;
			return result;
		}

		*out |= (uint64_t)(byte & 0x7f) << ((result.bytes_consumed) * 7);
		result.bytes_consumed++;
	} while ((byte & 0x80) != 0);
	return result;
}

ReadResult read_sleb128(BinaryReader *reader, uint64_t *out) {
	/* TODO: Implement */
}

/* TODO: Copy instead of returning pointer to string */
ReadResult read_cstring(BinaryReader *reader, const char **out) {
	ReadResult result = {0};
	void *end = memchr(reader->cursor, '\0', reader->remaining);
	if (!end) {
		result.status = READ_ERR_OUT_OF_BOUNDS;
		return result;
	}
	*out = (const char *)reader->cursor;
	result.bytes_consumed = (uint8_t *)end - reader->cursor + 1;
	reader->cursor += result.bytes_consumed;
	reader->remaining -= result.bytes_consumed;
	return result;
}
