#ifndef DABUGGER_TUI_H
#define DABUGGER_TUI_H

#include <stddef.h>

int open_tui();
int close_tui();
void set_picker_options(const char **options, size_t count);

#endif /* DABUGGER_TUI_H */
