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

		const char **picker_options =
			calloc(line_info->comp_unit_count, sizeof(char *));

		printf("Parsed %zu compilation units:\n", line_info->comp_unit_count);

		for (size_t i = 0; i < line_info->comp_unit_count; i++) {
			const LineInfoCompUnit *comp_unit = &line_info->comp_units[i];
			const char *file_name = comp_unit->header->file_names[0].path;
			picker_options[i] = file_name;
			printf("%s\n", file_name);
		}

		if (line_info->comp_unit_count > 1) {
			size_t common_prefix_len = 0;
			const char *a = picker_options[0];
			const char *b = picker_options[1];

			while (a[common_prefix_len] != '\0' &&
				   a[common_prefix_len] == b[common_prefix_len])
				common_prefix_len++;

			for (size_t i = 0; i < line_info->comp_unit_count; i++) {
				picker_options[i] += common_prefix_len;
			}
		}

		set_picker_options(picker_options, line_info->comp_unit_count);

		open_tui();
		close_tui();
	}

	return EXIT_SUCCESS;
}
