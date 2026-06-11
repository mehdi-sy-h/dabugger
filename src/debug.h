#ifndef DABUGGER_DEBUG_H
#define DABUGGER_DEBUG_H

#include "dwarf.h"
#include "elf.h"

#include <sys/types.h>
#include <sys/user.h>

#define MAX_OUTPUT_SIZE 1024

typedef struct {
	size_t address;
	/* The data that was replaced by INT3 when inserting a breakpoint */
	uint8_t original_byte;
} Breakpoint;

typedef struct {
	size_t breakpoint_count;
	Breakpoint *breakpoints;
} Breakpoints;

typedef struct {
	size_t address;
	size_t comp_unit_index;
	size_t line_num;
} SourceBreakpoint;

typedef struct {
	size_t src_breakpoint_count;
	SourceBreakpoint *src_breakpoints;
} SourceBreakpoints;

typedef enum {
	DEBUG_DEAD,
	DEBUG_RUNNING,
	DEBUG_BREAKPOINT,
} DebugState;

typedef struct {
	char buffer[MAX_OUTPUT_SIZE];
	char *cursor;
} OutputBuffer;

typedef struct {
	OutputBuffer output;
	struct user_regs_struct inferior_registers;
	const char *inferior_real_path;
	char **inferior_args;
	ProgramData *program_data;
	LineInfo *line_info;
	Breakpoints *breakpoints;
	SourceBreakpoints *src_breakpoints;
	size_t proc_load_address;
	DebugState state;
	pid_t inferior_pid;
	int inferior_master_fd;
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

DebugSession *init_debug_session(const char *inferior_path,
								 char **inferior_args);

void spawn_inferior(DebugSession *session);
void continue_inferior(DebugSession *session);
void stop_inferior(DebugSession *session);

void handle_inferior_signal(DebugSession *session, int signal_child_fd);
void read_inferior_output(DebugSession *session);

LinesBuffer *get_source_buffer(DebugSession *session, size_t comp_unit_index);
AssemblyBuffer *get_assembly_buffer(DebugSession *session,
									size_t comp_unit_index);
LinesBuffer *get_file_picker_buffer(DebugSession *session);
void free_lines_buffer(LinesBuffer *buffer);

size_t get_instruction_pointer_vma(DebugSession *session);

LineInstructions *get_instructions_for_line(DebugSession *session,
											size_t comp_unit_index,
											size_t line_num);
void free_line_instructions(LineInstructions *line_instructions);

size_t *get_line_segment_for_address(DebugSession *session,
									 size_t comp_unit_index, size_t vma);

bool add_breakpoint(DebugSession *session, size_t address);
void remove_breakpoint(DebugSession *session, size_t address);
void toggle_breakpoint(DebugSession *session, size_t address);

bool set_source_breakpoint(DebugSession *session, size_t comp_unit_index,
						   size_t line_num);
void remove_source_breakpoint(DebugSession *session, size_t comp_unit_index,
							  size_t line_num);
void toggle_source_breakpoint(DebugSession *session, size_t comp_unit_index,
							  size_t line_num);

#endif /* DABUGGER_DEBUG_H */
