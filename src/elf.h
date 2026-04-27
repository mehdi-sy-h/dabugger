#ifndef DABUGGER_ELF_H
#define DABUGGER_ELF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	size_t size;
	uint8_t *data;
} SectionBuffer;

typedef struct {
	SectionBuffer debug_line;
	SectionBuffer debug_str;
	SectionBuffer debug_line_str;
} ProgramSections;

typedef struct {
	ProgramSections sections;
	size_t entry_point;
} ProgramData;

ProgramData parse_elf_file(const char *path);

#endif /* DABUGGER_ELF_H */
