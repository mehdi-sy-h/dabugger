#include "debug.h"
#include "dwarf.h"
#include "elf.h"

#include <Zycore/Types.h>
#include <Zydis/Disassembler.h>
#include <Zydis/SharedTypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Call in the parent process. */
DebugSession *init_debug_session(const char *inferior_path) {
	DebugSession *debug_session = calloc(1, sizeof(DebugSession));
	if (!debug_session) {
		/* TODO */
	}

	debug_session->inferior_path = inferior_path;
	debug_session->program_data = calloc(1, sizeof(ProgramData));
	debug_session->line_info = calloc(1, sizeof(LineInfo));

	if (!debug_session->program_data || !debug_session->line_info) {
		/* TODO */
	}

	*debug_session->program_data = parse_elf_file(inferior_path);
	debug_session->line_info =
		parse_debug_line_section(debug_session->program_data->sections);

	return debug_session;
}

/* The caller is responsible for freeing the returned buffer */
LinesBuffer *get_source_buffer(DebugSession *session, size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));

	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];
	const char *file_name = comp_unit.header->file_names[0].path;

	FILE *file = fopen(file_name, "r");
	if (!file) {
		/* TODO: Handle */
		/* TODO: Also figure out why sometimes file_name includes the directory
		 * and sometimes doesn't. */
	}

	char *line = NULL;
	size_t line_len = 0;

	for (unsigned line_num = 0; getline(&line, &line_len, file) > 0;
		 line_num++) {
		line[strcspn(line, "\n")] = '\0';

		buffer->lines =
			reallocarray(buffer->lines, line_num + 1, sizeof(char *));
		buffer->lines[line_num] = calloc(1, line_len);
		buffer->line_count = line_num + 1;

		strcpy((char *)buffer->lines[line_num], line);
	}

	fclose(file);

	return buffer;
}

/* The caller is responsible for freeing the returned buffer */
LinesBuffer *get_assembly_buffer(DebugSession *session,
								 size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));

	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];

	SectionBuffer text_section = session->program_data->sections.text;

	for (size_t i = 0; i < comp_unit.table->sequences_count; i++) {
		const LineInfoSequence *sequence = &comp_unit.table->sequences[i];
		size_t start_addr = sequence->entries[0].address;
		size_t end_addr = sequence->entries[sequence->entry_count - 1].address;

		ZydisDisassembledInstruction instruction;
		ZyanU64 instruction_addr = start_addr;
		unsigned line_num = 0;

		while (ZYAN_SUCCESS(ZydisDisassembleIntel(
			ZYDIS_MACHINE_MODE_LONG_64, instruction_addr,
			text_section.data + (instruction_addr - text_section.address),
			end_addr - instruction_addr, &instruction))) {
			buffer->lines =
				reallocarray(buffer->lines, line_num + 1, sizeof(char *));
			buffer->lines[line_num] = calloc(1, strlen(instruction.text) + 1);
			buffer->line_count =
				line_num + 1; /* Safer to update this every iteration */

			strcpy((char *)buffer->lines[line_num], instruction.text);

			line_num += 1;
			instruction_addr += instruction.info.length;
		}
	}

	return buffer;
}

/* The caller is responsible for freeing the returned buffer */
LinesBuffer *get_file_picker_buffer(DebugSession *session) {
	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));
	/* TODO */
	return buffer;
}

void free_lines_buffer(LinesBuffer *buffer) {
	for (size_t i = 0; i < buffer->line_count; i++) {
		free((void *)buffer->lines[i]);
	}
	free(buffer->lines);
	free(buffer);
}
