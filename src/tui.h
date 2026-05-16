#ifndef DABUGGER_TUI_H
#define DABUGGER_TUI_H

#include "debug.h"

#include <stddef.h>

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

typedef enum {
	MSG_NONE,
	MSG_QUIT,
	MSG_OPEN_PICKER,
	MSG_CONFIRM,
	MSG_BUFFER_MOTION,
	MSG_GO_TO_BUFFER_LINE,
	MSG_CHANGE_SECTION
} TuiMsgType;

typedef struct {
	enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } direction;
	union {
		enum { BUFFER_HALF, BUFFER_FULL, BUFFER_START, BUFFER_END } relative;
		unsigned long absolute;
	} amount;
} TuiMotion;

typedef struct {
	TuiMsgType type;
	union {
		TuiMotion motion;
	} value;
} TuiMsg;

typedef enum {
	CMD_NONE,
} TuiCmdType;

typedef struct {
	TuiCmdType type;
	union {

	} value;
} TuiCmd;

typedef struct {
	DebugSession *session;
	bool is_picker_open;
} TuiModel;

typedef struct {
	char buffer[MAX_INPUT_BUFFER];
	char key;
	unsigned count;
} InputBuffer;

void open_tui();
void close_tui();

void update_tui(TuiMsg msg, TuiModel *model);
void view_tui(TuiModel model);

char get_input_key(InputBuffer *buffer);
void clear_input_buffer(InputBuffer *input);
void on_motion_input(TuiMsg *msg, InputBuffer *input);

#endif /* DABUGGER_TUI_H */
