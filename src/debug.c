#include "debug.h"
#include "dwarf.h"
#include "elf.h"

#include <Zycore/Types.h>
#include <Zydis/Disassembler.h>
#include <Zydis/SharedTypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The caller is responsible for freeing the returned string */
/* TODO: Put file paths in session struct so we can free these nicely */
static char *get_file_path(LineNumProgHeader64 *line_prog_header,
						   size_t file_index) {
	/* TODO: If the path starts with /build/ then assume it was built with
	 * nix (do further checks if there is a nice way to check) and replace
	 * with /nix/store/ */
	DwarfLineNumContentEntry file_entry =
		line_prog_header->file_names[file_index];

	const char *file_name = file_entry.path;
	if (!file_name)
		return NULL;

	/* Some build systems give the file entry path in absolute form and some
	 * give a relative path (cmake, nix and plain gcc uses different forms) */
	if (file_name[0] == '/')
		return strdup(file_name);

	/* Note that the compilation directory may not necessarily exist during
	 * runtime, for example when using `nix build`. */
	const char *directory =
		line_prog_header->directories[file_entry.directory_index].path;
	if (!directory)
		return NULL;

	if (directory[0] == '/') {
		char *file_path = malloc(strlen(directory) + 1 + strlen(file_name) + 1);
		char *p = stpcpy(file_path, directory);
		*p++ = '/';
		strcpy(p, file_name);
		return file_path;
	}

	const char *compilation_dir = line_prog_header->directories[0].path;
	if (!compilation_dir)
		return NULL;

	char *file_path = malloc(strlen(compilation_dir) + 1 + strlen(directory) +
							 1 + strlen(file_name) + 1);
	char *p = stpcpy(file_path, compilation_dir);
	*p++ = '/';
	p = stpcpy(p, directory);
	*p++ = '/';
	strcpy(p, file_name);

	return file_path;
}

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
	char *file_name = get_file_path(comp_unit.header, 0);

	FILE *file = fopen(file_name, "r");
	if (!file) {
		/* TODO: Handle */
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
	free(file_name);

	return buffer;
}

/* The caller is responsible for freeing the returned buffer */
AssemblyBuffer *get_assembly_buffer(DebugSession *session,
									size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	AssemblyBuffer *asm_buffer = calloc(1, sizeof(AssemblyBuffer));
	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));
	asm_buffer->text_buffer = buffer;

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

			asm_buffer->addresses = reallocarray(
				asm_buffer->addresses, buffer->line_count, sizeof(size_t *));
			asm_buffer->addresses[line_num] = instruction_addr;

			strcpy((char *)buffer->lines[line_num], instruction.text);

			line_num += 1;
			instruction_addr += instruction.info.length;
		}
	}

	return asm_buffer;
}

/* The caller is responsible for freeing the returned buffer */
LinesBuffer *get_file_picker_buffer(DebugSession *session) {
	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));
	buffer->lines = calloc(session->line_info->comp_unit_count, sizeof(char *));
	buffer->line_count = session->line_info->comp_unit_count;

	for (size_t i = 0; i < session->line_info->comp_unit_count; i++) {
		char *file_name =
			get_file_path(session->line_info->comp_units[i].header, 0);
		buffer->lines[i] = file_name;
	}

	return buffer;
}

void free_lines_buffer(LinesBuffer *buffer) {
	for (size_t i = 0; i < buffer->line_count; i++) {
		free((void *)buffer->lines[i]);
	}
	free(buffer->lines);
	free(buffer);
}

/* Get the VMAs associated with the (1-indexed) line number.
 * The caller is responsible for freeing the returned buffer. */
LineInstructions *get_instructions_for_line(DebugSession *session,
											size_t comp_unit_index,
											size_t line_num) {
	assert(line_num != 0);

	LineInstructions *line_instructions = calloc(1, sizeof(LineInstructions));
	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];

	/* While addresses in the line info table are guaranteed to be increasing,
	 * the same is not necessarily true for line numbers, so we unfortunately
	 * have to linear search. However, we at least know that successive
	 * insertions into line_addresses will be increasing. */
	/* TODO: Consider memoizing the addresses in get_source_buffer() */
	for (size_t i = 0; i < comp_unit.table->sequences_count; i++) {
		LineInfoSequence sequence = comp_unit.table->sequences[i];
		for (size_t j = 0; j < sequence.entry_count; j++) {
			LineInfoEntry entry = sequence.entries[j];
			if (entry.line == line_num) {
				line_instructions->instruction_count += 1;
				line_instructions->instructions =
					reallocarray(line_instructions->instructions,
								 line_instructions->instruction_count,
								 sizeof(LineInfoEntry));
				line_instructions
					->instructions[line_instructions->instruction_count - 1] =
					entry;
			}
		}
	}

	return line_instructions;
}
