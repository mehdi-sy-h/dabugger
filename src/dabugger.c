#include "debug.h"
#include "tui.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
	TuiMsg *queue;
	size_t count;
} TuiMsgQueue;

/* Expects that msg_queue is zero initialized */
static void enqueue_msg(TuiMsgQueue *msg_queue, TuiMsg msg) {
	msg_queue->count += 1;
	msg_queue->queue =
		reallocarray(msg_queue->queue, msg_queue->count, sizeof(TuiMsg));
	msg_queue->queue[msg_queue->count - 1] = msg;
}

/* Expects that msg_queue is non empty */
static TuiMsg dequeue_msg(TuiMsgQueue *msg_queue) {
	assert(msg_queue->count > 0);

	TuiMsg msg = msg_queue->queue[0];

	for (size_t i = 1; i < msg_queue->count; i++) {
		msg_queue->queue[i - 1] = msg_queue->queue[i];
	}
	msg_queue->count -= 1;
	msg_queue->queue =
		reallocarray(msg_queue->queue, msg_queue->count, sizeof(TuiMsg));

	return msg;
}

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

	TuiLinesBuffer picker_buffer = {.buffer = get_file_picker_buffer(session),
									.selected_line = 0};
	picker_buffer.line_count = picker_buffer.buffer->line_count;
	model.buffers.picker = picker_buffer;

	TuiMsg current_msg = {.type = MSG_NONE};
	TuiCmd current_cmd = {.type = CMD_NONE};

	TuiMsgQueue msg_queue = {0};
	enqueue_msg(&msg_queue, current_msg);

	InputBuffer input = {0};

	open_tui();

	while (current_msg.type != MSG_QUIT) {
		view_tui(&model);

		/* TODO: Track other events (SIGWINCH, other signals, etc) */
		if (msg_queue.count > 0) {
			current_msg = dequeue_msg(&msg_queue);
		} else {
			TuiMsg none_msg = {.type = MSG_NONE};
			current_msg = none_msg;
		}

		if (current_msg.type == MSG_NONE) {
			get_input_key(&input);

			switch (input.key) {
			case KEY_QUIT:
				current_msg.type = MSG_QUIT;
				break;
			case KEY_CHORD_FILE_PICKER:
				current_msg.type = MSG_SHOW_PICKER;
				current_msg.value.is_open = !model.is_picker_open;
				break;
			case KEY_CONFIRM:
				current_msg.type = MSG_CONFIRM;
				break;
			case KEY_MOTION_UP:
			case KEY_MOTION_DOWN:
			case KEY_MOTION_LEFT:
			case KEY_MOTION_RIGHT:
			case KEY_MOTION_START_OF_FILE:
			case KEY_MOTION_LINE_SELECTOR:
			case KEY_CHORD_HALF_UP:
			case KEY_CHORD_HALF_DOWN:
				on_motion_input(&current_msg, &input);
				break;
			}
		}

		current_cmd = update_tui(current_msg, &model);

		switch (current_cmd.type) {
		case CMD_SELECT_COMP_UNIT:
			size_t index = current_cmd.value.comp_unit_index;

			TuiMsg set_source_msg = {.type = MSG_SET_SOURCE_BUFFER,
									 .value.new_source_buffer =
										 get_source_buffer(session, index)};
			enqueue_msg(&msg_queue, set_source_msg);

			TuiMsg set_assembly_msg = {.type = MSG_SET_ASSEMBLY_BUFFER,
									   .value.new_assembly_buffer =
										   get_assembly_buffer(session, index)};
			enqueue_msg(&msg_queue, set_assembly_msg);

			TuiMsg hide_picker_msg = {.type = MSG_SHOW_PICKER,
									  .value.is_open = false};
			enqueue_msg(&msg_queue, hide_picker_msg);

			break;
		case CMD_NONE:
			break;
		}
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
