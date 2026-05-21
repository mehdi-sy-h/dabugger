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
	/* TODO: Associate lines with addresses */
} LinesBuffer;

typedef struct {
	LinesBuffer *text_buffer;
	size_t *addresses;
	/* TODO: Associate addresses with line segments */
} AssemblyBuffer;

/* Entry addresses are in increasing order  */
typedef struct {
	size_t instruction_count;
	LineInfoEntry *instructions;
} LineInstructions;

DebugSession *init_debug_session(const char *inferior_path);

LinesBuffer *get_source_buffer(DebugSession *session, size_t comp_unit_index);
AssemblyBuffer *get_assembly_buffer(DebugSession *session,
									size_t comp_unit_index);
LinesBuffer *get_file_picker_buffer(DebugSession *session);
void free_lines_buffer(LinesBuffer *buffer);

LineInstructions *get_instructions_for_line(DebugSession *session,
											size_t comp_unit_index,
											size_t line_num);
size_t *get_line_segment_for_address(DebugSession *session,
									 size_t comp_unit_index, size_t vma);

#endif /* DABUGGER_DEBUG_H */
