#include "tui.h"
#include "dwarf.h"

#include <Zydis/Disassembler.h>
#include <Zydis/SharedTypes.h>
#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TODO: Non widechar support (via conditional macros) */
#define NCURSES_WIDECHAR 1
#include <Zydis/Zydis.h>
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#include <unistd.h>

/* TODO: Implement The Elm Architecture
 * - Put state in a single Model struct
 * - Input -> Msg (tagged union) -> update(Model, Msg) -> Model
 * - render(Model) repaints virtual screen from scratch each input loop tick
 * - Use ncurses wnoutrefresh, doupdate, etc
 * - Do not use wclear/werase before redrawing, overwrite in place instead
 * - Keep update() pure (no ncurses calls or I/O).
 * - Put side effects in render()
 * - dabugger.c stays decoupled by supplying the initial Model, and perhaps
 *   cmd/sub?
 */

/* TODO:
 * - Assembly view
 * - Breakpoints in src/asm
 * - Display registers in regs win, allow modification (use ncurses form?)
 * - Display stdout+stderr of debuggee in output win
 */

#define MAX_INPUT_BUFFER 10

#define CTRL(c) ((c) & 0x1f)

#define KEY_QUIT 'q'
#define KEY_MOTION_UP 'k'
#define KEY_MOTION_DOWN 'j'
#define KEY_MOTION_LEFT 'h'
#define KEY_MOTION_RIGHT 'l'
#define KEY_MOTION_START_OF_FILE 'g'
#define KEY_MOTION_LINE_SELECTOR 'G'
#define KEY_MOTION_PAGE_UP KEY_PPAGE
#define KEY_MOTION_PAGE_DOWN KEY_NPAGE
#define KEY_CHORD_HALF_UP CTRL('u')
#define KEY_CHORD_HALF_DOWN CTRL('d')
#define KEY_CHORD_SWITCH_WIN CTRL('w')
#define KEY_CHORD_FILE_PICKER CTRL('p')
#define KEY_CONFIRM '\n'
#define KEY_SET_BREAKPOINT ' '
#define KEY_EXECUTE_PROG 'R'

#define DEFAULT_COLOR 1
#define INACTIVE_COLOR 2
#define ACTIVE_COLOR 3

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define SECTION_COUNT 5
typedef enum {
	SECTION_SRC = 0,
	SECTION_ASM,
	SECTION_OUTPUT,
	SECTION_REGS,
	SECTION_PICKER,
} Section;

typedef struct {
	const char *title;
	const Section section;
} SectionTitleEntry;

typedef struct {
	Section section;
	WINDOW *window;
	PANEL *panel;
} TuiSection;

// typedef struct {
// TuiSection source;
// TuiSection assembly;
// TuiSection output;
// TuiSection registers;
// TuiSection picker;
//
// /*
// const char **picker_options;
// size_t picker_option_count;
// size_t selected_picker_option;
// void (*on_selected)(const char **, size_t);
// */
//
// /*
// const size_t *src_breakpoints;
// char **src_lines;
// size_t src_lines_count;
// size_t selected_src_line;
// size_t src_buffer_pos;
//
// const size_t *asm_breakpoints;
// char **asm_lines;
// size_t asm_lines_count;
// size_t selected_asm_line;
// size_t asm_buffer_pos;
// */
//
// /*uint8_t selected_reg;*/
//
// Section focused;
//
// unsigned rows;
// unsigned cols;
//
// bool is_picker_visible;
// } TuiState;
//

typedef struct {
	char buffer[MAX_INPUT_BUFFER];
	char key;
	uint8_t count;
} InputBuffer;

/*
static TuiState tui = {0};
static InputBuffer input = {0};
*/

static const char *get_section_title(Section section_enum) {
	switch (section_enum) {
	case SECTION_SRC:
		return "Source";
	case SECTION_ASM:
		return "Assembly";
	case SECTION_OUTPUT:
		return "Output";
	case SECTION_REGS:
		return "Registers";
	case SECTION_PICKER:
		return "Picker";
	}
}

static TuiSection *get_section_from_enum(Section section_enum) {
	TuiSection *section = NULL;
	switch (section_enum) {
	case SECTION_SRC:
		section = &tui.source;
		break;
	case SECTION_ASM:
		section = &tui.assembly;
		break;
	case SECTION_OUTPUT:
		section = &tui.output;
		break;
	case SECTION_REGS:
		section = &tui.registers;
		break;
	case SECTION_PICKER:
		section = &tui.picker;
		break;
	}
	return section;
}

static void set_win_border(WINDOW *win, attr_t attr, short color_pair) {
	static cchar_t ls, rs, ts, bs, tl, tr, bl, br;
	/* TODO: Idk if attr changes anything for these characters */
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

static void init_colors() {
	if (!has_colors())
		return;
	start_color();
	init_pair(DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);
	/* TODO: Different colors for inactive */
	init_pair(INACTIVE_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(ACTIVE_COLOR, COLOR_GREEN, COLOR_BLACK);
}

static void init_section(Section section_enum, float row_size_scale,
						 float col_size_scale, float row_pos_scale,
						 float col_pos_scale) {
	TuiSection *section = get_section_from_enum(section_enum);
	section->section = section_enum;
	section->window = newwin((int)(row_size_scale * (float)tui.rows),
							 (int)(col_size_scale * (float)tui.cols),
							 (int)(row_pos_scale * (float)tui.rows),
							 (int)(col_pos_scale * (float)tui.cols));
	section->panel = new_panel(section->window);

	set_win_border(section->window, A_NORMAL, INACTIVE_COLOR);

	wattron(section->window, A_BOLD);
	mvwprintw(section->window, 1, 2, "%s", get_section_title(section_enum));
	wattroff(section->window, A_BOLD);

	intrflush(section->window, FALSE);
	keypad(section->window, TRUE);
}

static void init_sections() {
	init_section(SECTION_SRC, 2.0f / 3.0f, 1.0f / 2.0f, 0, 0);
	init_section(SECTION_ASM, 2.0f / 3.0f, 1.0f / 2.0f, 0, 1.0f / 2.0f);
	init_section(SECTION_OUTPUT, 1.0f / 3.0f, 1.0f / 2.0f, 2.0f / 3.0f, 0);
	init_section(SECTION_REGS, 1.0f / 3.0f, 1.0f / 2.0f, 2.0f / 3.0f,
				 1.0f / 2.0f);
	init_section(SECTION_PICKER, 1.0f / 2.0f, 1.0f / 2.0f, 1.0f / 4.0f,
				 1.0f / 4.0f);

	hide_panel(tui.picker.panel);

	update_panels();
	doupdate();
}

static void focus_section(Section section_enum) {
	tui.focused = section_enum;

	for (Section i = 0; i < SECTION_COUNT; i++) {
		if (i == section_enum) {
			TuiSection *section = get_section_from_enum(i);
			set_win_border(section->window, A_NORMAL, ACTIVE_COLOR);
			wattron(section->window, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
			mvwprintw(section->window, 1, 2, "%s", get_section_title(i));
			wattroff(section->window, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		} else {
			TuiSection *section = get_section_from_enum(i);
			set_win_border(section->window, A_NORMAL, INACTIVE_COLOR);
			wattron(section->window, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));
			mvwprintw(section->window, 1, 2, "%s", get_section_title(i));
			wattroff(section->window, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));
		}
	}

	update_panels();
	doupdate();
}

static void clear_input_buffer(InputBuffer *input) {
	input->count = 0;
	input->key = 0;
	memset(input->buffer, 0, sizeof(input->buffer));
}

// static void update_picker_menu() {
// size_t common_prefix_len = 0;
//
// if (tui.picker_option_count > 1) {
// const char *a = tui.picker_options[0];
// const char *b = tui.picker_options[1];
//
// while (a[common_prefix_len] != '\0' &&
// a[common_prefix_len] == b[common_prefix_len])
// common_prefix_len++;
// }
//
// for (size_t i = 0; i < tui.picker_option_count; i++) {
// if (i == tui.selected_picker_option) {
// wattron(tui.picker_win, A_STANDOUT | A_BOLD | A_UNDERLINE |
// COLOR_PAIR(ACTIVE_COLOR));
// mvwprintw(tui.picker_win, (int)i + 3, 2, "%s",
// tui.picker_options[i] + common_prefix_len);
// wattroff(tui.picker_win, A_STANDOUT | A_BOLD | A_UNDERLINE |
// COLOR_PAIR(ACTIVE_COLOR));
// } else {
// mvwprintw(tui.picker_win, (int)i + 3, 2, "%s",
// tui.picker_options[i] + common_prefix_len);
// }
// }
// }
//
// static void update_src_win() {
// unsigned src_rows, src_cols;
// getmaxyx(tui.src_win, src_rows, src_cols);
//
// /* TODO: Too many magic numbers, deal with this */
// unsigned max_lines = src_rows - 3;
// unsigned max_line_length = src_cols - 8;
//
// for (unsigned i = 2; i < src_rows - 1; i++) {
// for (unsigned j = 1; j < src_cols - 1; j++)
// mvwaddch(tui.src_win, i, j, ' ');
// }
//
// for (unsigned i = 0; i < max_lines; i++) {
// const unsigned line_num = (unsigned)tui.src_buffer_pos + i;
//
// if (line_num >= tui.src_lines_count)
// break;
// char *line = tui.src_lines[line_num];
//
// /* TODO: Line wrapping instead of truncation */
// if (line_num == tui.selected_src_line) {
// wattron(tui.src_win,
// A_STANDOUT | A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
// mvwprintw(tui.src_win, (int)i + 2, 1, "%4d", line_num + 1);
// waddwstr(tui.src_win, L"\u2502 ");
// wprintw(tui.src_win, "%.*s", max_line_length, line);
// wattroff(tui.src_win,
// A_STANDOUT | A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
// } else {
// wattron(tui.src_win, A_BOLD);
// mvwprintw(tui.src_win, (int)i + 2, 1, "%4d", line_num + 1);
// waddwstr(tui.src_win, L"\u2502 ");
// wattroff(tui.src_win, A_BOLD);
// wprintw(tui.src_win, "%.*s", max_line_length, line);
// }
// }
// }

// static void on_motion_input() {
// assert(input.key == KEY_MOTION_DOWN || input.key == KEY_MOTION_LEFT ||
// input.key == KEY_MOTION_RIGHT || input.key == KEY_MOTION_UP ||
// input.key == KEY_CHORD_HALF_UP || input.key == KEY_CHORD_HALF_DOWN ||
// input.key == KEY_MOTION_LINE_SELECTOR ||
// input.key == KEY_MOTION_START_OF_FILE);
//
// if (tui.is_picker_visible) {
// if (input.key == KEY_MOTION_UP) {
// tui.selected_picker_option =
// (tui.selected_picker_option - 1 + tui.picker_option_count) %
// tui.picker_option_count;
// } else if (input.key == KEY_MOTION_DOWN) {
// tui.selected_picker_option =
// (tui.selected_picker_option + 1) % tui.picker_option_count;
// }
// update_picker_menu();
// } else if (input.count > 1 &&
// input.buffer[input.count - 2] == KEY_CHORD_SWITCH_WIN) {
// /* TODO: Fix chord delay */
// /* TODO: Dont allow wrapping */
// Win current_win = tui.focused;
// Win new_win = current_win;
// if (input.key == KEY_MOTION_UP && current_win >= WINDOW_COLS) {
// new_win = current_win - WINDOW_COLS;
// } else if (input.key == KEY_MOTION_DOWN &&
// current_win < WINDOW_COUNT - WINDOW_COLS) {
// new_win = current_win + WINDOW_COLS;
// } else if (input.key == KEY_MOTION_LEFT && current_win > 0) {
// new_win = current_win - 1;
// } else if (input.key == KEY_MOTION_RIGHT &&
// current_win < WINDOW_COUNT - 1) {
// new_win = current_win + 1;
// }
// focus_win(new_win);
// } else {
// unsigned line_distance = 0;
// bool is_move_down = true;
//
// /* We assume that src and asm have the same dimensions */
// unsigned src_rows = (unsigned)getmaxy(tui.src_win);
//
// switch (input.key) {
// case KEY_MOTION_UP:
// is_move_down = false;
// case KEY_MOTION_DOWN:
// line_distance = 1;
// break;
// case KEY_CHORD_HALF_UP:
// is_move_down = false;
// case KEY_CHORD_HALF_DOWN:
// line_distance = src_rows / 2;
// break;
// }
//
// WINDOW *win = NULL;
// size_t *selected_line = NULL;
// size_t *buffer_pos = NULL;
// size_t lines_count = 0;
//
// unsigned max_lines = src_rows - 3;
//
// if (tui.focused == WIN_SRC) {
// win = tui.src_win;
// selected_line = &tui.selected_src_line;
// buffer_pos = &tui.src_buffer_pos;
// lines_count = tui.src_lines_count;
// } else if (tui.focused == WIN_ASM) {
// win = tui.asm_win;
// selected_line = &tui.selected_asm_line;
// buffer_pos = &tui.asm_buffer_pos;
// lines_count = tui.asm_lines_count;
// }
//
// if (selected_line) {
// /* TODO: Do this properly so it cant't overflow */
// if (input.key == KEY_MOTION_LINE_SELECTOR) {
// char *num_start = NULL;
// for (int i = input.count - 2; i >= 0; i--) {
// if (isdigit(input.buffer[i]))
// num_start = input.buffer + i;
// }
// if (num_start == NULL) {
// *selected_line = lines_count - 1;
// } else {
// long line_num = strtol(num_start, NULL, 10);
// *selected_line =
// MIN((size_t)line_num - 1, tui.src_lines_count - 1);
// }
// } else if (input.key == KEY_MOTION_START_OF_FILE) {
// if (input.count > 1 &&
// input.buffer[input.count - 2] == input.key) {
// *selected_line = 0;
// } else if (input.count == 1 ||
// input.buffer[input.count - 2] != input.key) {
// return;
// }
// } else if (is_move_down) {
// *selected_line =
// MIN(lines_count - 1, *selected_line + line_distance);
// } else {
// *selected_line = (size_t)MAX(0, (int64_t)*selected_line -
// (int64_t)line_distance);
// }
//
// /* TODO: Center the buffer on C-d and C-u */
// if (*selected_line < *buffer_pos) {
// *buffer_pos = *selected_line;
// } else if (*selected_line > *buffer_pos + max_lines - 1) {
// *buffer_pos = *selected_line - max_lines + 1;
// }
//
// update_src_win();
// }
// }
//
// clear_input_buffer();
// }

// /* TODO: The following function should be removed upon refactor */
// void set_picker_options(const char **options, size_t count) {
// tui.picker_options = options;
// tui.picker_option_count = count;
// }

void update_tui(TuiMsg msg, TuiModel *model) {
	switch (msg.type) {
	case MSG_OPEN_PICKER:
		model->is_picker_open = !model->is_picker_open;
		break;
	case MSG_MOTION:
		break;
	default:
		break;
	}
}

void setup_tui() {
	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();

	curs_set(0);

	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	unsigned rows, cols;
	getmaxyx(stdscr, rows, cols);
	tui.rows = rows;
	tui.cols = cols;

	refresh();

	init_colors();
	init_sections();
}

int close_tui() {
	endwin();
	return 0;
}

void render_tui() {}

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

void start_input_loop() {
	bool quit = false;

	while (!quit) {
		char key = getch();
		if (++input.count == MAX_INPUT_BUFFER) {
			clear_input_buffer();
			input.count = 1;
		}

		input.buffer[input.count - 1] = key;
		input.key = key;

		switch (key) {
		case KEY_QUIT:
			quit = true;
			break;
		case KEY_CHORD_FILE_PICKER:
			// tui.is_picker_visible = !tui.is_picker_visible;
			// if (tui.is_picker_visible) {
			// update_picker_menu();
			// show_panel(tui.picker_pan);
			// } else {
			// hide_panel(tui.picker_pan);
			// }
			break;
		case KEY_CONFIRM:
			// if (tui.is_picker_visible) {
			// tui.is_picker_visible = false;
			// hide_panel(tui.picker_pan);

			// /* TODO: Free relevant pointers */
			// tui.src_lines_count = 0;
			// tui.selected_src_line = 0;
			// tui.src_buffer_pos = 0;

			// tui.asm_lines_count = 0;
			// tui.selected_asm_line = 0;
			// tui.src_buffer_pos = 0;

			// /* TODO: Separate UI code and logic (put some of the stuff
			// * here in a callback instead) */
			// FILE *file =
			// fopen(tui.picker_options[tui.selected_picker_option], "r");

			// /* TODO: Ensure that this code path is not executed, or that it
			// * is handled properly, if the screen is too small. */
			// char *line = NULL;
			// size_t line_len = 0;

			// unsigned line_num = 0;
			// while ((getline(&line, &line_len, file)) > 0) {
			// line[strcspn(line, "\n")] = '\0';

			// /* TODO: Use 2 or 4 spaces for tabs, will need to put this
			// * preprocessing in update_src_win */
			// for (unsigned i = 0; i < line_len; i++) {
			// if (line[i] == '\t')
			// line[i] = ' ';
			// }

			// tui.src_lines = reallocarray(tui.src_lines, line_num + 1,
			// sizeof(char *));
			// tui.src_lines[line_num] = calloc(1, line_len);
			// strcpy(tui.src_lines[line_num], line);
			// line_num++;
			// }
			// tui.src_lines_count = line_num;
			// update_src_win();

			// fclose(file);

			// /* TODO: Update asm win */
			// const LineInfoCompUnit *comp_unit = NULL;

			// for (size_t i = 0; i < tui.line_info->comp_unit_count; i++) {
			// const LineInfoCompUnit *this_comp_unit =
			// &tui.line_info->comp_units[i];
			// const char *file_name =
			// this_comp_unit->header->file_names[0].path;

			// if (strcmp(
			// file_name,
			// tui.picker_options[tui.selected_picker_option]) ==
			// 0) {
			// comp_unit = this_comp_unit;
			// break;
			// }
			// }

			// /* TODO: Add util function for getting vma from line
			// * number to dwarf.c */
			// if (comp_unit) {
			// for (size_t i = 0; i < comp_unit->table->sequences_count;
			// i++) {
			// const LineInfoSequence *sequence =
			// &comp_unit->table->sequences[i];
			// size_t start_addr = sequence->entries[0].address;
			// size_t end_addr =
			// sequence->entries[sequence->entry_count - 1]
			// .address;
			// ZyanU64 runtime_addr = start_addr; /* TODO: Probably
			// need to set to something else */
			// ZyanUSize offset = 0;
			// ZydisDisassembledInstruction instruction;
			// while (ZYAN_SUCCESS(ZydisDisassembleIntel(
			// ZYDIS_MACHINE_MODE_LONG_64, runtime_addr,
			// *elf_text_section_buffer + offset,
			// end_addr - start_addr - offset, &instruction))) {
			// }
			// }
			// }

			// /*
			// tui.on_selected(tui.picker_options, tui.selected_picker_option);
			// */
			// }
			// clear_input_buffer();
			break;
		case KEY_MOTION_UP:
		case KEY_MOTION_DOWN:
		case KEY_MOTION_LEFT:
		case KEY_MOTION_RIGHT:
		/* TODO: Set InputBuffer.key to correct type to support keypad
		characters

		case KEY_MOTION_PAGE_UP:
		case KEY_MOTION_PAGE_DOWN:
		*/
		case KEY_MOTION_START_OF_FILE:
		case KEY_MOTION_LINE_SELECTOR:
		case KEY_CHORD_HALF_UP:
		case KEY_CHORD_HALF_DOWN:
			// on_motion_input();
			break;
		default:
			break;
		}
	}
}
