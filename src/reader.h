#ifndef _DABUGGER_READER_H
#define _DABUGGER_READER_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
	const uint8_t* cursor;
	size_t remaining;
} BinaryReader;

typedef enum {
	READ_OK = 0,
	READ_ERR_OUT_OF_BOUNDS,
	READ_ERR_LEB_U64_OVERFLOW,
	READ_ERR_LEB_I64_OVERFLOW
} ReadStatus;

typedef struct {
	size_t bytes_consumed;
	ReadStatus status;
} ReadResult;

extern ReadResult advance_reader(BinaryReader *reader, size_t bytes);

extern ReadResult read_bytes(BinaryReader *reader, void *out, size_t n);

extern ReadResult read_uleb128(BinaryReader *reader, uint64_t *out);

extern ReadResult read_sleb128(BinaryReader *reader, int64_t *out);

extern ReadResult read_cstring(BinaryReader *reader, const char** out);

#endif /* _DABUGGER_READER_H */
