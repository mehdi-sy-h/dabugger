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

/* TODO: Fix these defs */
typedef uint8_t InitialLength32;

typedef struct InitialLength64 {
	uint8_t value[12];
} InitialLength64;

/* See Dwarf 5 Specification 7.22 */
typedef enum DwarfLineNumContentType {
	DW_LNCT_path = 0x1,
	DW_LNCT_directory_index = 0x2,
	DW_LNCT_timestamp = 0x3,
	DW_LNCT_size = 0x4,
	DW_LNCT_MD5 = 0x5,
	DW_LNCT_lo_user = 0x2000,
	DW_LNCT_hi_user = 0x3fff,
} DwarfLineNumContentType;

/* See Dwarf 5 Specification 7.5.5 */
typedef enum DwarfFormCode {
	DW_FORM_data1      = 0x0b,
	DW_FORM_data2      = 0x05,
	DW_FORM_data4      = 0x06,
	DW_FORM_data8      = 0x07,
	DW_FORM_data16     = 0x1e,
	DW_FORM_sdata      = 0x0d,
	DW_FORM_udata      = 0x0f,

	DW_FORM_string     = 0x08,
	DW_FORM_strp       = 0x0e,
	DW_FORM_line_strp  = 0x1f,
	DW_FORM_strp_sup   = 0x1d,

	DW_FORM_strx       = 0x1a,
	DW_FORM_strx1      = 0x25,
	DW_FORM_strx2      = 0x26,
	DW_FORM_strx3      = 0x27,
	DW_FORM_strx4      = 0x28,

	DW_FORM_block      = 0x09,
	DW_FORM_block1     = 0x0a,
	DW_FORM_block2     = 0x03,
	DW_FORM_block4     = 0x04,

	DW_FORM_flag       = 0x0c,

	DW_FORM_sec_offset = 0x17,
} DwarfFormCode;

/* TODO: Enums to specify what the possible content type and form code combinations are,
 * maybe tagged union. */
typedef struct DwarfFormatDesc {
	DwarfLineNumContentType content_type;
	DwarfFormCode form_code;
} DwarfFormatDesc;

/* TODO: Type for 32 bit dwarf header */

/* The line number program header for a compilation unit (64 bit format) */
typedef struct LineNumProgHeader64 {
	InitialLength64 unit_length;
	uint16_t version;
	uint8_t address_size;
	uint8_t segment_selector_size;
	size_t header_length; /* TODO: This is dependent on the object file not the system */
	uint8_t minimum_instruction_length;
	uint8_t maximum_operations_per_instruction;
	uint8_t default_is_stmt;
	int8_t line_base;
	uint8_t line_range;
	uint8_t opcode_base;
	const uint8_t *standard_opcode_lengths;
	uint8_t directory_entry_format_count;
	DwarfFormatDesc *directory_entry_format;
	uint64_t directories_count;
	/* TODO: directories */
	uint8_t file_name_entry_format_count;
	DwarfFormatDesc *file_name_entry_format;
	uint64_t file_names_count;
	/* TODO: file_names */
} LineNumProgHeader64;

extern void parse_debug_line_section(DebugLineSection debug_line_section);

#endif /* _DABUGGER_DWARF_H */
