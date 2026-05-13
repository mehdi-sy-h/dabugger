#include "dwarf.h"
#include "elf.h"

#include <stdlib.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <unistd.h>

typedef struct {
	const char *inferior_path;
	ProgramData *program_data;
	LineInfo *line_info;
	/* TODO: breakpoints */
} DebugSession;

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
