#include "dwarf.h"
#include "elf.h"
#include "leb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Cursor {
	const uint8_t *ptr;
	size_t remaining;
} Cursor;

/* TODO: Implement and replace naked advancements with this to incorporate
 * safety checks. */
static inline void advance_cursor(Cursor *cursor, size_t n) {};

static inline void read_bytes(Cursor *cursor, void *dst, size_t n) {
	n = n <= cursor->remaining ? n : cursor->remaining;
	memcpy(dst, cursor->ptr, n);
	cursor->ptr += n;
	cursor->remaining -= n;
}

static LineNumProgHeader64 alloc_line_header64(Cursor *cursor) {
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

	header.directory_entry_format =
		malloc(sizeof(DwarfFormatDesc) * header.directory_entry_format_count);

	/* TODO: Safety checks */
	for (uint8_t i = 0; i < header.directory_entry_format_count; i++) {
		ULEBReadResult content_type = read_uleb128(cursor->ptr);
		cursor->ptr += content_type.bytes_read;
		cursor->remaining -= content_type.bytes_read;

		header.directory_entry_format[i].content_type = content_type.value;

		ULEBReadResult form_code = read_uleb128(cursor->ptr);
		cursor->ptr += form_code.bytes_read;
		cursor->remaining -= form_code.bytes_read;

		header.directory_entry_format[i].form_code = form_code.value;
	}

	ULEBReadResult directories_count = read_uleb128(cursor->ptr);
	cursor->ptr += directories_count.bytes_read;
	cursor->remaining -= directories_count.bytes_read;

	header.directories_count = directories_count.value;

	/* TODO: directories */

	for (uint64_t i = 0; i < header.directories_count; i++) {
		for (uint8_t j = 0; j < header.directory_entry_format_count; j++) {
			DwarfFormatDesc format_desc = header.directory_entry_format[j];
			switch (format_desc.content_type) {
			case DW_LNCT_path:
				if (format_desc.form_code == DW_FORM_string) {
					/* TODO: Read null terminated string */
				} else if (format_desc.form_code == DW_FORM_line_strp ||
						   format_desc.form_code == DW_FORM_strp ||
						   format_desc.form_code == DW_FORM_strp_sup) {
					/* TODO: Read uint8_t offset */
				} else if (format_desc.form_code == DW_FORM_strx ||
						   format_desc.form_code == DW_FORM_strx1 ||
						   format_desc.form_code == DW_FORM_strx2 ||
						   format_desc.form_code == DW_FORM_strx3 ||
						   format_desc.form_code == DW_FORM_strx4) {
					/* TODO: This is .debug_line.dwo section stuff idk if I'll
					 * handle it (or split objects at all) */
				} else {
					/* TODO: Handle invalid case */
				}
				break;
			case DW_LNCT_directory_index:
				if (format_desc.form_code == DW_FORM_data1) {
					/* TODO: Read uint8_t */
				} else if (format_desc.form_code == DW_FORM_data2) {
					/* TODO: Read uint16_t */
				} else if (format_desc.form_code == DW_FORM_udata) {
					/* TODO: Read ULEB128 */
				} else {
					/* TODO: Handle invalid case */
				}
				break;
			case DW_LNCT_timestamp:
				if (format_desc.form_code == DW_FORM_udata) {
					/* TODO: Read ULEB128 */
				} else if (format_desc.form_code == DW_FORM_data4) {
					/* TODO: Read uint32_t */
				} else if (format_desc.form_code == DW_FORM_data8) {
					/* TODO: Read uint64_t */
				} else if (format_desc.form_code == DW_FORM_block) {
					/* TODO: Read ULEB128 length n followed by n bytes */
				} else {
					/* TODO: Handle invalid case */
				}
				break;
			case DW_LNCT_size:
				if (format_desc.form_code == DW_FORM_udata) {
					/* TODO: Read ULEB128 */
				} else if (format_desc.form_code == DW_FORM_data1) {
					/* TODO: Read uint8_t */
				} else if (format_desc.form_code == DW_FORM_data2) {
					/* TODO: Read uint16_t */
				} else if (format_desc.form_code == DW_FORM_data4) {
					/* TODO: Read uint32_t */
				} else if (format_desc.form_code == DW_FORM_data8) {
					/* TODO: Read uint64_t */
				} else {
					/* TODO: Handle invalid case */
				}
				break;
			case DW_LNCT_MD5:
				if (format_desc.form_code == DW_FORM_data16) {
					/* TODO: Read 16 byte MD5 digest */
				} else {
					/* TODO: Handle invalid case */
				}
			default:
				/* TODO: Handle invalid case */;
			}
		}
	}

	return header;
}

static void free_line_header64(LineNumProgHeader64 *header) {
	free(header->directory_entry_format);
	free(header->file_name_entry_format);
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

	LineNumProgHeader64 header = alloc_line_header64(&cursor);
	free_line_header64(&header);
}
