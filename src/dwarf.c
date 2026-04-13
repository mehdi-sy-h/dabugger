#include "dwarf.h"
#include "elf.h"
#include "reader.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void read_lnct_path(BinaryReader *debug_line_reader,
						   SectionBuffer *debug_line_str_buffer,
						   DwarfFormCode form_code,
						   DwarfLineNumContentEntry *out_entry) {
	const char *path;
	ReadResult result;

	if (form_code == DW_FORM_string) {
		/* TODO: Could be bad, malloc path and copy into that instead?
		 * Or reimplement read_cstring to return a copy of the string. */
		result = read_cstring(debug_line_reader, &path);
	} else if (form_code == DW_FORM_line_strp || form_code == DW_FORM_strp ||
			   form_code == DW_FORM_strp_sup) {
		uint64_t offset; /* TODO: This is uint32_t if using 32 bit dwarf */
		result = read_bytes(debug_line_reader, &offset, 8);

		if (form_code == DW_FORM_line_strp) {
			/* TODO: Again for similar reasons as above this could be unsafe
			 * returning a pointer to a buffer with spurious lifetime.
			 * If allocations get too annoying perhaps switch to an arena
			 * because the lifetime of all these things are mostly the same. */
			path = (const char *)(debug_line_str_buffer->data) + offset;
		} else if (form_code == DW_FORM_strp) {
			/* TODO */
		} else if (form_code == DW_FORM_strp_sup) {
			/* Supplementary string section (in split object file), probably
			 * wont handle. */
		}
	} else if (form_code == DW_FORM_strx || form_code == DW_FORM_strx1 ||
			   form_code == DW_FORM_strx2 || form_code == DW_FORM_strx3 ||
			   form_code == DW_FORM_strx4) {
		/* This is .debug_line.dwo section stuff idk if I'll handle it (or split
		 * objects at all) */
	} else {
		/* TODO: Handle invalid case */
	}

	/* TODO: malloc error? */
	/*
	if (path == NULL) {
	}
	*/

	if (result.status != READ_OK) {
		/* TODO: Handle read error */
		return;
	}

	printf("%s\n", path);
	out_entry->path = path;
}

static void read_lnct_directory_index(BinaryReader *reader,
									  DwarfFormCode form_code,
									  DwarfLineNumContentEntry *out_entry) {
	uint64_t directory_index = 0;
	ReadResult result;

	if (form_code == DW_FORM_data1) {
		result = read_bytes(reader, &directory_index, 1);
	} else if (form_code == DW_FORM_data2) {
		result = read_bytes(reader, &directory_index, 2);
	} else if (form_code == DW_FORM_udata) {
		result = read_uleb128(reader, &directory_index);
	} else {
		/* TODO: Handle invalid case */
	}

	if (result.status != READ_OK) {
		/* TODO: Handle error */
		return;
	}

	out_entry->directory_index = directory_index;
}

static void read_lnct_timestamp(BinaryReader *reader, DwarfFormCode form_code,
								DwarfLineNumContentEntry *out_entry) {
	uint64_t timestamp;
	ReadResult result;

	if (form_code == DW_FORM_udata) {
		result = read_uleb128(reader, &timestamp);
	} else if (form_code == DW_FORM_data4) {
		result = read_bytes(reader, &timestamp, 4);
	} else if (form_code == DW_FORM_data8) {
		result = read_bytes(reader, &timestamp, 8);
	} else if (form_code == DW_FORM_block) {
		/* TODO: Read ULEB128 length n followed by n bytes */
	} else {
		/* TODO: Handle invalid case */
	}

	if (result.status != READ_OK) {
		/* TODO: Handle error */
		return;
	}

	out_entry->timestamp = timestamp;
}
static void read_lnct_size(BinaryReader *reader, DwarfFormCode form_code,
						   DwarfLineNumContentEntry *out_entry) {
	uint64_t size;
	ReadResult result;

	if (form_code == DW_FORM_udata) {
		result = read_uleb128(reader, &size);
	} else if (form_code == DW_FORM_data1) {
		result = read_bytes(reader, &size, 1);
	} else if (form_code == DW_FORM_data2) {
		result = read_bytes(reader, &size, 2);
	} else if (form_code == DW_FORM_data4) {
		result = read_bytes(reader, &size, 4);
	} else if (form_code == DW_FORM_data8) {
		result = read_bytes(reader, &size, 8);
	} else {
		/* TODO: Handle invalid case */
	}

	if (result.status != READ_OK) {
		/* TODO: Handle error */
		return;
	}

	out_entry->size = size;
}
static void read_lnct_md5(BinaryReader *reader, DwarfFormCode form_code,
						  DwarfLineNumContentEntry *out_entry) {
	uint8_t md5[16];
	ReadResult result;

	if (form_code == DW_FORM_data16) {
		result = read_bytes(reader, &md5, 16);
	} else {
		/* TODO: Handle invalid case */
	}

	if (result.status != READ_OK) {
		/* TODO: Handle error */
		return;
	}

	memcpy(out_entry->md5, md5, 16);
}

/* TODO: 32 bit parsing is more important than 64 bit! */
static LineNumProgHeader64 alloc_line_header64(DebugSections *sections) {
	/* TODO: Put assert guards here (and all around the code base!) */
	LineNumProgHeader64 header;

	printf("sz: %ld\n", sections->debug_line.size);
	BinaryReader debug_line_reader = {
		.cursor = sections->debug_line.data,
		.remaining = sections->debug_line.size,
	};

	/* TODO: Handle read results */
	read_bytes(&debug_line_reader, &header.unit_length,
			   sizeof(header.unit_length));
	read_bytes(&debug_line_reader, &header.version, sizeof(header.version));
	read_bytes(&debug_line_reader, &header.address_size,
			   sizeof(header.address_size));
	read_bytes(&debug_line_reader, &header.segment_selector_size,
			   sizeof(header.segment_selector_size));
	read_bytes(&debug_line_reader, &header.header_length,
			   sizeof(header.header_length));
	read_bytes(&debug_line_reader, &header.minimum_instruction_length,
			   sizeof(header.minimum_instruction_length));
	read_bytes(&debug_line_reader, &header.maximum_operations_per_instruction,
			   sizeof(header.maximum_operations_per_instruction));
	read_bytes(&debug_line_reader, &header.default_is_stmt,
			   sizeof(header.default_is_stmt));
	read_bytes(&debug_line_reader, &header.line_base, sizeof(header.line_base));
	read_bytes(&debug_line_reader, &header.line_range,
			   sizeof(header.line_range));
	read_bytes(&debug_line_reader, &header.opcode_base,
			   sizeof(header.opcode_base));

	header.standard_opcode_lengths =
		calloc(header.opcode_base - 1, sizeof(uint8_t));
	if (header.standard_opcode_lengths == NULL) {
		/* TODO: Handle failure */
	}

	read_bytes(&debug_line_reader, header.standard_opcode_lengths,
			   header.opcode_base - 1);
	read_bytes(&debug_line_reader, &header.directory_entry_format_count,
			   sizeof(header.directory_entry_format_count));

	header.directory_entry_format = calloc(sizeof(DwarfLineNumFormatDesc),
										   header.directory_entry_format_count);
	if (header.directory_entry_format == NULL) {
		/* TODO: Handle failure */
	}

	for (uint8_t i = 0; i < header.directory_entry_format_count; i++) {
		uint64_t content_type_tmp, form_code_tmp;

		read_uleb128(&debug_line_reader, &content_type_tmp);
		header.directory_entry_format[i].content_type =
			(DwarfLineNumContentType)content_type_tmp;

		read_uleb128(&debug_line_reader, &form_code_tmp);
		header.directory_entry_format[i].form_code =
			(DwarfFormCode)form_code_tmp;
	}

	read_uleb128(&debug_line_reader, &header.directories_count);

	header.directories =
		calloc(header.directories_count, sizeof(DwarfLineNumContentEntry));

	if (header.directories == NULL) {
		/* TODO: Handle error */
	}

	/* Vibe slop printout start */
	/*
	printf("unit_length (raw): %x %lx\n", header.unit_length.marker,
		   header.unit_length.length);
	*/

	printf("unit_length (raw): %02x %02x %02x %02x %02x %02x %02x %02x %02x "
		   "%02x %02x %02x\n",
		   header.unit_length.marker[0], header.unit_length.marker[1],
		   header.unit_length.marker[2], header.unit_length.marker[3],
		   header.unit_length.length[0], header.unit_length.length[1],
		   header.unit_length.length[2], header.unit_length.length[3],
		   header.unit_length.length[4], header.unit_length.length[5],
		   header.unit_length.length[6], header.unit_length.length[7]);
	printf("version: %u\n", header.version);
	printf("address_size: %u\n", header.address_size);
	printf("segment_selector_size: %u\n", header.segment_selector_size);
	printf("header_length: %zu\n", header.header_length);
	printf("minimum_instruction_length: %u\n",
		   header.minimum_instruction_length);
	printf("maximum_operations_per_instruction: %u\n",
		   header.maximum_operations_per_instruction);
	printf("default_is_stmt: %u\n", header.default_is_stmt);
	printf("line_base: %d\n", header.line_base);
	printf("line_range: %u\n", header.line_range);
	printf("opcode_base: %u\n", header.opcode_base);
	printf("standard_opcode_lengths:");
	for (uint8_t i = 0; i < header.opcode_base - 1; i++) {
		printf(" %u", header.standard_opcode_lengths[i]);
	}
	printf("\n");
	printf("directory_entry_format_count: %u\n",
		   header.directory_entry_format_count);
	for (uint8_t i = 0; i < header.directory_entry_format_count; i++) {
		printf("  format[%u]: content_type=%ud form_code=%ud\n", i,
			   header.directory_entry_format[i].content_type,
			   header.directory_entry_format[i].form_code);
	}
	printf("directories_count: %ld\n", header.directories_count);
	/* Vibe slop printout end */

	for (uint64_t i = 0; i < header.directories_count; i++) {
		for (uint8_t j = 0; j < header.directory_entry_format_count; j++) {
			DwarfLineNumFormatDesc fmt = header.directory_entry_format[j];
			DwarfLineNumContentEntry entry = {0};
			header.directories[i] = entry;
			switch (fmt.content_type) {
			case DW_LNCT_path:
				read_lnct_path(&debug_line_reader, &sections->debug_line_str,
							   fmt.form_code, &entry);
				break;
			case DW_LNCT_directory_index:
				read_lnct_directory_index(&debug_line_reader, fmt.form_code,
										  &entry);
				break;
			case DW_LNCT_timestamp:
				read_lnct_timestamp(&debug_line_reader, fmt.form_code, &entry);
				break;
			case DW_LNCT_size:
				read_lnct_size(&debug_line_reader, fmt.form_code, &entry);
				break;
			case DW_LNCT_MD5:
				read_lnct_md5(&debug_line_reader, fmt.form_code, &entry);
				break;
			default:
				/* Skip vendor defined content descriptions */
				if (fmt.content_type >= DW_LNCT_lo_user &&
					fmt.content_type <= DW_LNCT_hi_user) {
					continue;
				}
				/* TODO: Handle invalid case */;
			}
		}
	}

	return header;
}

static void free_line_header64(LineNumProgHeader64 *header) {
	free(header->directory_entry_format);
	/*free(header->file_name_entry_format);*/
}

void parse_debug_line_section(DebugSections debug_sections) {
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

	LineNumProgHeader64 header = alloc_line_header64(&debug_sections);
	free_line_header64(&header);
}
