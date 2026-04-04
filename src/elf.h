#ifndef _DABUGGER_ELF_H
#define _DABUGGER_ELF_H

#include <stddef.h>
#include <stdint.h>

typedef struct DebugLineSection {
	size_t size;
	uint8_t *data;
} DebugLineSection;

extern DebugLineSection parse_elf64_file(const char *path);

#endif /* _DABUGGER_ELF_H */
