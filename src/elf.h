#ifndef _DABUGGER_ELF_H
#define _DABUGGER_ELF_H

#include "reader.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
	size_t size;
	uint8_t *data;
} SectionBuffer;

typedef struct {
	SectionBuffer debug_line;
	SectionBuffer debug_line_str;
} DebugSections;

extern DebugSections parse_elf64_file(const char *path);

#endif /* _DABUGGER_ELF_H */
