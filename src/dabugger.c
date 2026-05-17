#include "debug.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

static void start_child_process(const char *inferior_path,
								char **inferior_args) {
	ptrace(PTRACE_TRACEME);
	personality(ADDR_NO_RANDOMIZE);
	execv(inferior_path, inferior_args);
}

static void start_debug_process(int debugger_pid, const char *inferior_path) {
	int status;
	waitpid(debugger_pid, &status, __WALL);
	ptrace(PTRACE_SETOPTIONS, debugger_pid, NULL, PTRACE_O_EXITKILL);

	DebugSession *session = init_debug_session(inferior_path);

	TuiModel model = {.session = session, .is_picker_open = false};
	TuiMsg msg = {.type = MSG_NONE};

	TuiLinesBuffer picker_buffer = {.buffer = get_file_picker_buffer(session),
									.selected_line = 0};
	model.buffers.picker = &picker_buffer;

	InputBuffer input = {0};

	open_tui();

	while (msg.type != MSG_QUIT) {
		view_tui(&model);

		get_input_key(&input);
		/* TODO: Track other events (SIGWINCH, other signals, etc) */

		switch (input.key) {
		case KEY_QUIT:
			msg.type = MSG_QUIT;
			break;
		case KEY_CHORD_FILE_PICKER:
			msg.type = MSG_SHOW_PICKER;
			msg.value.is_open = !model.is_picker_open;
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
			on_motion_input(&msg, &input);
			break;
		default:
			msg.type = MSG_NONE;
			break;
		}

		/* TODO: Implement cmds */

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

	return EXIT_SUCCESS;
}
