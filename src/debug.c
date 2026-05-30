#include "debug.h"
#include "dwarf.h"
#include "elf.h"

#include <Zydis/Disassembler.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

#define INT3 0xccU

/* The caller is responsible for freeing the returned string */
/* TODO: Put file paths in session struct so we can free these nicely */
static char *get_file_path(LineNumProgHeader64 *line_prog_header,
						   size_t file_index) {
	/* TODO: If the path starts with /build/ then assume it was built with
	 * nix (do further checks if there is a nice way to check) and replace
	 * with /nix/store/ */
	DwarfLineNumContentEntry file_entry =
		line_prog_header->file_names[file_index];

	const char *file_name = file_entry.path;
	if (!file_name)
		return NULL;

	/* Some build systems give the file entry path in absolute form and some
	 * give a relative path (cmake, nix and plain gcc uses different forms) */
	if (file_name[0] == '/')
		return strdup(file_name);

	/* Note that the compilation directory may not necessarily exist during
	 * runtime, for example when using `nix build`. */
	const char *directory =
		line_prog_header->directories[file_entry.directory_index].path;
	if (!directory)
		return NULL;

	if (directory[0] == '/') {
		char *file_path = malloc(strlen(directory) + 1 + strlen(file_name) + 1);
		char *p = stpcpy(file_path, directory);
		*p++ = '/';
		strcpy(p, file_name);
		return file_path;
	}

	const char *compilation_dir = line_prog_header->directories[0].path;
	if (!compilation_dir)
		return NULL;

	char *file_path = malloc(strlen(compilation_dir) + 1 + strlen(directory) +
							 1 + strlen(file_name) + 1);
	char *p = stpcpy(file_path, compilation_dir);
	*p++ = '/';
	p = stpcpy(p, directory);
	*p++ = '/';
	strcpy(p, file_name);

	return file_path;
}

/* Sets the breakpoint by inserting an INT3 instruction at address.
 * Returns the original byte that was replaced by INT3. */
static uint8_t set_process_breakpoint(pid_t pid, size_t address) {
	/* TODO: Handle errors */
	uint64_t word = (uint64_t)ptrace(PTRACE_PEEKTEXT, pid, address);
	uint8_t original_byte = word & 0xffU;
	uint64_t breakpoint_word = (word & ~0xffULL) | INT3;
	ptrace(PTRACE_POKETEXT, pid, address, breakpoint_word);
	return original_byte;
}

void spawn_inferior(DebugSession *session) {
	/* TODO: Reap old child process */
	assert(session->state == DEBUG_DEAD);

	pid_t pid = forkpty(&session->inferior_master_fd, NULL, NULL, NULL);

	if (pid == 0) {
		ptrace(PTRACE_TRACEME);
		/* TODO: Read load address in /proc/pid/maps to support PIE and ASLR. */
		personality(ADDR_NO_RANDOMIZE);
		execv(session->inferior_path, session->inferior_args);
		/* exec() functions only return on error
		 * Also, we use _exit instead of exit because the debugger process might
		 * want to read the child's stdio streams, and _exit() does not flush
		 * them (and doesn't clean up some other resources), but exit() does.
		 */
		_exit(EXIT_FAILURE);
	} else if (pid == -1) {
		/* TODO: Handle error */
	}

	session->inferior_pid = pid;
	session->state = DEBUG_RUNNING;

	int status;
	waitpid(pid, &status, 0);

	ptrace(PTRACE_SETOPTIONS, session->inferior_pid, NULL, PTRACE_O_EXITKILL);

	/* TODO: Better (more "functional/reactive"?) way to update breakpoints */
	Breakpoints *breakpoint_data = session->breakpoints;

	for (size_t i = 0; i < breakpoint_data->breakpoint_count; i++) {
		Breakpoint breakpoint = breakpoint_data->breakpoints[i];
		set_process_breakpoint(pid, breakpoint.address);
	}
}

void continue_inferior(DebugSession *session) {
	/*assert(session->state == DEBUG_DEAD || session->state ==
	 * DEBUG_BREAKPOINT);*/
	ptrace(PTRACE_CONT, session->inferior_pid);
}

void stop_inferior(DebugSession *session) {}

DebugSession *init_debug_session(const char *inferior_path,
								 char **inferior_args) {
	DebugSession *debug_session = calloc(1, sizeof(DebugSession));
	if (!debug_session) {
		/* TODO */
	}

	debug_session->inferior_path = inferior_path;
	debug_session->inferior_args = inferior_args;
	debug_session->inferior_master_fd = -1;
	debug_session->program_data = calloc(1, sizeof(ProgramData));
	debug_session->line_info = calloc(1, sizeof(LineInfo));
	debug_session->breakpoints = calloc(1, sizeof(Breakpoints));
	debug_session->src_breakpoints = calloc(1, sizeof(SourceBreakpoints));
	debug_session->output.cursor = debug_session->output.buffer;

	if (!debug_session->program_data || !debug_session->line_info) {
		/* TODO */
	}

	*debug_session->program_data = parse_elf_file(inferior_path);
	debug_session->line_info =
		parse_debug_line_section(debug_session->program_data->sections);

	return debug_session;
}

void handle_inferior_signal(DebugSession *session, int signal_child_fd) {
	/* See signal(7): Queueing and delivery semantics for standard signals.
	 * We only need to read one signalfd_siginfo structure from the signal file
	 * descriptor, and since we don't care about the results (we get the
	 * information we need from waitpid) we read it into a temporary buffer.
	 */
	struct signalfd_siginfo _sig_info;
	read(signal_child_fd, &_sig_info, sizeof(_sig_info));

	int status;

	/* FIX: Should this be a while loop with the inverse condition instead? */
	if (waitpid(session->inferior_pid, &status, __WALL | WNOHANG) <= 0)
		return; /*current_msg;*/

	/* TODO: Maybe add exit status/signal/stop signal to msg.value */
	if (WIFEXITED(status)) {
		/* TODO */
	} else if (WIFSTOPPED(status)) {
		if (WSTOPSIG(status) == SIGTRAP)
			/* TODO */;

		/* TODO */
	} else if (WIFSIGNALED(status)) {
		/* TODO */
	} else if (WIFCONTINUED(status)) {
		/* TODO */
	}
}

void read_inferior_output(DebugSession *session) {
	/* TODO: Error handling and precondition checks */
	/* TODO: Put in reader.c instead? */
	size_t cursor_offset =
		(size_t)(session->output.cursor - session->output.buffer);
	if (cursor_offset + 1 > MAX_OUTPUT_SIZE) {
		cursor_offset = 0;
		session->output.cursor = session->output.buffer;
	}
	size_t bytes_remaining = MAX_OUTPUT_SIZE - cursor_offset;
	long bytes_read = read(session->inferior_master_fd, session->output.cursor,
						   bytes_remaining);
	if (bytes_read == -1) {
		/* TODO */
	}
	session->output.cursor += bytes_read;
}

/* The caller is responsible for freeing the returned buffer */
LinesBuffer *get_source_buffer(DebugSession *session, size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));

	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];
	char *file_name = get_file_path(comp_unit.header, 0);

	FILE *file = fopen(file_name, "r");
	if (!file) {
		/* TODO: Handle */
	}

	char *line = NULL;
	size_t line_len = 0;

	for (unsigned line_num = 0; getline(&line, &line_len, file) > 0;
		 line_num++) {
		line[strcspn(line, "\n")] = '\0';

		buffer->lines =
			reallocarray(buffer->lines, line_num + 1, sizeof(char *));
		buffer->lines[line_num] = calloc(1, line_len);
		buffer->line_count = line_num + 1;

		strcpy((char *)buffer->lines[line_num], line);
	}

	fclose(file);
	free(file_name);

	return buffer;
}

/* The caller is responsible for freeing the returned buffer */
AssemblyBuffer *get_assembly_buffer(DebugSession *session,
									size_t comp_unit_index) {
	if (comp_unit_index >= session->line_info->comp_unit_count) {
		return NULL;
	}

	AssemblyBuffer *asm_buffer = calloc(1, sizeof(AssemblyBuffer));
	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));
	asm_buffer->text_buffer = buffer;

	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];

	SectionBuffer text_section = session->program_data->sections.text;

	for (size_t i = 0; i < comp_unit.table->sequences_count; i++) {
		const LineInfoSequence *sequence = &comp_unit.table->sequences[i];
		if (sequence->entry_count == 0)
			continue;

		size_t start_addr = sequence->entries[0].address;
		size_t end_addr = sequence->entries[sequence->entry_count - 1].address;

		ZydisDisassembledInstruction instruction;
		ZyanU64 instruction_addr = start_addr;
		unsigned line_num = 0;

		while (ZYAN_SUCCESS(ZydisDisassembleIntel(
			ZYDIS_MACHINE_MODE_LONG_64, instruction_addr,
			text_section.data + (instruction_addr - text_section.address),
			end_addr - instruction_addr, &instruction))) {
			buffer->lines =
				reallocarray(buffer->lines, line_num + 1, sizeof(char *));
			buffer->lines[line_num] = calloc(1, strlen(instruction.text) + 1);
			buffer->line_count =
				line_num + 1; /* Safer to update this every iteration */

			asm_buffer->addresses = reallocarray(
				asm_buffer->addresses, buffer->line_count, sizeof(size_t *));
			asm_buffer->addresses[line_num] = instruction_addr;

			strcpy((char *)buffer->lines[line_num], instruction.text);

			line_num += 1;
			instruction_addr += instruction.info.length;
		}
	}

	return asm_buffer;
}

/* The caller is responsible for freeing the returned buffer */
LinesBuffer *get_file_picker_buffer(DebugSession *session) {
	LinesBuffer *buffer = calloc(1, sizeof(LinesBuffer));
	buffer->lines = calloc(session->line_info->comp_unit_count, sizeof(char *));
	buffer->line_count = session->line_info->comp_unit_count;

	for (size_t i = 0; i < session->line_info->comp_unit_count; i++) {
		char *file_name =
			get_file_path(session->line_info->comp_units[i].header, 0);
		buffer->lines[i] = file_name;
	}

	return buffer;
}

void free_lines_buffer(LinesBuffer *buffer) {
	for (size_t i = 0; i < buffer->line_count; i++) {
		free((void *)buffer->lines[i]);
	}
	free(buffer->lines);
	free(buffer);
}

/* Get the VMAs associated with the (1-indexed) line number.
 * The caller is responsible for freeing the returned buffer. */
LineInstructions *get_instructions_for_line(DebugSession *session,
											size_t comp_unit_index,
											size_t line_num) {
	assert(line_num != 0);

	LineInstructions *line_instructions = calloc(1, sizeof(LineInstructions));
	LineInfoCompUnit comp_unit =
		session->line_info->comp_units[comp_unit_index];

	/* TODO: Consider memoizing the addresses in get_source_buffer() */
	for (size_t i = 0; i < comp_unit.table->sequences_count; i++) {
		LineInfoSequence sequence = comp_unit.table->sequences[i];
		for (size_t j = 0; j < sequence.entry_count; j++) {
			LineInfoEntry entry = sequence.entries[j];
			if (entry.end_sequence)
				continue;

			if (entry.line == line_num) {
				line_instructions->instruction_count += 1;
				line_instructions->instructions =
					reallocarray(line_instructions->instructions,
								 line_instructions->instruction_count,
								 sizeof(LineInfoEntry));
				line_instructions
					->instructions[line_instructions->instruction_count - 1] =
					entry;
			}
		}
	}

	return line_instructions;
}

void free_line_instructions(LineInstructions *line_instructions) {
	free(line_instructions->instructions);
	free(line_instructions);
}

/* Returns true on success, false otherwise */
bool add_breakpoint(DebugSession *session, size_t address) {
	Breakpoints *breakpoint_data = session->breakpoints;

	for (size_t i = 0; i < breakpoint_data->breakpoint_count; i++) {
		if (breakpoint_data->breakpoints[i].address == address)
			return false;
	}

	/* TODO: Handle error */

	/* TODO: Maybe don't call this function when the debuggee hasnt been spawned
	 * yet. */
	/*
	uint8_t original_byte =
		set_process_breakpoint(session->inferior_pid, address);
	*/

	Breakpoint breakpoint = {.address = address, .original_byte = 0};

	breakpoint_data->breakpoint_count += 1;
	breakpoint_data->breakpoints =
		reallocarray(breakpoint_data->breakpoints,
					 breakpoint_data->breakpoint_count, sizeof(Breakpoint));
	breakpoint_data->breakpoints[breakpoint_data->breakpoint_count - 1] =
		breakpoint;

	return true;
}

void remove_breakpoint(DebugSession *session, size_t address) {
	Breakpoints *breakpoint_data = session->breakpoints;

	for (size_t i = 0; i < breakpoint_data->breakpoint_count; i++) {
		if (breakpoint_data->breakpoints[i].address == address) {
			for (size_t j = i; j < breakpoint_data->breakpoint_count - 1; j++) {
				breakpoint_data->breakpoints[j] =
					breakpoint_data->breakpoints[j + 1];
			}
			breakpoint_data->breakpoint_count -= 1;
			breakpoint_data->breakpoints = reallocarray(
				breakpoint_data->breakpoints, breakpoint_data->breakpoint_count,
				sizeof(Breakpoint));
			break;
		}
	}

	SourceBreakpoints *src_breakpoint_data = session->src_breakpoints;

	for (size_t i = 0; i < src_breakpoint_data->src_breakpoint_count; i++) {
		SourceBreakpoint src_breakpoint =
			src_breakpoint_data->src_breakpoints[i];

		if (src_breakpoint.address == address)
			remove_source_breakpoint(session, src_breakpoint.comp_unit_index,
									 src_breakpoint.line_num);
	}
}

void toggle_breakpoint(DebugSession *session, size_t address) {
	Breakpoints *breakpoint_data = session->breakpoints;
	for (size_t i = 0; i < breakpoint_data->breakpoint_count; i++) {
		if (breakpoint_data->breakpoints[i].address == address) {
			remove_breakpoint(session, address);
			return;
		}
	}
	add_breakpoint(session, address);
}

bool set_source_breakpoint(DebugSession *session, size_t comp_unit_index,
						   size_t line_num) {
	SourceBreakpoints *src_breakpoint_data = session->src_breakpoints;

	for (size_t i = 0; i < src_breakpoint_data->src_breakpoint_count; i++) {
		SourceBreakpoint src_breakpoint =
			src_breakpoint_data->src_breakpoints[i];

		if (src_breakpoint.comp_unit_index == comp_unit_index &&
			src_breakpoint.line_num == line_num)
			return false;
	}

	LineInstructions *selected_instructions =
		get_instructions_for_line(session, comp_unit_index, line_num);

	bool found_address = false;
	size_t address = 0;

	for (size_t i = 0; i < selected_instructions->instruction_count; i++) {
		LineInfoEntry entry = selected_instructions->instructions[i];
		if (entry.is_stmt) {
			found_address = true;
			address = entry.address;
			break;
		}
	}

	free_line_instructions(selected_instructions);

	/* TODO: Maybe try subsequent lines if there is no is_stmt address for the
	 * requested line. Will require notifying the user so they dont think its a
	 * bug. */
	if (!found_address)
		return false;

	/* Since add_breakpoint fails if there is more than one breakpoint for
	 * a given address, the user cannot set a source breakpoint and a normal
	 * breakpoint which correspond to the same address, at the same time. */
	bool is_breakpoint_set = add_breakpoint(session, address);
	if (is_breakpoint_set) {
		SourceBreakpoint src_breakpoint = {.address = address,
										   .comp_unit_index = comp_unit_index,
										   .line_num = line_num};

		src_breakpoint_data->src_breakpoint_count += 1;
		src_breakpoint_data->src_breakpoints =
			reallocarray(src_breakpoint_data->src_breakpoints,
						 src_breakpoint_data->src_breakpoint_count,
						 sizeof(SourceBreakpoint));
		src_breakpoint_data
			->src_breakpoints[src_breakpoint_data->src_breakpoint_count - 1] =
			src_breakpoint;
	}

	return is_breakpoint_set;
}

void remove_source_breakpoint(DebugSession *session, size_t comp_unit_index,
							  size_t line_num) {
	SourceBreakpoints *src_breakpoint_data = session->src_breakpoints;

	for (size_t i = 0; i < src_breakpoint_data->src_breakpoint_count; i++) {
		SourceBreakpoint src_breakpoint =
			src_breakpoint_data->src_breakpoints[i];

		if (src_breakpoint.comp_unit_index == comp_unit_index &&
			src_breakpoint.line_num == line_num) {

			for (size_t j = i;
				 j < src_breakpoint_data->src_breakpoint_count - 1; j++) {
				src_breakpoint_data->src_breakpoints[j] =
					src_breakpoint_data->src_breakpoints[j + 1];
			}
			src_breakpoint_data->src_breakpoint_count -= 1;
			src_breakpoint_data->src_breakpoints =
				reallocarray(src_breakpoint_data->src_breakpoints,
							 src_breakpoint_data->src_breakpoint_count,
							 sizeof(SourceBreakpoint));

			remove_breakpoint(session, src_breakpoint.address);
			break;
		}
	}
}

void toggle_source_breakpoint(DebugSession *session, size_t comp_unit_index,
							  size_t line_num) {
	SourceBreakpoints *src_breakpoint_data = session->src_breakpoints;

	for (size_t i = 0; i < src_breakpoint_data->src_breakpoint_count; i++) {
		SourceBreakpoint src_breakpoint =
			src_breakpoint_data->src_breakpoints[i];

		if (src_breakpoint.comp_unit_index == comp_unit_index &&
			src_breakpoint.line_num == line_num) {
			remove_source_breakpoint(session, comp_unit_index, line_num);
			return;
		}
	}

	set_source_breakpoint(session, comp_unit_index, line_num);
}
