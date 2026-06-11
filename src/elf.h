#ifndef DABUGGER_ELF_H
#define DABUGGER_ELF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t address;
    size_t size;
    uint8_t *data;
} SectionBuffer;

typedef struct {
    SectionBuffer text;
    SectionBuffer debug_line;
    SectionBuffer debug_str;
    SectionBuffer debug_line_str;
} ProgramSections;

typedef struct {
    ProgramSections sections;
    size_t entry_point;
    size_t load_address;
} ProgramData;

ProgramData parse_elf_file(const char *path);

#endif /* DABUGGER_ELF_H */
