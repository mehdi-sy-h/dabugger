#include <locale.h>
#include <ncursesw/curses.h>
#include <unistd.h>

int init_ui() {
	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();

	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	while (true) {
		sleep(1);
	}

	endwin();

	return 0;
}
