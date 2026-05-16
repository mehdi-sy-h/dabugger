#include "tui.h"

#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Non widechar support */
#define NCURSES_WIDECHAR 1
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>

/* Section i.e. a window that is fixed on screen and not a popup like picker */
#define SECTION_ROWS 2
#define SECTION_COLS 2
#define SECTION_COUNT (SECTION_ROWS * SECTION_COLS)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

void open_tui() {
	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();

	curs_set(0);

	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
}

void close_tui() {
	endwin();
}

static TuiLinesBuffer *get_focused_buffer(TuiModel *model) {
	if (model->focused_win == WIN_SOURCE) {
		return &model->buffers.source;
	} else if (model->focused_win == WIN_ASSEMBLY) {
		return &model->buffers.assembly;
	} else if (model->focused_win == WIN_PICKER) {
		return &model->buffers.picker;
	}
	return NULL;
}

void update_tui(TuiMsg msg, TuiModel *model) {
	TuiLinesBuffer *tui_buffer = get_focused_buffer(model);

	switch (msg.type) {
	case MSG_BUFFER_MOTION:
		if (!tui_buffer)
			return;

		/* This should only be the case for MSG_GO_TO_BUFFER_LINE */
		assert(msg.value.motion.amount.relative != BUFFER_START &&
			   msg.value.motion.amount.relative != BUFFER_END);

		if (msg.value.motion.amount.relative == BUFFER_HALF) {
			/* TODO */
		} else if (msg.value.motion.amount.relative == BUFFER_FULL) {
			/* TODO */
		} else if (msg.value.motion.direction == DIR_DOWN) {
			size_t remaining_lines =
				tui_buffer->buffer->line_count - 1 - tui_buffer->selected_line;
			if (remaining_lines >= msg.value.motion.amount.absolute)
				tui_buffer->selected_line += msg.value.motion.amount.absolute;
		} else if (msg.value.motion.direction == DIR_UP) {
			if (tui_buffer->selected_line >= msg.value.motion.amount.absolute)
				tui_buffer->selected_line -= msg.value.motion.amount.absolute;
		}

		break;
	case MSG_GO_TO_BUFFER_LINE:
		if (!tui_buffer)
			return;

		if (msg.value.motion.amount.relative == BUFFER_END) {
			tui_buffer->selected_line = tui_buffer->buffer->line_count - 1;
		} else {
			size_t zero_indexed_line = msg.value.motion.amount.absolute - 1;
			size_t line =
				MIN(zero_indexed_line, tui_buffer->buffer->line_count - 1);
			tui_buffer->selected_line = line;
		}

		break;
	case MSG_CHANGE_SECTION:
		TuiWindow focused = model->focused_win;
		if (focused == WIN_PICKER)
			return;

		enum Direction direction = msg.value.motion.direction;

		if (direction == DIR_UP && focused >= SECTION_COLS) {
			model->focused_win -= SECTION_COLS;
		} else if (direction == DIR_DOWN &&
				   focused < SECTION_COUNT - SECTION_COLS) {
			model->focused_win += SECTION_COLS;
		} else if (direction == DIR_LEFT && focused > 0) {
			model->focused_win -= 1;
		} else if (direction == DIR_RIGHT && focused < SECTION_COUNT - 1) {
			model->focused_win += 1;
		}

		break;
	case MSG_CONFIRM:
		break;
	case MSG_SHOW_PICKER:
		model->is_picker_open = msg.value.is_open;
		if (msg.value.is_open) {
			model->focused_win = WIN_PICKER;
		} else if (model->focused_win == WIN_PICKER) {
			model->focused_win = WIN_SOURCE;
		}
		break;
	case MSG_QUIT:
	case MSG_NONE:
	default:
		break;
	}
}

void view_tui(TuiModel model) {
	doupdate();
}

int get_input_key(InputBuffer *input) {
	int key = getch();
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
		if (input->key == KEY_CHORD_HALF_UP) {
			msg->type = MSG_BUFFER_MOTION;
			msg->value.motion.direction = DIR_UP;
			msg->value.motion.amount.relative = BUFFER_HALF;
		} else if (input->key == KEY_CHORD_HALF_DOWN) {
			msg->type = MSG_BUFFER_MOTION;
			msg->value.motion.direction = DIR_DOWN;
			msg->value.motion.amount.relative = BUFFER_HALF;
		} else if (input->key == KEY_MOTION_START_OF_FILE) {
			if (input->count > 1 &&
				input->buffer[input->count - 2] == input->key) {
				msg->type = MSG_GO_TO_BUFFER_LINE;
				msg->value.motion.amount.relative = BUFFER_START;
			} else if (input->count == 1 ||
					   input->buffer[input->count - 2] == input->key) {
				msg->type = MSG_NONE;
			}
		} else {
			msg->type = input->key == KEY_MOTION_LINE_SELECTOR
							? MSG_GO_TO_BUFFER_LINE
							: MSG_BUFFER_MOTION;

			unsigned abs_amount = 0;

			int *num_begin = NULL;
			for (int i = (int)input->count - 2; i >= 0; i--) {
				if (!isdigit(input->buffer[i]))
					break;
				num_begin = input->buffer + i;
			}
			if (num_begin) {
				/* FIX: Both casts here are possibly problematic */
				abs_amount = (unsigned)strtoul((char *)num_begin, NULL, 10);
			}

			if (msg->type == MSG_GO_TO_BUFFER_LINE && abs_amount == 0) {
				msg->value.motion.amount.relative = BUFFER_END;
			} else {
				msg->value.motion.amount.absolute = abs_amount;
			}
		}
	}

	if (msg->type != MSG_NONE)
		clear_input_buffer(input);
}
