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
#define KEY_TOGGLE_BREAKPOINT ' '
#define KEY_RUN_PROG 'R'
#define KEY_STOP_PROG 'F'
#define KEY_STEP_INSTRUCTION 's'
#define KEY_STEP_OVER 'S'

typedef enum {
    MSG_NONE = 0,
    MSG_QUIT,
    MSG_SHOW_PICKER,
    MSG_CONFIRM,
    MSG_BUFFER_MOTION,
    MSG_GO_TO_BUFFER_LINE,
    MSG_CHANGE_SECTION,
    MSG_SET_SOURCE_BUFFER,
    MSG_SET_ASSEMBLY_BUFFER,
    MSG_TOGGLE_BREAKPOINT,
    MSG_OUTPUT_UPDATE,
    MSG_RUN_DEBUGGEE,
    MSG_STEP_INSTRUCTION,
    MSG_STEP_OVER,
} TuiMsgType;

typedef enum {
    CMD_NONE = 0,
    CMD_SELECT_COMP_UNIT,
    CMD_TOGGLE_BREAKPOINT,
    CMD_TOGGLE_SOURCE_BREAKPOINT,
    CMD_RUN_DEBUGGEE,
} TuiCmdType;

typedef enum {
    WIN_SOURCE = 0,
    WIN_ASSEMBLY,
    WIN_OUTPUT,
    WIN_REGISTERS,
    WIN_PICKER,
} TuiWindow;

typedef struct {
    enum Direction { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } direction;
    union {
        enum : long {
            /* Used with MSG_BUFFER_MOTION  */
            BUFFER_HALF = -3, /* Negative values so its possible to distinguish
                                                     from absolute */
            BUFFER_FULL = -2,

            /* Used with MSG_GO_TO_BUFFER_LINE  */
            BUFFER_START = -1,
            BUFFER_END = 0, /* Needs to be 0 to coincide with absolute = 0 */
        } relative;
        long absolute;
    } amount;
} TuiMotion;

typedef struct {
    TuiMsgType type;
    union {
        TuiMotion motion;
        bool is_open;
        LinesBuffer *new_source_buffer;
        AssemblyBuffer *new_assembly_buffer;
    } value;
} TuiMsg;

typedef struct {
    TuiCmdType type;
    union {
        size_t comp_unit_index;
        size_t breakpoint_address;
        struct {
            size_t comp_unit_index;
            size_t line_num;
        } source_breakpoint_info;
    } value;
} TuiCmd;

typedef struct {
    size_t selected_line;
    size_t line_count;
    size_t line_pos;
    void *buffer;
} TuiBuffer;

typedef struct {
    size_t selected_line;
    size_t line_count;
    size_t line_pos;
    LinesBuffer *buffer;
} TuiLinesBuffer;

typedef struct {
    size_t selected_line;
    size_t line_count;
    size_t line_pos;
    AssemblyBuffer *buffer;
} TuiAssemblyBuffer;

typedef struct {
    struct {
        TuiLinesBuffer source;
        TuiAssemblyBuffer assembly;
        TuiLinesBuffer picker;
    } buffers;
    DebugSession *session;
    LineInstructions *selected_line_instructions;
    size_t selected_comp_unit_index;
    TuiWindow focused_win;
    bool is_picker_open;
} TuiModel;

typedef struct {
    char buffer[MAX_INPUT_BUFFER];
    char key;
    uint8_t count;
} InputBuffer;

void open_tui();
void close_tui();

TuiCmd update_tui(TuiMsg msg, TuiModel *model);
void view_tui(TuiModel *model);

char get_input_key(InputBuffer *buffer);
void clear_input_buffer(InputBuffer *input);
void on_motion_input(TuiMsg *msg, InputBuffer *input);

#endif /* DABUGGER_TUI_H */
