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
	READ_ERR_UNKNOWN,
} ReadStatus;

typedef struct {
	size_t bytes_read;
	ReadStatus status;
} ReadResult;

extern ReadResult advance_reader(BinaryReader *reader, size_t bytes);

extern ReadResult read_bytes(BinaryReader *reader, void *out, size_t n);

extern ReadResult read_uleb128(BinaryReader *reader, uint64_t *value);

extern ReadResult read_sleb128(BinaryReader *reader, uint64_t *value);

extern ReadResult read_cstring(BinaryReader *reader, char *out);

#endif /* _DABUGGER_READER_H */
