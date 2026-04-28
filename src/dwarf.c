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

/* TODO: Define and use this and pass it around static functions in dwarf.c */
typedef struct {
} DwarfLineContext;

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
			/* TODO: Read from .debug_str */
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

	/*printf("%s\n", path);*/
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
	/* TODO: Fix for big endian */
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
		size_t bytes_count = 0;
		result = read_uleb128(reader, &bytes_count);
		/* TODO: Handle error */
		result = read_bytes(reader, &timestamp, bytes_count);
		/* TODO: Handle overflow */
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
	/* Fix for big endian */
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

static void read_lnct_entries(DwarfLineNumContentEntry *entries,
							  DwarfLineNumFormatDesc *formats,
							  uint64_t entry_count, uint8_t format_count,
							  BinaryReader *debug_line_reader,
							  SectionBuffer *debug_line_str) {
	/*printf("\n------\n");*/
	for (uint64_t i = 0; i < entry_count; i++) {
		DwarfLineNumContentEntry entry = {0};
		for (uint8_t j = 0; j < format_count; j++) {
			DwarfLineNumFormatDesc fmt = formats[j];
			/* TODO: Fix */

			switch (fmt.content_type) {
			case DW_LNCT_path:
				/*printf("[%ld]: ", i);*/
				read_lnct_path(debug_line_reader, debug_line_str, fmt.form_code,
							   &entry);
				break;
			case DW_LNCT_directory_index:
				read_lnct_directory_index(debug_line_reader, fmt.form_code,
										  &entry);
				break;
			case DW_LNCT_timestamp:
				read_lnct_timestamp(debug_line_reader, fmt.form_code, &entry);
				break;
			case DW_LNCT_size:
				read_lnct_size(debug_line_reader, fmt.form_code, &entry);
				break;
			case DW_LNCT_MD5:
				read_lnct_md5(debug_line_reader, fmt.form_code, &entry);
				break;
			default:
				/* Skip vendor defined content descriptions */
				if (fmt.content_type >= DW_LNCT_lo_user &&
					fmt.content_type <= DW_LNCT_hi_user) {
					/* TODO: Need to advance reader according to the present
					 * form codes */
				}
				/* TODO: Handle invalid case */;
			}
		}
		entries[i] = entry;
	}
}

/* TODO: 32 bit parsing is more important than 64 bit! */
static LineNumProgHeader64 *
parse_line_header64(ProgramSections *sections,
					BinaryReader *debug_line_reader) {
	/* TODO: Put assert guards here (and all around the code base!) */
	LineNumProgHeader64 *header = calloc(1, sizeof(LineNumProgHeader64));

	/* TODO: Handle read results */
	read_bytes(debug_line_reader, &header->unit_length,
			   sizeof(header->unit_length));
	read_bytes(debug_line_reader, &header->version, sizeof(header->version));
	read_bytes(debug_line_reader, &header->address_size,
			   sizeof(header->address_size));
	read_bytes(debug_line_reader, &header->segment_selector_size,
			   sizeof(header->segment_selector_size));
	read_bytes(debug_line_reader, &header->header_length,
			   sizeof(header->header_length));
	read_bytes(debug_line_reader, &header->minimum_instruction_length,
			   sizeof(header->minimum_instruction_length));
	read_bytes(debug_line_reader, &header->maximum_operations_per_instruction,
			   sizeof(header->maximum_operations_per_instruction));
	read_bytes(debug_line_reader, &header->default_is_stmt,
			   sizeof(header->default_is_stmt));
	read_bytes(debug_line_reader, &header->line_base,
			   sizeof(header->line_base));
	read_bytes(debug_line_reader, &header->line_range,
			   sizeof(header->line_range));
	read_bytes(debug_line_reader, &header->opcode_base,
			   sizeof(header->opcode_base));

	header->standard_opcode_lengths =

		calloc(header->opcode_base - 1, sizeof(uint8_t));
	if (header->standard_opcode_lengths == NULL) {
		/* TODO: Handle failure */
	}

	read_bytes(debug_line_reader, header->standard_opcode_lengths,
			   header->opcode_base - 1);

	read_bytes(debug_line_reader, &header->directory_entry_format_count,
			   sizeof(header->directory_entry_format_count));

	header->directory_entry_format = calloc(
		header->directory_entry_format_count, sizeof(DwarfLineNumFormatDesc));
	if (header->directory_entry_format == NULL) {
		/* TODO: Handle failure */
	}

	for (uint8_t i = 0; i < header->directory_entry_format_count; i++) {
		uint64_t content_type_tmp, form_code_tmp;

		read_uleb128(debug_line_reader, &content_type_tmp);
		header->directory_entry_format[i].content_type =
			(DwarfLineNumContentType)content_type_tmp;

		read_uleb128(debug_line_reader, &form_code_tmp);
		header->directory_entry_format[i].form_code =
			(DwarfFormCode)form_code_tmp;
	}

	read_uleb128(debug_line_reader, &header->directories_count);

	header->directories =
		calloc(header->directories_count, sizeof(DwarfLineNumContentEntry));

	if (header->directories == NULL) {
		/* TODO: Handle error */
	}

	read_lnct_entries(header->directories, header->directory_entry_format,
					  header->directories_count,
					  header->directory_entry_format_count, debug_line_reader,
					  &sections->debug_line_str);

	/* TODO: Put these reads and allocations in read_lnct_entries too? */
	read_bytes(debug_line_reader, &header->file_name_entry_format_count,
			   sizeof(header->file_name_entry_format_count));

	header->file_name_entry_format = calloc(
		header->file_name_entry_format_count, sizeof(DwarfLineNumFormatDesc));
	if (header->file_name_entry_format == NULL) {
		/* TODO: Handle failure */
	}

	for (uint8_t i = 0; i < header->file_name_entry_format_count; i++) {
		uint64_t content_type_tmp, form_code_tmp;

		read_uleb128(debug_line_reader, &content_type_tmp);
		header->file_name_entry_format[i].content_type =
			(DwarfLineNumContentType)content_type_tmp;

		read_uleb128(debug_line_reader, &form_code_tmp);
		header->file_name_entry_format[i].form_code =
			(DwarfFormCode)form_code_tmp;
	}

	read_uleb128(debug_line_reader, &header->file_names_count);

	header->file_names =
		calloc(header->file_names_count, sizeof(DwarfLineNumContentEntry));

	if (header->file_names == NULL) {
		/* TODO: Handle error */
	}

	read_lnct_entries(header->file_names, header->file_name_entry_format,
					  header->file_names_count,
					  header->file_name_entry_format_count, debug_line_reader,
					  &sections->debug_line_str);

	return header;
}

static void free_line_header64(LineNumProgHeader64 *header) {
	free(header->directory_entry_format);
	/*free(header->file_name_entry_format);*/
	/* TODO */
}

/* Successive insertions must be monotonically increasing in the
 * operation pointer (i.e. address for non-VLIW architectures). */
static void append_line_info_entry(LineInfoTable *line_info_table,
								   LineNumStateMachine *state_machine) {
	LineInfoSequence *sequence = NULL;

	if (state_machine->end_sequence || line_info_table->sequences_count == 0) {
		line_info_table->sequences_count += 1;
		line_info_table->sequences = reallocarray(
			line_info_table->sequences, line_info_table->sequences_count,
			sizeof(LineInfoSequence));

		if (line_info_table->sequences == NULL) {
			/* TODO */
		}

		sequence =
			&line_info_table->sequences[line_info_table->sequences_count - 1];
		sequence->entry_count = 0;
		sequence->entries = NULL;
		/*printf("(new seq) ");*/
	} else {
		/* The commented invariant above means we don't have to search for the
		 * containing sequence. We know that it must be the last. */
		sequence =
			&line_info_table->sequences[line_info_table->sequences_count - 1];
		/*printf("(same seq) ");*/
	}

	sequence->entry_count += 1;
	sequence->entries = reallocarray(sequence->entries, sequence->entry_count,
									 sizeof(LineInfoEntry));

	if (sequence->entries == NULL) {
		/* TODO */
	}

	LineInfoEntry *entry = &sequence->entries[sequence->entry_count - 1];
	entry->address = state_machine->address;
	entry->op_index = state_machine->op_index;
	entry->file = state_machine->file;
	entry->line = state_machine->line;
	entry->column = state_machine->column;
	entry->discriminator = state_machine->discriminator;
	entry->end_sequence = state_machine->end_sequence;
	entry->is_stmt = state_machine->is_stmt;
	entry->basic_block = state_machine->basic_block;
	entry->prologue_end = state_machine->prologue_end;
	entry->epilogue_begin = state_machine->epilogue_begin;

	/*
	printf("addr: 0x%zx, f: %u, line: %u, col: %u, "
		   "disc: %u, block: %b, stmt: %b, endseq: %b, "
		   "prologend: %b, epibegin: %b\n",
		   state_machine->address, state_machine->file, state_machine->line,
		   state_machine->column, state_machine->discriminator,
		   state_machine->basic_block, state_machine->is_stmt,
		   state_machine->end_sequence, state_machine->prologue_end,
		   state_machine->epilogue_begin);
	*/
}

/* See DWARF 5 Specification Table 6.4 for initial values */
static void reset_line_num_state_machine(LineNumStateMachine *state_machine,
										 bool default_is_stmt) {
	state_machine->address = 0;
	state_machine->op_index = 0;
	state_machine->file = 1;
	state_machine->line = 1;
	state_machine->column = 0;
	state_machine->is_stmt = default_is_stmt;
	state_machine->basic_block = false;
	state_machine->end_sequence = false;
	state_machine->prologue_end = false;
	state_machine->epilogue_begin = false;
	state_machine->isa = 0;
	state_machine->discriminator = 0;
}

static void state_machine_advance_pc(LineNumStateMachine *state_machine,
									 LineNumProgHeader64 *header,
									 uint64_t op_advance) {
	state_machine->address += header->minimum_instruction_length *
							  ((state_machine->op_index + op_advance) /
							   header->maximum_operations_per_instruction);
	state_machine->op_index = (state_machine->op_index + op_advance) %
							  header->maximum_operations_per_instruction;
}

static LineInfoTable decode_line_num_prog(LineNumProgHeader64 *header,
										  BinaryReader *debug_line_reader) {
	LineInfoTable line_info_table = {0};

	LineNumStateMachine state_machine;
	reset_line_num_state_machine(&state_machine, header->default_is_stmt);

	/* Unit length does not count the size of the unit length value itself.
	 * Likewise the header length only counts starting from after the header
	 * length value up until the end of the header.
	 * See DWARF 5 Specification Section 6.2.4 Page 154 */
	uint64_t line_prog_length =
		header->unit_length.length - sizeof(header->version) -
		sizeof(header->address_size) - sizeof(header->segment_selector_size) -
		sizeof(header->header_length) - header->header_length;
	assert(line_prog_length <= debug_line_reader->remaining);

	uint64_t orig_remaining = debug_line_reader->remaining;
	uint8_t opcode;

	while ((orig_remaining - debug_line_reader->remaining) < line_prog_length) {
		ReadResult result = read_bytes(debug_line_reader, &opcode, 1);
		if (result.status != READ_OK) {
			/* TODO: Handle read error, goto style */
			break;
		}

		if (opcode == 0) {
			/* Extended opcode */
			/* TODO: Handle read error */
			size_t instruction_size = 0;
			read_uleb128(debug_line_reader, &instruction_size);

			uint8_t extended_opcode = 0;
			read_bytes(debug_line_reader, &extended_opcode, 1);

			uint64_t discriminator = 0;
			/* This is architecture dependent, but we use the size_t of the
			 * debugger machine */
			size_t address = 0;
			switch (extended_opcode) {
			case DW_LNE_end_sequence:
				state_machine.end_sequence = true;
				append_line_info_entry(&line_info_table, &state_machine);
				reset_line_num_state_machine(&state_machine,
											 header->default_is_stmt);
				break;
			case DW_LNE_set_address:
				read_bytes(debug_line_reader, &address, instruction_size - 1);
				/* This is the only operation that directly stores an address
				 * rather than add a delta to it */
				state_machine.address = address;
				state_machine.op_index = 0;
				break;
			case DW_LNE_set_discriminator:
				read_uleb128(debug_line_reader, &discriminator);
				state_machine.discriminator = (uint32_t)discriminator;
				break;
			}
		} else if (opcode < header->opcode_base) {
			/* Standard opcode */
			uint64_t op_advance = 0;
			uint64_t file_index = 0;
			uint64_t column = 0;
			uint64_t isa = 0;
			uint16_t fixed_advance = 0;
			int64_t line_increment = 0;
			switch (opcode) {
			case DW_LNS_copy:
				append_line_info_entry(&line_info_table, &state_machine);
				state_machine.discriminator = 0;
				state_machine.basic_block = false;
				state_machine.prologue_end = false;
				state_machine.epilogue_begin = false;
				break;
			case DW_LNS_advance_pc:
				/* TODO: Handle read error */
				read_uleb128(debug_line_reader, &op_advance);
				state_machine.address +=
					header->minimum_instruction_length *
					((state_machine.op_index + op_advance) /
					 header->maximum_operations_per_instruction);
				state_machine.op_index =
					(state_machine.op_index + op_advance) %
					header->maximum_operations_per_instruction;
				break;
			case DW_LNS_advance_line:
				/* TODO: Handle read error */
				read_sleb128(debug_line_reader, &line_increment);
				state_machine.line += line_increment;
				break;
			case DW_LNS_set_file:
				/* TODO: Handle read error */
				read_uleb128(debug_line_reader, &file_index);
				state_machine.file = (uint32_t)file_index;
				break;
			case DW_LNS_set_column:
				/* TODO: Handle read error */
				read_uleb128(debug_line_reader, &column);
				state_machine.column = (uint32_t)column;
				break;
			case DW_LNS_negate_stmt:
				state_machine.is_stmt = !state_machine.is_stmt;
				break;
			case DW_LNS_set_basic_block:
				state_machine.basic_block = true;
				break;
			case DW_LNS_const_add_pc:
				op_advance = (255 - header->opcode_base) / header->line_range;
				state_machine_advance_pc(&state_machine, header, op_advance);
				break;
			case DW_LNS_fixed_advance_pc:
				/* TODO: Check if this is UB, and also handle read error */
				read_bytes(debug_line_reader, &fixed_advance, 2);
				state_machine.address += fixed_advance;
				state_machine.op_index = 0;
				break;
			case DW_LNS_set_prologue_end:
				state_machine.prologue_end = true;
				break;
			case DW_LNS_set_epilogue_begin:
				state_machine.epilogue_begin = true;
				break;
			case DW_LNS_set_isa:
				/* TODO: Handle read error */
				read_uleb128(debug_line_reader, &isa);
				state_machine.isa = (uint16_t)isa;
				break;
			}
		} else {
			/* Special opcode */
			uint8_t adjusted_opcode = opcode - header->opcode_base;
			uint8_t op_advance = adjusted_opcode / header->line_range;

			state_machine.line +=
				(uint32_t)(header->line_base +
						   (adjusted_opcode % header->line_range));

			state_machine_advance_pc(&state_machine, header, op_advance);

			append_line_info_entry(&line_info_table, &state_machine);

			state_machine.basic_block = false;
			state_machine.prologue_end = false;
			state_machine.epilogue_begin = false;
			state_machine.discriminator = 0;
		}
	}

	return line_info_table;
}

LineInfo *parse_debug_line_section(ProgramSections sections) {
	BinaryReader debug_line_reader = {
		.cursor = sections.debug_line.data,
		.remaining = sections.debug_line.size,
	};

	LineInfo *line_info = malloc(sizeof(LineInfo));
	line_info->comp_unit_count = 0;
	line_info->comp_units = NULL;

	while (debug_line_reader.remaining > 0) {
		LineNumProgHeader64 *header =
			parse_line_header64(&sections, &debug_line_reader);

		line_info->comp_unit_count++;

		line_info->comp_units =
			reallocarray(line_info->comp_units, line_info->comp_unit_count,
						 sizeof(LineInfoCompUnit));

		LineInfoCompUnit *comp_unit =
			&line_info->comp_units[line_info->comp_unit_count - 1];
		comp_unit->header = header;
		comp_unit->table = malloc(sizeof(LineInfoTable));

		if (comp_unit->table == NULL) {
			/* TODO */
		}

		*comp_unit->table = decode_line_num_prog(header, &debug_line_reader);

		// free_line_header64(&header);
	}

	return line_info;
}
