#include "debug.h"
#include "tui.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

static void clear_input_buffer(InputBuffer *input) {
	input->count = 0;
	input->key = 0;
	memset(input->buffer, 0, sizeof(input->buffer));
}

static void on_motion_input(TuiMsg *msg, InputBuffer *input) {
	if (input->key == KEY_MOTION_UP) {
		msg->value.motion.direction = DIR_UP;
	} else if (input->key == KEY_MOTION_DOWN) {
		msg->value.motion.direction = DIR_DOWN;
	} else if (input->key == KEY_MOTION_LEFT) {
		msg->value.motion.direction = DIR_LEFT;
	} else if (input->key == KEY_MOTION_RIGHT) {
		msg->value.motion.direction = DIR_RIGHT;
	}

	if (input->count > 1 &&
		input->buffer[input->count - 2] == KEY_CHORD_SWITCH_WIN) {

		msg->type = MSG_CHANGE_SECTION;
		msg->value.motion.amount.absolute = 1;
	} else {
		msg->type = MSG_BUFFER_MOTION;

		if (input->key == KEY_CHORD_HALF_UP) {
			msg->value.motion.direction = DIR_UP;
			msg->value.motion.amount.relative = BUFFER_HALF;
		} else if (input->key == KEY_CHORD_HALF_DOWN) {
			msg->value.motion.direction = DIR_DOWN;
			msg->value.motion.amount.relative = BUFFER_HALF;
		} else if (input->key == KEY_MOTION_START_OF_FILE) {
			if (input->count > 1 &&
				input->buffer[input->count - 2] == input->key) {
				msg->value.motion.amount.relative = BUFFER_START;
			} else if (input->count == 1 ||
					   input->buffer[input->count - 2] == input->key) {
				/* Return early so we don't clear the input buffer */
				return;
			}
		} else {
			if (input->key == KEY_MOTION_LINE_SELECTOR) {
				msg->type = MSG_GO_TO_BUFFER_LINE;
			}

			unsigned long abs_amount = 0;

			char *num_begin = NULL;
			for (int i = (int)input->count - 2; i >= 0; i--) {
				if (!isdigit(input->buffer[i]))
					break;
				num_begin = input->buffer + i;
			}
			if (num_begin) {
				abs_amount = strtoul(num_begin, NULL, 10);
			}

			msg->value.motion.amount.absolute = abs_amount;
		}
	}

	clear_input_buffer(input);
}

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
		/* TODO: Function that gets the next msg by blocking and polling for
		 * relevant events (input, signals, etc) */
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
			on_motion_input(&msg, &input);
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
