#include <assert.h>
#include <locale.h>
#include <string.h>
/* TODO: Non widechar support (conditional macros) */
#define NCURSES_WIDECHAR 1
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#include <unistd.h>

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
#define KEY_MOTION_PAGE_UP KEY_PPAGE
#define KEY_MOTION_PAGE_DOWN KEY_NPAGE
#define KEY_CHORD_HALF_UP CTRL('u')
#define KEY_CHORD_HALF_DOWN CTRL('d')
#define KEY_CHORD_SWITCH_WIN CTRL('w')
#define KEY_CHORD_FILE_PICKER CTRL('p')
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

static void on_motion_input() {
	assert(input.key == KEY_MOTION_DOWN || input.key == KEY_MOTION_LEFT ||
		   input.key == KEY_MOTION_RIGHT || input.key == KEY_MOTION_UP);

	if (input.count == 1) {
	} else if (input.buffer[input.count - 2] == KEY_CHORD_SWITCH_WIN) {
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
	}

	clear_input_buffer();
}

void set_picker_options(const char **options, size_t count) {
	tui.picker_options = options;
	tui.picker_option_count = count;
}

int open_tui() {
	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();

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
				for (size_t i = 0; i < tui.picker_option_count; i++) {
					mvwprintw(tui.picker_win, (int)i + 3, 2, "%s",
							  tui.picker_options[i]);
				}
				show_panel(tui.picker_pan);
			} else {
				hide_panel(tui.picker_pan);
			}
			break;
		case KEY_MOTION_UP:
		case KEY_MOTION_DOWN:
		case KEY_MOTION_LEFT:
		case KEY_MOTION_RIGHT:
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
