#include "dwarf.h"
#include "elf.h"

#include <stdbool.h>
#include <stddef.h>

void parse_debug_line_section(DebugLineSection debug_line_section) {
	// See DWARF 5 Specification Table 6.4
	LineNumStateMachine state_machine = {
		.address = 0,
		.op_index = 0,
		.file = 1,
		.line = 1,
		.column = 0,
		/* TODO(obtain from program header): .is_stmt = ... */
		.basic_block = false,
		.end_sequence = false,
		.prologue_end = false,
		.epilogue_begin = false,
		.isa = 0,
		.discriminator = 0,
	};
}
