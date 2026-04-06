#include "dwarf.h"
#include "elf.h"
#include "leb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct Cursor {
	const uint8_t *ptr;
	size_t remaining;
} Cursor;

static inline void read_bytes(Cursor *cursor, void *dst, size_t n) {
	n = n <= cursor->remaining ? n : cursor->remaining;
	memcpy(dst, cursor->ptr, n);
	cursor->ptr += n;
	cursor->remaining -= n;
}

static LineNumProgHeader64 parse_line_header64(Cursor *cursor) {
	LineNumProgHeader64 header;

	read_bytes(cursor, &header.unit_length, sizeof(header.unit_length));
	read_bytes(cursor, &header.version, sizeof(header.version));
	read_bytes(cursor, &header.address_size, sizeof(header.address_size));
	read_bytes(cursor, &header.segment_selector_size,
			   sizeof(header.segment_selector_size));
	read_bytes(cursor, &header.header_length, sizeof(header.header_length));
	read_bytes(cursor, &header.minimum_instruction_length,
			   sizeof(header.minimum_instruction_length));
	read_bytes(cursor, &header.maximum_operations_per_instruction,
			   sizeof(header.maximum_operations_per_instruction));
	read_bytes(cursor, &header.default_is_stmt, sizeof(header.default_is_stmt));
	read_bytes(cursor, &header.line_base, sizeof(header.line_base));
	read_bytes(cursor, &header.line_range, sizeof(header.line_range));
	read_bytes(cursor, &header.opcode_base, sizeof(header.opcode_base));

	/* TODO: In the places where you are setting cursor fields directly, do
	 * safety checks */
	header.standard_opcode_lengths = cursor->ptr;
	cursor->ptr += header.opcode_base - 1;
	cursor->remaining -= header.opcode_base - 1;

	read_bytes(cursor, &header.directory_entry_format_count,
			   sizeof(header.directory_entry_format_count));

	/* TODO: Write/allocate ULEB pairs in header struct, may need to refactor
	 * types, need to consider ownership as well. Directories and file names
	 * will most likely need to be stored on the heap because of the variation
	 * in format types. */
	for (uint8_t i = 0; i < header.directory_entry_format_count; i++) {
		ULEBReadResult content_type = read_uleb128(cursor->ptr);
		cursor->ptr += content_type.bytes_read;
		cursor->remaining -= content_type.bytes_read;

		ULEBReadResult form_code = read_uleb128(cursor->ptr);
		cursor->ptr += form_code.bytes_read;
		cursor->remaining -= form_code.bytes_read;
	}

	return header;
}

void parse_debug_line_section(DebugLineSection debug_line_section) {
	/* See DWARF 5 Specification Table 6.4 */
	LineNumStateMachine state_machine = {
		.address = 0,
		.op_index = 0,
		.file = 1,
		.line = 1,
		.column = 0,
		/* TODO(obtain from program header): .is_stmt = ... */
		.basic_block = false,
		.end_sequence = false,
		.prologue_end = false,
		.epilogue_begin = false,
		.isa = 0,
		.discriminator = 0,
	};

	Cursor cursor = {.ptr = debug_line_section.data,
					 .remaining = debug_line_section.size};
	parse_line_header64(&cursor);
}
