#include "dwarf.h"
#include "elf.h"
#include "reader.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* TODO: 32 bit parsing is more important than 64 bit! */
static LineNumProgHeader64 alloc_line_header64(BinaryReader *reader) {
	LineNumProgHeader64 header;

	/* TODO: Handle read results */
	read_bytes(reader, &header,
			   sizeof(header.unit_length) + sizeof(header.version) +
				   sizeof(header.address_size) +
				   sizeof(header.segment_selector_size) +
				   sizeof(header.header_length) +
				   sizeof(header.minimum_instruction_length) +
				   sizeof(header.maximum_operations_per_instruction) +
				   sizeof(header.default_is_stmt) + sizeof(header.line_base) +
				   sizeof(header.line_range) + sizeof(header.opcode_base));

	header.standard_opcode_lengths =
		calloc(header.opcode_base - 1, sizeof(uint8_t));
	if (header.standard_opcode_lengths == NULL) {
		/* TODO: Handle failure */
	}

	read_bytes(reader, header.standard_opcode_lengths, header.opcode_base - 1);
	read_bytes(reader, &header.directory_entry_format_count,
			   sizeof(header.directory_entry_format_count));

	header.directory_entry_format =
		calloc(sizeof(DwarfFormatDesc), header.directory_entry_format_count);
	if (header.directory_entry_format == NULL) {
		/* TODO: Handle failure */
	}

	for (uint8_t i = 0; i < header.directory_entry_format_count; i++) {
		read_uleb128(reader, &header.directory_entry_format[i].content_type);
		read_uleb128(reader, &header.directory_entry_format[i].form_code);
	}

	read_uleb128(reader, &header.directories_count);

	/* Vibe slop printout start */
	printf("unit_length (raw): %02x %02x %02x %02x %02x %02x %02x %02x %02x "
		   "%02x %02x %02x\n",
		   header.unit_length.value[0], header.unit_length.value[1],
		   header.unit_length.value[2], header.unit_length.value[3],
		   header.unit_length.value[4], header.unit_length.value[5],
		   header.unit_length.value[6], header.unit_length.value[7],
		   header.unit_length.value[8], header.unit_length.value[9],
		   header.unit_length.value[10], header.unit_length.value[11]);
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
		printf("  format[%u]: content_type=%" PRIu64 " form_code=%" PRIu64 "\n",
			   i, header.directory_entry_format[i].content_type,
			   header.directory_entry_format[i].form_code);
	}
	printf("directories_count: %" PRIu64 "\n", header.directories_count);
	/* Vibe slop printout end */

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
	/*free(header->file_name_entry_format);*/
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

	BinaryReader reader = {.cursor = debug_line_section.data,
						   .remaining = debug_line_section.size};

	LineNumProgHeader64 header = alloc_line_header64(&reader);
	free_line_header64(&header);
}
