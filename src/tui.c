#include "tui.h"
#include "debug.h"
#include "dwarf.h"

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

#define MIN_TERM_ROWS 20
#define MIN_TERM_COLS 20

/* The top line, section title and bottom line occupy 3 rows per section */
#define SECTION_ROW_MARGIN 3

#define DEFAULT_COLOR 0
#define ACTIVE_COLOR 1
#define INACTIVE_COLOR 2
#define SECONDARY_COLOR 3
#define BREAKPOINT_COLOR 4

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

#define VERTICAL_LINE_CHAR L"\u2502"	 /* │ */
#define HORIZONTAL_LINE_CHAR L"\u2500"	 /* ─ */
#define TL_ROUNDED_CORNER_CHAR L"\u256D" /* ╭ */
#define TR_ROUNDED_CORNER_CHAR L"\u256E" /* ╮ */
#define BL_ROUNDED_CORNER_CHAR L"\u2570" /* ╰ */
#define BR_ROUNDED_CORNER_CHAR L"\u256F" /* ╯ */

#define BREAKPOINT_GUTTER_MARKER L"\u25CF" /* ● */
#define IP_GUTTER_MARKER L"\u2192"

static WINDOW *source_win = NULL;
static WINDOW *assembly_win = NULL;
static WINDOW *output_win = NULL;
static WINDOW *registers_win = NULL;
static WINDOW *picker_win = NULL;
static PANEL *picker_panel = NULL;

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
		init_pair(DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);
		init_pair(ACTIVE_COLOR, COLOR_GREEN, COLOR_BLACK);
		init_pair(INACTIVE_COLOR, COLOR_BLUE, COLOR_BLACK);
		init_pair(SECONDARY_COLOR, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(BREAKPOINT_COLOR, COLOR_RED, COLOR_BLACK);
	}
}

void close_tui() {
	endwin();
}

TuiCmd update_tui(TuiMsg msg, TuiModel *model) {
	TuiCmd cmd = {.type = CMD_NONE};

	TuiBuffer *tui_buffer = NULL;
	unsigned tui_buffer_rows = 0;

	/* TODO: Find a nice way to move the getmaxy() side effect out of here or
	 * move it to a cmd or whatever */
	if (model->focused_win == WIN_SOURCE) {
		tui_buffer = (TuiBuffer *)&model->buffers.source;
		tui_buffer_rows = (unsigned)getmaxy(source_win) - SECTION_ROW_MARGIN;
	} else if (model->focused_win == WIN_ASSEMBLY) {
		tui_buffer = (TuiBuffer *)&model->buffers.assembly;
		tui_buffer_rows = (unsigned)getmaxy(assembly_win) - SECTION_ROW_MARGIN;
	} else if (model->focused_win == WIN_PICKER) {
		tui_buffer = (TuiBuffer *)&model->buffers.picker;
		tui_buffer_rows = (unsigned)getmaxy(picker_win) - SECTION_ROW_MARGIN;
	}

	size_t initial_selected_line = tui_buffer->selected_line;

	switch (msg.type) {
	case MSG_BUFFER_MOTION:
		if (!tui_buffer || tui_buffer->line_count == 0)
			break;

		/* This should only be the case for MSG_GO_TO_BUFFER_LINE */
		assert(msg.value.motion.amount.relative != BUFFER_START);

		/* In case we get something like 0j */
		if (msg.value.motion.amount.relative == BUFFER_END)
			break;

		unsigned long line_distance = 0;

		if (msg.value.motion.amount.relative == BUFFER_HALF) {
			line_distance = tui_buffer_rows / 2;
		} else if (msg.value.motion.amount.relative == BUFFER_FULL) {
			line_distance = tui_buffer_rows;
		} else {
			line_distance = msg.value.motion.amount.absolute;
		}

		if (msg.value.motion.direction == DIR_DOWN) {
			size_t remaining_lines =
				tui_buffer->line_count - 1 - tui_buffer->selected_line;
			if (remaining_lines >= line_distance) {
				tui_buffer->selected_line += line_distance;
			} else {
				tui_buffer->selected_line = tui_buffer->line_count - 1;
			}
		} else if (msg.value.motion.direction == DIR_UP) {
			if (tui_buffer->selected_line >= line_distance) {
				tui_buffer->selected_line -= line_distance;
			} else {
				tui_buffer->selected_line = 0;
			}
		}

		/* Factor the following out of this case and next case */
		if (tui_buffer->selected_line < tui_buffer->line_pos) {
			tui_buffer->line_pos = tui_buffer->selected_line;
		} else if (tui_buffer->selected_line + 1 > tui_buffer_rows &&
				   tui_buffer->selected_line + 1 >
					   tui_buffer->line_pos + tui_buffer_rows) {
			tui_buffer->line_pos =
				tui_buffer->selected_line + 1 - tui_buffer_rows;
		}

		if (model->focused_win == WIN_SOURCE &&
			initial_selected_line != tui_buffer->selected_line) {
			if (model->selected_line_instructions) {
				free(model->selected_line_instructions->instructions);
				free(model->selected_line_instructions);
			}

			model->selected_line_instructions = get_instructions_for_line(
				model->session, model->selected_comp_unit_index,
				tui_buffer->selected_line + 1);
		}

		break;
	case MSG_GO_TO_BUFFER_LINE:
		if (!tui_buffer)
			break;

		if (msg.value.motion.amount.relative == BUFFER_END) {
			tui_buffer->selected_line = tui_buffer->line_count - 1;
		} else if (msg.value.motion.amount.relative == BUFFER_START) {
			tui_buffer->selected_line = 0;
		} else {
			size_t zero_indexed_line = msg.value.motion.amount.absolute - 1;
			size_t line = MIN(zero_indexed_line, tui_buffer->line_count - 1);
			tui_buffer->selected_line = line;
		}

		if (tui_buffer->selected_line < tui_buffer->line_pos) {
			tui_buffer->line_pos = tui_buffer->selected_line;
		} else if (tui_buffer->selected_line + 1 > tui_buffer_rows &&
				   tui_buffer->selected_line + 1 >
					   tui_buffer->line_pos + tui_buffer_rows) {
			tui_buffer->line_pos =
				tui_buffer->selected_line + 1 - tui_buffer_rows;
		}

		if (model->focused_win == WIN_SOURCE &&
			initial_selected_line != tui_buffer->selected_line) {
			if (model->selected_line_instructions)
				free(model->selected_line_instructions);

			model->selected_line_instructions = get_instructions_for_line(
				model->session, model->selected_comp_unit_index,
				tui_buffer->selected_line + 1);
		}

		break;
	case MSG_CHANGE_SECTION:
		TuiWindow focused = model->focused_win;
		if (focused == WIN_PICKER)
			break;

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
		cmd.type = CMD_SELECT_COMP_UNIT;
		cmd.value.comp_unit_index = model->buffers.picker.selected_line;
		/* TODO: Need to figure out how to handle selected buffers/comp units
		 * during execution */
		model->selected_comp_unit_index = model->buffers.picker.selected_line;
		break;
	case MSG_SHOW_PICKER:
		model->is_picker_open = msg.value.is_open;
		if (msg.value.is_open) {
			model->focused_win = WIN_PICKER;
		} else if (model->focused_win == WIN_PICKER) {
			model->focused_win = WIN_SOURCE;
		}
		break;
	case MSG_SET_SOURCE_BUFFER:
		model->buffers.source.line_pos = 0;
		model->buffers.source.selected_line = 0;
		model->buffers.source.buffer = msg.value.new_source_buffer;
		model->buffers.source.line_count =
			msg.value.new_source_buffer->line_count;
		break;
	case MSG_SET_ASSEMBLY_BUFFER:
		model->buffers.assembly.line_pos = 0;
		model->buffers.assembly.selected_line = 0;
		model->buffers.assembly.buffer = msg.value.new_assembly_buffer;
		model->buffers.assembly.line_count =
			msg.value.new_assembly_buffer->text_buffer->line_count;
		break;
	case MSG_TOGGLE_BREAKPOINT:
		if (model->focused_win == WIN_SOURCE) {
			cmd.type = CMD_TOGGLE_SOURCE_BREAKPOINT;
			cmd.value.source_breakpoint_info.comp_unit_index =
				model->selected_comp_unit_index;
			cmd.value.source_breakpoint_info.line_num =
				tui_buffer->selected_line + 1;
		} else if (model->focused_win == WIN_ASSEMBLY) {
			cmd.type = CMD_TOGGLE_BREAKPOINT;
			cmd.value.breakpoint_address =
				model->buffers.assembly.buffer
					->addresses[model->buffers.assembly.selected_line];
		}
		break;
	case MSG_QUIT:
	case MSG_NONE:
		break;
	}
	return cmd;
}

static void set_win_border(WINDOW *win, attr_t attr, short color_pair) {
	static cchar_t ls, rs, ts, bs, tl, tr, bl, br;
	setcchar(&ls, VERTICAL_LINE_CHAR, attr, color_pair, NULL);
	setcchar(&rs, VERTICAL_LINE_CHAR, attr, color_pair, NULL);
	setcchar(&ts, HORIZONTAL_LINE_CHAR, attr, color_pair, NULL);
	setcchar(&bs, HORIZONTAL_LINE_CHAR, attr, color_pair, NULL);
	setcchar(&tl, TL_ROUNDED_CORNER_CHAR, attr, color_pair, NULL);
	setcchar(&tr, TR_ROUNDED_CORNER_CHAR, attr, color_pair, NULL);
	setcchar(&bl, BL_ROUNDED_CORNER_CHAR, attr, color_pair, NULL);
	setcchar(&br, BR_ROUNDED_CORNER_CHAR, attr, color_pair, NULL);
	wborder_set(win, &ls, &rs, &ts, &bs, &tl, &tr, &bl, &br);
}

static void view_source_buffer(TuiModel *model) {
	unsigned rows, cols;
	getmaxyx(source_win, rows, cols);

	rows -= SECTION_ROW_MARGIN; /* Accounting for top title and bottom margin */
	cols -= 8; /* Accounting for left line number and right margin */

	TuiLinesBuffer source = model->buffers.source;
	SourceBreakpoints *src_breakpoint_data = model->session->src_breakpoints;

	for (size_t i = 0; i < rows; i++) {
		size_t zero_indexed_line_num = source.line_pos + i;
		if (zero_indexed_line_num >= source.line_count)
			break;

		/* TODO: Handle error case (and in all occurences of memory
		 * allocation in the codebase) */
		char *line = strdup(source.buffer->lines[zero_indexed_line_num]);

		/* TODO: Replace tabs with 4 spaces instead of 1 */
		for (size_t c = 0; c < strlen(line); c++) {
			if (line[c] == '\t')
				line[c] = ' ';
		}

		attr_t line_attrs = A_NORMAL;
		if (zero_indexed_line_num == source.selected_line) {
			line_attrs =
				A_STANDOUT |
				COLOR_PAIR(model->focused_win == WIN_SOURCE ? ACTIVE_COLOR
															: INACTIVE_COLOR);
		} else {
			line_attrs = A_NORMAL | COLOR_PAIR(DEFAULT_COLOR);
		}

		bool is_breakpoint = false;
		attr_t line_num_attrs = line_attrs;

		for (size_t b = 0; b < src_breakpoint_data->src_breakpoint_count; b++) {
			SourceBreakpoint src_breakpoint =
				src_breakpoint_data->src_breakpoints[b];

			if (src_breakpoint.comp_unit_index ==
					model->selected_comp_unit_index &&
				src_breakpoint.line_num == zero_indexed_line_num + 1) {
				is_breakpoint = true;
				line_num_attrs = A_BOLD | COLOR_PAIR(BREAKPOINT_COLOR);
				break;
			}
		}

		bool is_current_instruction = false;

		wattron(source_win, line_num_attrs);
		mvwprintw(source_win, (int)i + 2, 1, "%4ld", zero_indexed_line_num + 1);

		waddwstr(source_win, is_breakpoint ? BREAKPOINT_GUTTER_MARKER : L" ");
		waddwstr(source_win, is_current_instruction ? IP_GUTTER_MARKER
													: VERTICAL_LINE_CHAR);
		wattroff(source_win, line_num_attrs);

		/* TODO: Line wrapping instead of truncation */
		wattron(source_win, line_attrs);
		wprintw(source_win, " %.*s", cols, line);
		wattroff(source_win, line_attrs);

		free(line);
	}
}

static void view_assembly_buffer(TuiModel *model) {
	unsigned rows, cols;
	getmaxyx(assembly_win, rows, cols);

	rows -= SECTION_ROW_MARGIN; /* Accounting for top title and bottom margin */
	/* TODO: This cols decrement is probably inaccurate */
	cols -= 12; /* Accounting for left line number and right margin */

	TuiAssemblyBuffer assembly = model->buffers.assembly;
	Breakpoints *breakpoint_data = model->session->breakpoints;

	for (size_t i = 0; i < rows; i++) {
		size_t zero_indexed_line_num = assembly.line_pos + i;
		if (zero_indexed_line_num >= assembly.line_count)
			break;

		/* TODO: Handle error case (and in all occurences of memory
		 * allocation in the codebase) */
		char *line =
			strdup(assembly.buffer->text_buffer->lines[zero_indexed_line_num]);

		size_t address = assembly.buffer->addresses[zero_indexed_line_num];
		LineInfoEntry *line_entry = NULL;

		/* TODO: Maybe put this in debug.c or dwarf.c */
		LineInstructions *selected_instructions =
			model->selected_line_instructions;

		if (selected_instructions) {
			for (size_t entry_index = 0;
				 entry_index < selected_instructions->instruction_count;
				 entry_index++) {
				LineInfoEntry *element =
					&selected_instructions->instructions[entry_index];
				if (element->address == address) {
					line_entry = element;
					break;
				}
			}
		}

		attr_t line_attrs = A_NORMAL;
		if (zero_indexed_line_num == assembly.selected_line) {
			line_attrs =
				A_STANDOUT |
				COLOR_PAIR(model->focused_win == WIN_ASSEMBLY ? ACTIVE_COLOR
															  : INACTIVE_COLOR);
		} else if (line_entry) {
			line_attrs = (line_entry->is_stmt ? A_UNDERLINE : A_NORMAL) |
						 COLOR_PAIR(SECONDARY_COLOR);
		} else {
			line_attrs = A_NORMAL | COLOR_PAIR(DEFAULT_COLOR);
		}

		/* TODO: We assume the breakpoint array will be small, but do this
		 * better (i.e. outside and after the loop) later on. */
		bool is_breakpoint = false;
		attr_t line_num_attrs = line_attrs;

		for (size_t b = 0; b < breakpoint_data->breakpoint_count; b++) {
			if (breakpoint_data->breakpoints[b].address == address) {
				is_breakpoint = true;
				line_num_attrs = A_BOLD | COLOR_PAIR(BREAKPOINT_COLOR);
				break;
			}
		}

		bool is_current_instruction = false;

		wattron(assembly_win, line_num_attrs);
		mvwprintw(assembly_win, (int)i + 2, 1, "%4ld %16lx",
				  zero_indexed_line_num + 1, address);
		waddwstr(assembly_win, is_breakpoint ? BREAKPOINT_GUTTER_MARKER : L" ");
		waddwstr(assembly_win, is_current_instruction ? IP_GUTTER_MARKER
													  : VERTICAL_LINE_CHAR);
		wattroff(assembly_win, line_num_attrs);

		/* TODO: Line wrapping instead of truncation */
		wattron(assembly_win, line_attrs);
		wprintw(assembly_win, " %.*s", cols, line);
		wattroff(assembly_win, line_attrs);

		free(line);
	}
}

void view_tui(TuiModel *model) {
	unsigned rows, cols;
	getmaxyx(stdscr, rows, cols);

	if (rows < MIN_TERM_ROWS || cols < MIN_TERM_COLS) {
		/* TODO: Display warning if screen too small */
		update_panels();
		doupdate();
		return;
	}

	/* Source */
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

	view_source_buffer(model);

	/* Assembly */
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

	view_assembly_buffer(model);

	/* Output */
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
		TuiLinesBuffer picker_buffer = model->buffers.picker;
		for (size_t i = 0; i < picker_buffer.buffer->line_count; i++) {
			if (i == picker_buffer.selected_line) {
				wattron(picker_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
				mvwprintw(picker_win, (int)i + 2, 2, "%s",
						  picker_buffer.buffer->lines[i]);
				wattroff(picker_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
			} else {
				mvwprintw(picker_win, (int)i + 2, 2, "%s",
						  picker_buffer.buffer->lines[i]);
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

	/* FIX: Ctrl+J makes the selected line go to start of buffer */
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

			unsigned long abs_amount = msg->type == MSG_BUFFER_MOTION ? 1 : 0;

			char *num_begin = NULL;
			for (int i = (int)input->count - 2; i >= 0; i--) {
				if (!isdigit(input->buffer[i]))
					break;
				num_begin = input->buffer + i;
			}
			if (num_begin) {
				abs_amount = strtoul(num_begin, NULL, 10);
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
