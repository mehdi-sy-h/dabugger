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

#define INACTIVE_COLOR 0
#define ACTIVE_COLOR 1

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

	if (has_colors()) {
		start_color();
		init_pair(INACTIVE_COLOR, COLOR_WHITE, COLOR_BLACK);
		init_pair(ACTIVE_COLOR, COLOR_GREEN, COLOR_BLACK);
	}
}

void close_tui() {
	endwin();
}

static TuiLinesBuffer *get_focused_buffer(TuiModel *model) {
	if (model->focused_win == WIN_SOURCE) {
		return model->buffers.source;
	} else if (model->focused_win == WIN_ASSEMBLY) {
		return model->buffers.assembly;
	} else if (model->focused_win == WIN_PICKER) {
		return model->buffers.picker;
	}
	return NULL;
}

void update_tui(TuiMsg msg, TuiModel *model) {
	TuiLinesBuffer *tui_buffer = get_focused_buffer(model);

	switch (msg.type) {
	case MSG_BUFFER_MOTION:
		if (!tui_buffer || tui_buffer->buffer->line_count == 0)
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
			if (remaining_lines >= msg.value.motion.amount.absolute) {
				tui_buffer->selected_line += msg.value.motion.amount.absolute;
			} else {
				tui_buffer->selected_line = tui_buffer->buffer->line_count - 1;
			}
		} else if (msg.value.motion.direction == DIR_UP) {
			if (tui_buffer->selected_line >= msg.value.motion.amount.absolute) {
				tui_buffer->selected_line -= msg.value.motion.amount.absolute;
			} else {
				tui_buffer->selected_line = 0;
			}
		}

		break;
	case MSG_GO_TO_BUFFER_LINE:
		if (!tui_buffer)
			return;

		if (msg.value.motion.amount.relative == BUFFER_END) {
			tui_buffer->selected_line = tui_buffer->buffer->line_count - 1;
		} else if (msg.value.motion.amount.relative == BUFFER_START) {
			tui_buffer->selected_line = 0;
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

static void set_win_border(WINDOW *win, attr_t attr, short color_pair) {
	static cchar_t ls, rs, ts, bs, tl, tr, bl, br;
	setcchar(&ls, L"\u2502", attr, color_pair, NULL); /* │ */
	setcchar(&rs, L"\u2502", attr, color_pair, NULL); /* │ */
	setcchar(&ts, L"\u2500", attr, color_pair, NULL); /* ─ */
	setcchar(&bs, L"\u2500", attr, color_pair, NULL); /* ─ */
	setcchar(&tl, L"\u256D", attr, color_pair, NULL); /* ╭ */
	setcchar(&tr, L"\u256E", attr, color_pair, NULL); /* ╮ */
	setcchar(&bl, L"\u2570", attr, color_pair, NULL); /* ╰ */
	setcchar(&br, L"\u256F", attr, color_pair, NULL); /* ╯ */
	wborder_set(win, &ls, &rs, &ts, &bs, &tl, &tr, &bl, &br);
}

void view_tui(TuiModel *model) {
	unsigned rows, cols;
	getmaxyx(stdscr, rows, cols);

	/* Source */
	static WINDOW *source_win = NULL;
	if (!source_win) {
		source_win = newwin(2 * rows / 3, cols / 2, 0, 0);
		new_panel(source_win);
		intrflush(source_win, FALSE);
		keypad(source_win, TRUE);
	} else {
		werase(source_win);
		mvwin(source_win, 0, 0);
		wresize(source_win, 2 * rows / 3, cols / 2);
	}
	set_win_border(source_win, A_NORMAL, 0);
	wattron(source_win, A_BOLD);
	mvwprintw(source_win, 1, 2, "%s", "Source");
	wattroff(source_win, A_BOLD);

	/* Assembly */
	static WINDOW *assembly_win = NULL;
	if (!assembly_win) {
		assembly_win = newwin(2 * rows / 3, cols / 2, 0, cols / 2);
		new_panel(assembly_win);
		intrflush(assembly_win, FALSE);
		keypad(assembly_win, TRUE);
	} else {
		werase(assembly_win);
		mvwin(assembly_win, 0, cols / 2);
		wresize(assembly_win, 2 * rows / 3, cols / 2);
	}
	set_win_border(assembly_win, A_NORMAL, 0);
	wattron(assembly_win, A_BOLD);
	mvwprintw(assembly_win, 1, 2, "%s", "Assembly");
	wattroff(assembly_win, A_BOLD);

	/* Output */
	static WINDOW *output_win = NULL;
	if (!output_win) {
		output_win = newwin(rows / 3, cols / 2, 2 * rows / 3, 0);
		new_panel(output_win);
		intrflush(output_win, FALSE);
		keypad(output_win, TRUE);
	} else {
		werase(output_win);
		mvwin(output_win, 2 * rows / 3, 0);
		wresize(output_win, rows / 3, cols / 2);
	}
	set_win_border(output_win, A_NORMAL, 0);
	wattron(output_win, A_BOLD);
	mvwprintw(output_win, 1, 2, "%s", "Output");
	wattroff(output_win, A_BOLD);

	/* Registers */
	static WINDOW *registers_win = NULL;
	if (!registers_win) {
		registers_win = newwin(rows / 3, cols / 2, 2 * rows / 3, cols / 2);
		new_panel(registers_win);
		intrflush(registers_win, FALSE);
		keypad(registers_win, TRUE);
	} else {
		werase(registers_win);
		mvwin(registers_win, 2 * rows / 3, cols / 2);
		wresize(registers_win, rows / 3, cols / 2);
	}
	set_win_border(registers_win, A_NORMAL, 0);
	wattron(registers_win, A_BOLD);
	mvwprintw(registers_win, 1, 2, "%s", "Registers");
	wattroff(registers_win, A_BOLD);

	/* Picker */
	static WINDOW *picker_win = NULL;
	static PANEL *picker_panel = NULL;
	if (!picker_win) {
		picker_win = newwin(rows / 2, cols / 2, rows / 4, cols / 4);
		picker_panel = new_panel(picker_win);
		intrflush(picker_win, FALSE);
		keypad(picker_win, TRUE);
	} else {
		werase(picker_win);
		mvwin(picker_win, rows / 4, cols / 4);
		wresize(picker_win, rows / 2, cols / 2);
	}
	set_win_border(picker_win, A_NORMAL, 0);
	wattron(picker_win, A_BOLD);
	mvwprintw(picker_win, 1, 2, "%s", "Picker");
	wattroff(picker_win, A_BOLD);

	if (model->is_picker_open) {
		TuiLinesBuffer *picker_buffer = model->buffers.picker;
		for (size_t i = 0; i < picker_buffer->buffer->line_count; i++) {
			if (i == picker_buffer->selected_line) {
				wattron(picker_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
				mvwprintw(picker_win, (int)i + 2, 2, "%s",
						  picker_buffer->buffer->lines[i]);
				wattroff(picker_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
			} else {
				mvwprintw(picker_win, (int)i + 2, 2, "%s",
						  picker_buffer->buffer->lines[i]);
			}
		}
		show_panel(picker_panel);
	} else {
		hide_panel(picker_panel);
	}

	/* Focus */
	WINDOW *focused_win = NULL;
	const char *focused_title = NULL;
	/* Switch instead of if because of exhaustive switch warning */
	switch (model->focused_win) {
	case WIN_SOURCE:
		focused_win = source_win;
		focused_title = "Source";
		break;
	case WIN_ASSEMBLY:
		focused_win = assembly_win;
		focused_title = "Assembly";
		break;
	case WIN_OUTPUT:
		focused_win = output_win;
		focused_title = "Output";
		break;
	case WIN_REGISTERS:
		focused_win = registers_win;
		focused_title = "Registers";
		break;
	case WIN_PICKER:
		focused_win = picker_win;
		focused_title = "Picker";
		break;
	}

	set_win_border(focused_win, A_BOLD, ACTIVE_COLOR);
	wattron(focused_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
	mvwprintw(focused_win, 1, 2, "%s", focused_title);
	wattroff(focused_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));

	update_panels();
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

			unsigned abs_amount = msg->type == MSG_BUFFER_MOTION ? 1 : 0;

			int *num_begin = NULL;
			for (int i = (int)input->count - 2; i >= 0; i--) {
				if (!isdigit(input->buffer[i]))
					break;
				num_begin = input->buffer + i;
			}
			if (num_begin) {
				/* FIX: Both casts here are problematic */
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
