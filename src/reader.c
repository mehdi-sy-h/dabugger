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
	result.bytes_read = bytes;
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
	result.bytes_read = n;
	return result;
}

ReadResult read_uleb128(BinaryReader *reader, uint64_t *value) {
	ReadResult result = {0};
	uint64_t data = 0;
	do {
		ReadResult byte_read_result = read_bytes(reader, &data, 1);

		if (byte_read_result.status != READ_OK) {
			/* TODO: Maybe more specific error instead */
			result.status = byte_read_result.status;
			return result;
		} else if (byte_read_result.bytes_read != 1) {
			/* Should not happen */
			result.status = READ_ERR_UNKNOWN;
			return result;
		}

		result.bytes_read++;

		*value |= (uint64_t)(data & 0x7f) << ((result.bytes_read - 1) * 7);
	} while ((data & 0x80) != 0);
	return result;
}

ReadResult read_cstring(BinaryReader *reader, char *out) {
	ReadResult result = {0};

	char *begin = (char *)(reader->cursor);
	char current_char;

	do {
		ReadResult byte_read_result = read_bytes(reader, &current_char, 1);

		if (byte_read_result.status != READ_OK) {
			/* TODO: Maybe more specific error instead */
			result.status = byte_read_result.status;
			return result;
		} else if (byte_read_result.bytes_read != 1) {
			/* Should not happen */
			result.status = READ_ERR_UNKNOWN;
			return result;
		}

		result.bytes_read++;
	} while (current_char != '\0');

	memcpy(out, begin, result.bytes_read);

	return result;
}
