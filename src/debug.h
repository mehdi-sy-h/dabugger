#ifndef DABUGGER_DEBUG_H
#define DABUGGER_DEBUG_H

#include "dwarf.h"
#include "elf.h"

typedef struct {
	const char *inferior_path;
	ProgramData *program_data;
	LineInfo *line_info;
	/* TODO: breakpoints */
} DebugSession;

typedef struct {
	size_t line_count;
	const char **lines;
} LinesBuffer;

typedef struct {
	LinesBuffer *buffer;
	size_t *addresses;
} AssemblyBuffer;

DebugSession *init_debug_session(const char *inferior_path);

LinesBuffer *get_source_buffer(DebugSession *session, size_t comp_unit_index);
AssemblyBuffer *get_assembly_buffer(DebugSession *session, size_t comp_unit_index);
LinesBuffer *get_file_picker_buffer(DebugSession *session);

void free_lines_buffer(LinesBuffer *buffer);

#endif /* DABUGGER_DEBUG_H */
