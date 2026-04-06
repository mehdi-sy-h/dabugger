#ifndef _DABUGGER_DWARF_H
#define _DABUGGER_DWARF_H

#include "elf.h"
#include "leb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct LineNumStateMachine {
	size_t address;
	uint64_t op_index;
	uint32_t file;
	uint32_t line;
	uint32_t column;
	uint32_t discriminator;
	uint16_t isa;
	bool is_stmt;
	bool basic_block;
	bool end_sequence;
	bool prologue_end;
	bool epilogue_begin;
} LineNumStateMachine;

typedef uint8_t InitialLength32;

typedef struct InitialLength64 {
	uint8_t value[12];
} InitialLength64;

typedef uint64_t ULEB128Value;

/* TODO: Type for 32 bit dwarf header */

/* The line number program header for a compilation unit (64 bit format) */
typedef struct LineNumProgHeader64 {
	InitialLength64 unit_length;
	uint16_t version;
	uint8_t address_size;
	uint8_t segment_selector_size;
	size_t header_length;
	uint8_t minimum_instruction_length;
	uint8_t maximum_operations_per_instruction;
	uint8_t default_is_stmt;
	int8_t line_base;
	uint8_t line_range;
	uint8_t opcode_base;
	const uint8_t *standard_opcode_lengths;
	uint8_t directory_entry_format_count;
	ULEB128Value *directory_entry_format[2];
	ULEB128Value directories_count;
	/* TODO: directories */
	uint8_t file_name_entry_format_count;
	ULEB128Value *file_name_entry_format[2];
	ULEB128Value file_names_count;
	/* TODO: file_names */
} LineNumProgHeader64;

extern void parse_debug_line_section(DebugLineSection debug_line_section);

#endif /* _DABUGGER_DWARF_H */
