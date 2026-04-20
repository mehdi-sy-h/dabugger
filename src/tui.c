#include <locale.h>
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#include <unistd.h>

int open_tui() {
	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();

	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	unsigned rows, cols;
	getmaxyx(stdscr, rows, cols);

	refresh();

	WINDOW *src_win = newwin(rows / 2, cols / 2, 0, 0);
	WINDOW *asm_win = newwin(rows / 2, cols / 2, 0, cols / 2);
	WINDOW *output_win = newwin(rows / 4, cols / 2, rows / 2, 0);
	WINDOW *regs_win = newwin(rows / 4, cols / 2, rows / 2, cols / 2);
	WINDOW *cmd_win = newwin(rows / 4, (int)cols, 3 * rows / 4, 0);

	PANEL *src_pan = new_panel(src_win);
	PANEL *asm_pan = new_panel(asm_win);
	PANEL *output_pan = new_panel(output_win);
	PANEL *regs_pan = new_panel(regs_win);
	PANEL *cmd_pan = new_panel(cmd_win);

	box(src_win, 0, 0);
	box(asm_win, 0, 0);
	box(output_win, 0, 0);
	box(regs_win, 0, 0);
	box(cmd_win, 0, 0);

	wattron(src_win, A_BOLD);
	mvwprintw(src_win, 1, 2, "Source");
	wattroff(src_win, A_BOLD);

	wattron(asm_win, A_BOLD);
	mvwprintw(asm_win, 1, 2, "Assembly");
	wattroff(asm_win, A_BOLD);

	wattron(output_win, A_BOLD);
	mvwprintw(output_win, 1, 2, "Output");
	wattroff(output_win, A_BOLD);

	wattron(regs_win, A_BOLD);
	mvwprintw(regs_win, 1, 2, "Registers");
	wattroff(regs_win, A_BOLD);

	wattron(cmd_win, A_BOLD);
	mvwprintw(cmd_win, 1, 2, "Commands");
	wattroff(cmd_win, A_BOLD);

	update_panels();
	doupdate();

	while (getch() != 'q') {
	}

	return 0;
}

int close_tui() {
	endwin();
	return 0;
}
