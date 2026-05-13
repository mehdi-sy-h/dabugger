#ifndef DABUGGER_DEBUG_H
#define DABUGGER_DEBUG_H

#include "dwarf.h"
#include "elf.h"

typedef struct {
	const char *inferior_path;
	ProgramData *program_data;
	LineInfo *line_info;
} DebugSession;

DebugSession *init_debug_session(const char *inferior_path);

#endif /* DABUGGER_DEBUG_H */
