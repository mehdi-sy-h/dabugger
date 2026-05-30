#include "debug.h"
#include "tui.h"

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
	TuiMsg *queue;
	size_t count;
} TuiMsgQueue;

typedef enum {
	POLL_FD_STDIN = 0,
	POLL_FD_SIGNAL_CHILD,
	POLL_FD_INFERIOR_MASTER
} PollFileDescriptor;

/* Expects that msg_queue is zero initialized */
static void enqueue_msg(TuiMsgQueue *msg_queue, TuiMsg msg) {
	msg_queue->count += 1;
	msg_queue->queue =
		reallocarray(msg_queue->queue, msg_queue->count, sizeof(TuiMsg));
	msg_queue->queue[msg_queue->count - 1] = msg;
}

static TuiMsg dequeue_msg(TuiMsgQueue *msg_queue) {
	if (msg_queue->count == 0) {
		TuiMsg none_msg = {.type = MSG_NONE};
		return none_msg;
	}

	TuiMsg msg = msg_queue->queue[0];

	for (size_t i = 1; i < msg_queue->count; i++) {
		msg_queue->queue[i - 1] = msg_queue->queue[i];
	}
	msg_queue->count -= 1;
	msg_queue->queue =
		reallocarray(msg_queue->queue, msg_queue->count, sizeof(TuiMsg));

	return msg;
}

static TuiMsg on_stdin_input(TuiModel *model) {
	static InputBuffer input = {0};
	TuiMsg current_msg = {.type = MSG_NONE};

	get_input_key(&input);

	/* TODO: Allow quitting picker with esc */
	/* TODO: Pressing enter at the start of the program opens the first
	 * compilation unit (bug, but a pleasant one, figure out why its happening)
	 */
	switch (input.key) {
	case KEY_QUIT:
		current_msg.type = MSG_QUIT;
		break;
	case KEY_CHORD_FILE_PICKER:
		current_msg.type = MSG_SHOW_PICKER;
		current_msg.value.is_open = !model->is_picker_open;
		break;
	case KEY_CONFIRM:
		current_msg.type = MSG_CONFIRM;
		break;
	case KEY_TOGGLE_BREAKPOINT:
		current_msg.type = MSG_TOGGLE_BREAKPOINT;
		break;
	case KEY_RUN_PROG:
		current_msg.type = MSG_RUN_DEBUGGEE;
		break;
	case KEY_STOP_PROG:
		break;
	case KEY_STEP_INSTRUCTION:
		/* TODO */
		break;
	case KEY_STEP_OVER:
		/* TODO */
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

	return current_msg;
}

static TuiMsg on_signal_child(DebugSession *session, int signal_child_fd) {
	TuiMsg current_msg = {.type = MSG_NONE};
	/* TODO: Return relevant msg? */
	handle_inferior_signal(session, signal_child_fd);
	return current_msg;
}

static TuiMsg on_inferior_pty_update(DebugSession *session) {
	TuiMsg current_msg = {.type = MSG_OUTPUT_UPDATE};
	read_inferior_output(session);
	return current_msg;
}

static void on_run_inferior(DebugSession *session) {
	switch (session->state) {
	case DEBUG_DEAD:
		/* TODO: Clean previous child process if any */
		spawn_inferior(session);
		break;
	case DEBUG_RUNNING:
		continue_inferior(session);
		break;
	case DEBUG_BREAKPOINT:
		continue_inferior(session);
		break;
	}
}

int main(int argc, [[maybe_unused]] char *argv[argc + 1]) {
	if (argc < 2) {
		fprintf(stderr, "You must supply the executable to debug!");
		return EXIT_FAILURE;
	}

	char **inferior_args = argv + 1;
	const char *inferior_path = inferior_args[0];

	DebugSession *session = init_debug_session(inferior_path, inferior_args);

	TuiModel model = {.session = session, .is_picker_open = false};

	TuiLinesBuffer picker_buffer = {.buffer = get_file_picker_buffer(session),
									.selected_line = 0};
	picker_buffer.line_count = picker_buffer.buffer->line_count;
	model.buffers.picker = picker_buffer;

	TuiMsg current_msg = {.type = MSG_NONE};
	TuiCmd current_cmd = {.type = CMD_NONE};

	TuiMsgQueue msg_queue = {0};
	enqueue_msg(&msg_queue, current_msg);

	open_tui();

	/* We want to poll on stdin (so that ncurses' getch() returns immediately),
	 * the debuggee's output (the file descriptor given by forkpty)
	 * and signals delivered to the debuggee that we intercept. */

	/* TODO: Handle all error cases below */
	/* TODO: Use pidfd instead? */
	/*int child_fd = (int)syscall(SYS_pidfd_open, session->inferior_pid);*/
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	/* Default disposition of SIGCHLD is to ignore so we need to change it */
	sigprocmask(SIG_BLOCK, &mask, NULL);

	int signal_child_fd = signalfd(-1, &mask, 0);

	struct pollfd poll_fds[] = {
		[POLL_FD_STDIN] = {.fd = STDIN_FILENO, .events = POLLIN},
		[POLL_FD_SIGNAL_CHILD] = {.fd = signal_child_fd, .events = POLLIN},
		[POLL_FD_INFERIOR_MASTER] = {.fd = session->inferior_master_fd,
									 .events = POLLIN},
	};
	nfds_t num_poll_fds = sizeof(poll_fds) / sizeof(struct pollfd);

	while (current_msg.type != MSG_QUIT) {
		view_tui(&model);

		do {
			current_msg = dequeue_msg(&msg_queue);
		} while (msg_queue.count > 0 && current_msg.type == MSG_NONE);

		if (current_msg.type == MSG_NONE) {
			poll_fds[POLL_FD_INFERIOR_MASTER].fd = session->inferior_master_fd;
			int ready_fds = poll(poll_fds, num_poll_fds, -1);

			for (nfds_t fd = 0; fd < num_poll_fds; fd++) {
				if (poll_fds[fd].revents == 0)
					continue;

				switch ((PollFileDescriptor)fd) {
				case POLL_FD_STDIN:
					enqueue_msg(&msg_queue, on_stdin_input(&model));
					break;
				case POLL_FD_SIGNAL_CHILD:
					enqueue_msg(&msg_queue,
								on_signal_child(session, signal_child_fd));
					break;
				case POLL_FD_INFERIOR_MASTER:
					enqueue_msg(&msg_queue, on_inferior_pty_update(session));
					break;
				}
			}
		}

		/* There may need to be a cmd queue too in the future. */
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
		case CMD_TOGGLE_BREAKPOINT:
			size_t breakpoint_address = current_cmd.value.breakpoint_address;
			/* TODO: Handle failure case */
			toggle_breakpoint(session, breakpoint_address);
			break;
		case CMD_TOGGLE_SOURCE_BREAKPOINT:
			toggle_source_breakpoint(
				session,
				current_cmd.value.source_breakpoint_info.comp_unit_index,
				current_cmd.value.source_breakpoint_info.line_num);
			break;
		case CMD_RUN_DEBUGGEE:
			on_run_inferior(session);
			break;
		case CMD_NONE:
			break;
		}
	}

	close_tui();

	return EXIT_SUCCESS;
}
