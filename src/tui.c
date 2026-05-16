#include "tui.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Non widechar support */
#define NCURSES_WIDECHAR 1
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>

void open_tui() {}

void close_tui() {}

void update_tui(TuiMsg msg, TuiModel *model) {}

void view_tui(TuiModel model) {}

char get_input_key(InputBuffer *input) {
	char key = getch();
	if (++input->count == MAX_INPUT_BUFFER) {
		clear_input_buffer(input);
		input->count = 1;
	}

	input->buffer[input->count - 1] = key;
	input->key = key;
	return key;
}

void clear_input_buffer(InputBuffer *input) {
	input->count = 0;
	input->key = 0;
	memset(input->buffer, 0, sizeof(input->buffer));
}

void on_motion_input(TuiMsg *msg, InputBuffer *input) {
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
