#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TODO: Non widechar support (conditional macros) */
#define NCURSES_WIDECHAR 1
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#include <unistd.h>

/* TODO: Future architecture update
 * I think this project would be a great place to implement The Elm Architecture
 * - Put state in a single Model struct (extend and modify the current TuiState)
 * - Input -> Msg (tagged union) -> update(Model, Msg) -> Model
 * - render(Model) repaints virtual screen from scratch each input loop tick
 * - Use ncurses wnoutrefresh, doupdate, etc
 * - Do not use wclear/werase before redrawing, overwrite in place instead
 * - Keep update() pure (no ncurses calls or I/O). Side effects belong in
 *   render() or in the event loop after update returns
 * - dabugger.c stays decoupled by supplying the initial Model, and perhaps
 *   subscriptions?
 */

/* TODO:
 * - Vim motions for navigation ([n](h|j|k|l), [n]G, gg, C-D, C-U, for win nav:
 * C-(h|j|k|l))
 * - File picker for src win
 * 	- get common prefix substring (i.e. src dir) and strip
 *  - or display under common subheaders
 *  - or check dwarf spec again and see whats useful
 * - Display text contents in src/asm win (either ncurses panel or viewport +
 *   scrolling buffer)
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
#define KEY_MOTION_LINE_SELECTOR 'g'
#define KEY_MOTION_END_OF_FILE 'G'
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

#define WINDOW_COUNT 4
#define WINDOW_ROWS 2
#define WINDOW_COLS 2

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef enum {
	WIN_SRC,
	WIN_ASM,
	WIN_OUTPUT,
	WIN_REGS,
} Win;

typedef struct {
	WINDOW *src_win;
	WINDOW *asm_win;
	WINDOW *output_win;
	WINDOW *regs_win;
	WINDOW *picker_win;

	PANEL *src_pan;
	PANEL *asm_pan;
	PANEL *output_pan;
	PANEL *regs_pan;
	PANEL *picker_pan;

	const char **picker_options;
	size_t picker_option_count;
	size_t selected_picker_option;
	void (*on_selected)(const char **, size_t);

	const size_t *src_breakpoints;
	char **src_lines;
	size_t src_lines_count;
	size_t selected_src_line;
	size_t src_buffer_pos;

	const size_t *asm_breakpoints;
	char **asm_lines;
	size_t asm_lines_count;
	size_t selected_asm_line;
	size_t asm_buffer_pos;

	/*uint8_t selected_reg;*/

	Win focused;

	unsigned rows;
	unsigned cols;

	bool is_picker_visible;
} TuiState;

typedef struct {
	char buffer[MAX_INPUT_BUFFER];
	char key;
	uint8_t count;
} InputBuffer;

static TuiState tui = {0};
static InputBuffer input = {0};

static void init_colors() {
	if (!has_colors())
		return;
	start_color();
	init_pair(DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);

	/* TODO: Different colors for inactive */
	init_pair(INACTIVE_COLOR, COLOR_WHITE, COLOR_BLACK);

	init_pair(ACTIVE_COLOR, COLOR_GREEN, COLOR_BLACK);
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

static void reset_window(Win win) {
	/* TODO: Implement this and use this instead of current method of clearing
	 * src window etc */
}

static void init_windows() {
	tui.src_win = newwin(2 * tui.rows / 3, tui.cols / 2, 0, 0);
	tui.asm_win = newwin(2 * tui.rows / 3, tui.cols / 2, 0, tui.cols / 2);
	tui.output_win = newwin(tui.rows / 3, tui.cols / 2, 2 * tui.rows / 3, 0);
	tui.regs_win =
		newwin(tui.rows / 3, tui.cols / 2, 2 * tui.rows / 3, tui.cols / 2);
	tui.picker_win =
		newwin(tui.rows / 2, tui.cols / 2, tui.rows / 4, tui.cols / 4);

	tui.src_pan = new_panel(tui.src_win);
	tui.asm_pan = new_panel(tui.asm_win);
	tui.output_pan = new_panel(tui.output_win);
	tui.regs_pan = new_panel(tui.regs_win);
	tui.picker_pan = new_panel(tui.picker_win);

	set_win_border(tui.src_win, A_NORMAL, 0);
	set_win_border(tui.asm_win, A_NORMAL, 0);
	set_win_border(tui.output_win, A_NORMAL, 0);
	set_win_border(tui.regs_win, A_NORMAL, 0);
	set_win_border(tui.picker_win, A_NORMAL, 0);

	wattron(tui.src_win, A_BOLD);
	mvwprintw(tui.src_win, 1, 2, "Source");
	wattroff(tui.src_win, A_BOLD);

	wattron(tui.asm_win, A_BOLD);
	mvwprintw(tui.asm_win, 1, 2, "Assembly");
	wattroff(tui.asm_win, A_BOLD);

	wattron(tui.output_win, A_BOLD);
	mvwprintw(tui.output_win, 1, 2, "Output");
	wattroff(tui.output_win, A_BOLD);

	wattron(tui.regs_win, A_BOLD);
	mvwprintw(tui.regs_win, 1, 2, "Registers");
	wattroff(tui.regs_win, A_BOLD);

	wattron(tui.picker_win, A_BOLD);
	mvwprintw(tui.picker_win, 1, 2, "Picker");
	wattroff(tui.picker_win, A_BOLD);

	intrflush(tui.src_win, FALSE);
	intrflush(tui.asm_win, FALSE);
	intrflush(tui.output_win, FALSE);
	intrflush(tui.regs_win, FALSE);
	intrflush(tui.picker_win, FALSE);

	keypad(tui.src_win, TRUE);
	keypad(tui.asm_win, TRUE);
	keypad(tui.output_win, TRUE);
	keypad(tui.regs_win, TRUE);
	keypad(tui.picker_win, TRUE);

	hide_panel(tui.picker_pan);

	update_panels();
	doupdate();
}

static void focus_win(Win win) {
	tui.focused = win;
	set_win_border(tui.src_win, A_NORMAL, INACTIVE_COLOR);
	wattron(tui.src_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));
	mvwprintw(tui.src_win, 1, 2, "Source");
	wattroff(tui.src_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));

	set_win_border(tui.asm_win, A_NORMAL, INACTIVE_COLOR);
	wattron(tui.asm_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));
	mvwprintw(tui.asm_win, 1, 2, "Assembly");
	wattroff(tui.asm_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));

	set_win_border(tui.output_win, A_NORMAL, INACTIVE_COLOR);
	wattron(tui.output_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));
	mvwprintw(tui.output_win, 1, 2, "Output");
	wattroff(tui.output_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));

	set_win_border(tui.regs_win, A_NORMAL, INACTIVE_COLOR);
	wattron(tui.regs_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));
	mvwprintw(tui.regs_win, 1, 2, "Registers");
	wattroff(tui.regs_win, A_BOLD | COLOR_PAIR(INACTIVE_COLOR));

	switch (win) {
	case WIN_SRC:
		set_win_border(tui.src_win, A_BOLD, ACTIVE_COLOR);
		wattron(tui.src_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		mvwprintw(tui.src_win, 1, 2, "Source");
		wattroff(tui.src_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		break;
	case WIN_ASM:
		set_win_border(tui.asm_win, A_BOLD, ACTIVE_COLOR);
		wattron(tui.asm_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		mvwprintw(tui.asm_win, 1, 2, "Assembly");
		wattroff(tui.asm_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		break;
	case WIN_OUTPUT:
		set_win_border(tui.output_win, A_BOLD, ACTIVE_COLOR);
		wattron(tui.output_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		mvwprintw(tui.output_win, 1, 2, "Output");
		wattroff(tui.output_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		break;
	case WIN_REGS:
		set_win_border(tui.regs_win, A_BOLD, ACTIVE_COLOR);
		wattron(tui.regs_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		mvwprintw(tui.regs_win, 1, 2, "Registers");
		wattroff(tui.regs_win, A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		break;
	}

	update_panels();
	doupdate();
}

static void clear_input_buffer() {
	input.count = 0;
	input.key = 0;
	memset(input.buffer, 0, sizeof(input.buffer));
}

static void update_picker_menu() {
	size_t common_prefix_len = 0;

	if (tui.picker_option_count > 1) {
		const char *a = tui.picker_options[0];
		const char *b = tui.picker_options[1];

		while (a[common_prefix_len] != '\0' &&
			   a[common_prefix_len] == b[common_prefix_len])
			common_prefix_len++;
	}

	for (size_t i = 0; i < tui.picker_option_count; i++) {
		if (i == tui.selected_picker_option) {
			wattron(tui.picker_win, A_STANDOUT | A_BOLD | A_UNDERLINE |
										COLOR_PAIR(ACTIVE_COLOR));
			mvwprintw(tui.picker_win, (int)i + 3, 2, "%s",
					  tui.picker_options[i] + common_prefix_len);
			wattroff(tui.picker_win, A_STANDOUT | A_BOLD | A_UNDERLINE |
										 COLOR_PAIR(ACTIVE_COLOR));
		} else {
			mvwprintw(tui.picker_win, (int)i + 3, 2, "%s",
					  tui.picker_options[i] + common_prefix_len);
		}
	}
}

static void update_src_win() {
	unsigned src_rows, src_cols;
	getmaxyx(tui.src_win, src_rows, src_cols);

	/* TODO: Too many magic numbers, deal with this */
	unsigned max_lines = src_rows - 3;
	unsigned max_line_length = src_cols - 8;

	for (unsigned i = 2; i < src_rows - 1; i++) {
		for (unsigned j = 1; j < src_cols - 1; j++)
			mvwaddch(tui.src_win, i, j, ' ');
	}

	for (unsigned i = 0; i < max_lines; i++) {
		const unsigned line_num = (unsigned)tui.src_buffer_pos + i;

		if (line_num >= tui.src_lines_count)
			break;
		char *line = tui.src_lines[line_num];

		/* TODO: Line wrapping instead of truncation */
		if (line_num == tui.selected_src_line) {
			wattron(tui.src_win,
					A_STANDOUT | A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
			mvwprintw(tui.src_win, (int)i + 2, 1, "%4d", line_num + 1);
			waddwstr(tui.src_win, L"\u2502 ");
			wprintw(tui.src_win, "%.*s", max_line_length, line);
			wattroff(tui.src_win,
					 A_STANDOUT | A_BOLD | COLOR_PAIR(ACTIVE_COLOR));
		} else {
			wattron(tui.src_win, A_BOLD);
			mvwprintw(tui.src_win, (int)i + 2, 1, "%4d", line_num + 1);
			waddwstr(tui.src_win, L"\u2502 ");
			wattroff(tui.src_win, A_BOLD);
			wprintw(tui.src_win, "%.*s", max_line_length, line);
		}
	}
}

static void on_motion_input() {
	assert(input.key == KEY_MOTION_DOWN || input.key == KEY_MOTION_LEFT ||
		   input.key == KEY_MOTION_RIGHT || input.key == KEY_MOTION_UP ||
		   input.key == KEY_CHORD_HALF_UP || input.key == KEY_CHORD_HALF_DOWN ||
		   input.key == KEY_MOTION_END_OF_FILE);

	if (tui.is_picker_visible) {
		if (input.key == KEY_MOTION_UP) {
			tui.selected_picker_option =
				(tui.selected_picker_option - 1 + tui.picker_option_count) %
				tui.picker_option_count;
		} else if (input.key == KEY_MOTION_DOWN) {
			tui.selected_picker_option =
				(tui.selected_picker_option + 1) % tui.picker_option_count;
		}
		update_picker_menu();
	} else if (input.count > 1 &&
			   input.buffer[input.count - 2] == KEY_CHORD_SWITCH_WIN) {
		/* TODO: Fix chord delay */
		/* TODO: Dont allow wrapping */
		Win current_win = tui.focused;
		Win new_win = current_win;
		if (input.key == KEY_MOTION_UP && current_win >= WINDOW_COLS) {
			new_win = current_win - WINDOW_COLS;
		} else if (input.key == KEY_MOTION_DOWN &&
				   current_win < WINDOW_COUNT - WINDOW_COLS) {
			new_win = current_win + WINDOW_COLS;
		} else if (input.key == KEY_MOTION_LEFT && current_win > 0) {
			new_win = current_win - 1;
		} else if (input.key == KEY_MOTION_RIGHT &&
				   current_win < WINDOW_COUNT - 1) {
			new_win = current_win + 1;
		}
		focus_win(new_win);
	} else {
		unsigned line_distance = 0;
		bool is_move_down = true;

		/* We assume that src and asm have the same dimensions */
		unsigned src_rows = (unsigned)getmaxy(tui.src_win);

		switch (input.key) {
		case KEY_MOTION_UP:
			is_move_down = false;
		case KEY_MOTION_DOWN:
			line_distance = 1;
			break;
		case KEY_CHORD_HALF_UP:
			is_move_down = false;
		case KEY_CHORD_HALF_DOWN:
			line_distance = src_rows / 2;
			break;
		}

		WINDOW *win = NULL;
		size_t *selected_line = NULL;
		size_t *buffer_pos = NULL;
		size_t lines_count = 0;

		unsigned max_lines = src_rows - 3;

		if (tui.focused == WIN_SRC) {
			win = tui.src_win;
			selected_line = &tui.selected_src_line;
			buffer_pos = &tui.src_buffer_pos;
			lines_count = tui.src_lines_count;
		} else if (tui.focused == WIN_ASM) {
			win = tui.asm_win;
			selected_line = &tui.selected_asm_line;
			buffer_pos = &tui.asm_buffer_pos;
			lines_count = tui.asm_lines_count;
		}

		if (selected_line) {
			/* TODO: Do this properly so it cant't overflow */
			if (input.key == KEY_MOTION_END_OF_FILE) {
				*selected_line = lines_count - 1;
			} else if (is_move_down) {
				*selected_line =
					MIN(lines_count - 1, *selected_line + line_distance);
			} else {
				*selected_line = (size_t)MAX(0, (int64_t)*selected_line -
													(int64_t)line_distance);
			}

			/* TODO: Center the buffer on C-d and C-u */
			if (*selected_line < *buffer_pos) {
				*buffer_pos = *selected_line;
			} else if (*selected_line > *buffer_pos + max_lines - 1) {
				*buffer_pos = *selected_line - max_lines + 1;
			}

			update_src_win();
		}
	}

	clear_input_buffer();
}

void set_picker_options(const char **options, size_t count) {
	tui.picker_options = options;
	tui.picker_option_count = count;
}

void set_picker_selected_callback(void (*on_selected)(const char **, size_t)) {
	tui.on_selected = on_selected;
}

int open_tui() {
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
	init_windows();

	focus_win(WIN_SRC);

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
			tui.is_picker_visible = !tui.is_picker_visible;
			if (tui.is_picker_visible) {
				update_picker_menu();
				show_panel(tui.picker_pan);
			} else {
				hide_panel(tui.picker_pan);
			}
			break;
		case KEY_CONFIRM:
			if (tui.is_picker_visible) {
				tui.is_picker_visible = false;
				hide_panel(tui.picker_pan);

				/* TODO: Free relevant pointers */
				tui.src_lines_count = 0;
				tui.selected_src_line = 0;
				tui.src_buffer_pos = 0;

				tui.asm_lines_count = 0;
				tui.selected_asm_line = 0;
				tui.src_buffer_pos = 0;

				/* TODO: Separate UI code and logic (put some of the stuff
				 * here in a callback instead) */
				FILE *file =
					fopen(tui.picker_options[tui.selected_picker_option], "r");

				unsigned src_rows, src_cols;
				getmaxyx(tui.src_win, src_rows, src_cols);
				/* TODO: Ensure that this code path is not executed, or that it
				 * is handled properly, if the screen is too small. */
				/*char *line = calloc(1, src_cols);*/
				unsigned row = 2;

				/*
				for (unsigned i = row; i < src_rows - 1; i++) {
					for (unsigned j = 1; j < src_cols - 1; j++)
						mvwaddch(tui.src_win, i, j, ' ');
				}
				*/

				src_cols -= 7;
				src_rows -= 1;

				char *line = NULL;
				size_t line_len = 0;

				unsigned line_num = 0;
				while ((getline(&line, &line_len, file)) > 0) {
					line[strcspn(line, "\n")] = '\0';

					/* TODO: Use 2 or 4 spaces for tabs, will need to put this
					 * preprocessing in update_src_win */
					for (unsigned i = 0; i < src_cols; i++) {
						if (line[i] == '\t')
							line[i] = ' ';
					}

					tui.src_lines = reallocarray(tui.src_lines, line_num + 1,
												 sizeof(char *));
					tui.src_lines[line_num] = calloc(1, line_len);
					strcpy(tui.src_lines[line_num], line);
					line_num++;
				}
				tui.src_lines_count = line_num;
				update_src_win();

				fclose(file);
				// unsigned line_num = 0;
				// while (/*row < src_rows &&*/ fgets(line, (int)src_cols,
				// file)) { line[strcspn(line, "\n")] = '\0';

				// /* TODO: Use 4 spaces for tabs, will require using fgets
				// * differently */
				// for (unsigned i = 0; i < src_cols; i++) {
				// if (line[i] == '\t')
				// line[i] = ' ';
				// }

				// tui.src_lines = reallocarray(tui.src_lines, line_num + 1,
				// sizeof(char *));
				// tui.src_lines[line_num] = calloc(1, src_cols);
				// strcpy(tui.src_lines[line_num], line);
				// memset(line, 0, src_cols);

				// /*row += 1;*/
				// line_num += 1;
				// }
				/*
				tui.on_selected(tui.picker_options, tui.selected_picker_option);
				*/
			}
			clear_input_buffer();
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
		case KEY_MOTION_END_OF_FILE:
		case KEY_CHORD_HALF_UP:
		case KEY_CHORD_HALF_DOWN:
			on_motion_input();
			break;
		default:
			break;
		}

		update_panels();
		doupdate();
	}

	return 0;
}

int close_tui() {
	endwin();
	return 0;
}
