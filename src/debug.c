#include "debug.h"
#include "dwarf.h"

#include <Zycore/Types.h>
#include <Zydis/Disassembler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Call in the parent process. */
DebugSession *init_debug_session(const char *inferior_path) {
	DebugSession *debug_session = malloc(sizeof(DebugSession));
	if (!debug_session) {
		/* TODO */
	}

	debug_session->inferior_path = inferior_path;
	debug_session->program_data = malloc(sizeof(ProgramData));
	debug_session->line_info = malloc(sizeof(LineInfo));

	if (!debug_session->program_data || !debug_session->line_info) {
		/* TODO */
	}

	*debug_session->program_data = parse_elf_file(inferior_path);
	debug_session->line_info =
		parse_debug_line_section(debug_session->program_data->sections);

	return debug_session;
}

LinesBuffer *get_source_buffer(DebugSession *session, size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	LinesBuffer *buffer = malloc(sizeof(LinesBuffer));
	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];
	const char *file_name = comp_unit.header->file_names[0].path;

	FILE *file = fopen(file_name, "r");

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

LinesBuffer *get_assembly_buffer(DebugSession *session,
								 size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	LinesBuffer *buffer = malloc(sizeof(LinesBuffer));
	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];

	for (size_t i = 0; i < comp_unit.table->sequences_count; i++) {
		const LineInfoSequence *sequence = &comp_unit.table->sequences[i];
		size_t start_addr = sequence->entries[0].address;
		size_t end_addr = sequence->entries[sequence->entry_count - 1].address;

		/* TODO: Probably need to set to something else */
		ZyanU64 runtime_addr = start_addr;
		ZyanUSize offset = 0;
		ZydisDisassembledInstruction instruction;
		unsigned line_num = 0;

		while (ZYAN_SUCCESS(ZydisDisassembleIntel(
			ZYDIS_MACHINE_MODE_LONG_64, runtime_addr,
			session->program_data->sections.text.data + offset,
			end_addr - start_addr - offset, &instruction))) {
			buffer->lines =
				reallocarray(buffer->lines, line_num + 1, sizeof(char *));
			buffer->lines[line_num] = calloc(1, strlen(instruction.text));
			buffer->line_count =
				line_num + 1; /* Safer to update this every iteration */

			strcpy((char *)buffer->lines[line_num], instruction.text);

			line_num += 1;
			offset += instruction.info.length;
			runtime_addr += instruction.info.length;
		}
	}

	return buffer;
}
