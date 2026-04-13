#include "elf.h"

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: More specific/appropriate name,
 * and perhaps check both 32 and 64 bit cases */
/* TODO: Refactor this function its garbage */
DebugSections parse_elf64_file(const char *path) {
	FILE *elf_file = fopen(path, "rb");

	if (elf_file == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE); // TODO: Return error code instead?
	}

	Elf64_Ehdr elf_header = {0};

	size_t read_count = fread(&elf_header, sizeof(Elf64_Ehdr), 1, elf_file);
	if (read_count != 1) {
		perror("fread");
		exit(EXIT_FAILURE);
	}

	/* TODO: Probably safer to put this before reading the whole thing and also
	 * you can check format size while you're at it*/
	if (memcmp(&elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
		perror("memcmp");
		exit(EXIT_FAILURE);
	};

	Elf64_Shdr *section_header =
		malloc(elf_header.e_shentsize * elf_header.e_shnum);

	printf("section header offset: %lx\n", elf_header.e_shoff);
	printf("section header entry size: %d\n", elf_header.e_shentsize);
	printf("section header count: %d\n", elf_header.e_shnum);

	if (fseek(elf_file, elf_header.e_shoff, SEEK_SET) != 0) {
		perror("fseek");
		exit(EXIT_FAILURE);
	};

	read_count = fread(section_header, elf_header.e_shentsize,
					   elf_header.e_shnum, elf_file);

	if (read_count != elf_header.e_shnum) {
		perror("fread");
		exit(EXIT_FAILURE);
	}

	// TODO: Determine if using pointer to string_table is more performant
	Elf64_Shdr string_table_header = section_header[elf_header.e_shstrndx];

	if (fseek(elf_file, string_table_header.sh_offset, SEEK_SET) != 0) {
		perror("fseek");
		exit(EXIT_FAILURE);
	};

	char *section_names = malloc(string_table_header.sh_size);

	read_count = fread(section_names, string_table_header.sh_size, 1, elf_file);

	if (read_count != 1) {
		perror("fread");
		exit(EXIT_FAILURE);
	}

	SectionBuffer debug_line_section = {0};
	SectionBuffer debug_line_str_section = {0};

	for (Elf64_Half i = 0; i < elf_header.e_shnum; i++) {
		Elf64_Shdr current_section_header = section_header[i];
		Elf64_Word section_name_offset = current_section_header.sh_name;

		char *section_name = &section_names[section_name_offset];

		if (strcmp(section_name, ".debug_line") == 0) {
			printf("%s | section num: %d\n", section_name, i);

			debug_line_section.size = current_section_header.sh_size;
			printf("sz: %ld\n", debug_line_section.size);
			debug_line_section.data = malloc(current_section_header.sh_size);

			// TODO: Proper error handling or just write a reader util
			fseek(elf_file, current_section_header.sh_offset, SEEK_SET);

			fread(debug_line_section.data, current_section_header.sh_size, 1,
				  elf_file);
		} else if (strcmp(section_name, ".debug_line_str") == 0) {
			printf("%s | section num: %d\n", section_name, i);

			debug_line_str_section.size = current_section_header.sh_size;
			debug_line_str_section.data =
				malloc(current_section_header.sh_size);

			// TODO: Proper error handling or just write a reader util
			fseek(elf_file, current_section_header.sh_offset, SEEK_SET);

			fread(debug_line_str_section.data, current_section_header.sh_size,
				  1, elf_file);
		}
	}

	DebugSections debug_sections = {.debug_line = debug_line_section,
									.debug_line_str = debug_line_str_section};

	return debug_sections;
}
