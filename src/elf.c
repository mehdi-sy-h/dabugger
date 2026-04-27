#include "elf.h"

#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ProgramData parse_elf_file(const char *path) {
	FILE *elf_file = fopen(path, "rb");
	if (elf_file == NULL)
		goto sys_error;

	size_t read_count;
	int seek_result;

	Elf64_Ehdr elf_header = {0};

	read_count = fread(&elf_header.e_ident, EI_NIDENT, 1, elf_file);
	if (read_count != 1)
		goto sys_error;

	if (memcmp(&elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
		fprintf(stderr, "This is not an ELF file!\n");
		goto error;
	}

	if (elf_header.e_ident[EI_CLASS] != ELFCLASS64) {
		fprintf(stderr,
				"dabugger currently only supports 64 bit executables.\n");
		goto error;
	}

	/* Would e_ident even have been read correctly if it were big endian? */
	if (elf_header.e_ident[EI_DATA] != ELFDATA2LSB) {
		fprintf(
			stderr,
			"dabugger currently only supports little endian executables.\n");
		goto error;
	}

	read_count =
		fread(&elf_header.e_type, sizeof(Elf64_Ehdr) - EI_NIDENT, 1, elf_file);
	if (read_count != 1)
		goto sys_error;

	if (elf_header.e_machine != EM_X86_64) {
		fprintf(stderr,
				"dabugger currently only supports x86-64 executables.\n");
		goto error;
	}

	Elf64_Shdr *section_header =
		malloc(elf_header.e_shentsize * elf_header.e_shnum);

	seek_result = fseek(elf_file, (long)elf_header.e_shoff, SEEK_SET);
	if (seek_result != 0)
		goto sys_error;

	read_count = fread(section_header, elf_header.e_shentsize,
					   elf_header.e_shnum, elf_file);
	if (read_count != elf_header.e_shnum)
		goto sys_error;

	Elf64_Shdr string_table_header = section_header[elf_header.e_shstrndx];

	seek_result =
		fseek(elf_file, (long)string_table_header.sh_offset, SEEK_SET);
	if (seek_result != 0)
		goto sys_error;

	char *section_names = malloc(string_table_header.sh_size);

	read_count = fread(section_names, string_table_header.sh_size, 1, elf_file);
	if (read_count != 1)
		goto sys_error;

	SectionBuffer debug_line_section = {0};
	SectionBuffer debug_str_section = {0};
	SectionBuffer debug_line_str_section = {0};

	for (Elf64_Half i = 0; i < elf_header.e_shnum; i++) {
		Elf64_Shdr current_section_header = section_header[i];
		Elf64_Word section_name_offset = current_section_header.sh_name;

		char *section_name = &section_names[section_name_offset];

		SectionBuffer *current_section = NULL;

		if (strcmp(section_name, ".debug_line") == 0) {
			current_section = &debug_line_section;
		} else if (strcmp(section_name, ".debug_str") == 0) {
			current_section = &debug_str_section;
		} else if (strcmp(section_name, ".debug_line_str") == 0) {
			current_section = &debug_line_str_section;
		} else {
			continue;
		}

		current_section->size = current_section_header.sh_size;
		current_section->data = malloc(current_section_header.sh_size);

		seek_result =
			fseek(elf_file, (long)current_section_header.sh_offset, SEEK_SET);
		if (seek_result != 0)
			goto sys_error;

		read_count = fread(current_section->data,
						   current_section_header.sh_size, 1, elf_file);
		if (read_count != 1)
			goto sys_error;
	}

	free(section_names);
	free(section_header);
	fclose(elf_file);

	ProgramSections sections = {.debug_line = debug_line_section,
								.debug_str = debug_str_section,
								.debug_line_str = debug_line_str_section};

	ProgramData data = {.sections = sections,
						.entry_point = elf_header.e_entry};
	return data;

sys_error:
	perror("error parsing file");
error:
	fprintf(stderr, "Failed to parse file as an x86-64 ELF executable: %s\n",
			path);
	exit(EXIT_FAILURE);
}
