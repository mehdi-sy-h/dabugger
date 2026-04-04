#include "elf.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, [[maybe_unused]] char *argv[argc + 1]) {
	if (argc < 2) {
		// TODO: Attach mode
		printf("Attach mode not implemented yet");
		return EXIT_SUCCESS;
	}

	pid_t pid = fork();
	const char *inferior_path = argv[1];

	if (pid == 0) {
		// Child process
		// execv(inferior_path, argv);
		// ptrace(PTRACE_TRACEME);
	} else if (pid > 0) {
		// Parent process
		parse_elf64_file(argv[1]);
	}

	return EXIT_SUCCESS;
}
