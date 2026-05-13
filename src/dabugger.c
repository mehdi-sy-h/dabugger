#include "debug.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

/* Must be called by the child process */
static void start_child_process(const char *inferior_path,
								char **inferior_args) {
	ptrace(PTRACE_TRACEME);
	personality(ADDR_NO_RANDOMIZE);
	execv(inferior_path, inferior_args);
}

/* Must be called by the parent process */
static void start_debug_process(int debugger_pid, const char *inferior_path) {
	int status;
	waitpid(debugger_pid, &status, __WALL);
	ptrace(PTRACE_SETOPTIONS, debugger_pid, NULL, PTRACE_O_EXITKILL);

	DebugSession *session = init_debug_session(inferior_path);

	TuiModel model = {0};
	TuiMsg msg = {.type = MSG_NONE};

	InputBuffer input = {0};

	open_tui();

	while (msg.type != MSG_QUIT) {
		view_tui(model);
		get_input_key(&input);

		switch (input.key) {
		case KEY_QUIT:
			msg.type = MSG_QUIT;
			break;
		case KEY_CHORD_FILE_PICKER:
			msg.type = MSG_OPEN_PICKER;
			break;
		case KEY_CONFIRM:
			msg.type = MSG_CONFIRM;
			break;
		case KEY_MOTION_UP:
		case KEY_MOTION_DOWN:
		case KEY_MOTION_LEFT:
		case KEY_MOTION_RIGHT:
		/* TODO: Keypad character support (for Page Up/Down) */
		case KEY_MOTION_START_OF_FILE:
		case KEY_MOTION_LINE_SELECTOR:
		case KEY_CHORD_HALF_UP:
		case KEY_CHORD_HALF_DOWN:
			msg.type = MSG_MOTION;
			break;
		default:
			msg.type = MSG_NONE;
			break;
		}

		update_tui(msg, &model);
	}

	close_tui();
}

int main(int argc, [[maybe_unused]] char *argv[argc + 1]) {
	if (argc < 2) {
		fprintf(stderr, "You must supply the executable to debug!");
		return EXIT_FAILURE;
	}

	char **inferior_args = argv + 1;
	const char *inferior_path = inferior_args[0];

	pid_t pid = fork();

	if (pid == 0) {
		start_child_process(inferior_path, inferior_args);
	} else if (pid > 0) {
		start_debug_process(pid, inferior_path);
	}

	// const char **picker_options =
	// calloc(line_info->comp_unit_count, sizeof(char *));

	// printf("Parsed %zu compilation units:\n",
	// line_info->comp_unit_count);

	// for (size_t i = 0; i < line_info->comp_unit_count; i++) {
	// const LineInfoCompUnit *comp_unit = &line_info->comp_units[i];
	// const char *file_name = comp_unit->header->file_names[0].path;
	// const char *directory = comp_unit->header->directories[0].path;
	// picker_options[i] = file_name;
	// printf("%s\n", directory);
	// printf("%s\n", file_name);
	// /*
	// char *full_path =
	// calloc(1, strlen(directory) + strlen(file_name) + 2);
	// strcpy(full_path, directory);
	// full_path[strlen(directory)] = '/';
	// strcat(full_path, file_name);
	// picker_options[i] = full_path;
	// */
	// }

	// set_picker_options(picker_options, line_info->comp_unit_count);

	// open_tui();
	// close_tui();

	return EXIT_SUCCESS;
}
