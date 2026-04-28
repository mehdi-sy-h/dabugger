#include "dwarf.h"
#include "elf.h"
#include "tui.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, [[maybe_unused]] char *argv[argc + 1]) {
	if (argc < 2) {
		/* TODO: Attach mode
		 * This will require handling PIE/ASLR. */
		printf("Attach mode not implemented yet");
		return EXIT_SUCCESS;
	}

	pid_t pid = fork();
	const char *inferior_path = argv[1];

	if (pid == 0) {
		/* Child process */
		ptrace(PTRACE_TRACEME);
		personality(ADDR_NO_RANDOMIZE);
		execl(inferior_path, *argv);
	} else if (pid > 0) {
		/* Parent process */
		int status;
		waitpid(pid, &status, __WALL);
		ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_EXITKILL);

		ProgramData program_data = parse_elf_file(argv[1]);
		LineInfo *line_info = parse_debug_line_section(program_data.sections);

		printf("Parsed %zu compilation units:\n", line_info->comp_unit_count);
		for (size_t i = 0; i < line_info->comp_unit_count; i++) {
			const LineInfoCompUnit *comp_unit = &line_info->comp_units[i];
			const char *file_name = comp_unit->header->file_names[0].path;
			printf("%s\n", file_name);
		}

		open_tui();
		close_tui();
	}

	return EXIT_SUCCESS;
}
