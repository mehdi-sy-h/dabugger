# dabugger
An x86-64 Linux debugger for ELF+DWARF executables.

You may find a series of articles describing the development process for this debugger on my blog [here](https://mehdisy.com/blog/dabugger).

Also, I'm looking for a job. If you are interested in hiring me (as a graduate or junior developer) and you are in the UK (remote, or hybrid/office in or near London), please email me at mehdi.h.business@pm.me. 

## Overview
`dabugger` implements a barebones DWARF 5 line number program (`.debug_line`) parser, an ELF64 parser and a terminal user interface with basic vim motions. Since we parse `.debug_line` we can map assembly instructions to the relevant source lines, provided the debuggee was compiled with debug symbols (that is, with the `-g` flag for `gcc` or `clang`). DWARF 5 requires that the current directory be included in the line program headers, so parsing the `.debug_line` section is effectively self contained. This means we don't have to parse the other DWARF sections (containing the DIEs), which are significantly more tedious to implement. However this means that `dabugger` cannot currently map variables to their respective addresses in memory, and features more complicated than this. The only dependencies are `libc`, `zydis` (for x86-64 disassembly, which is necessary for the assembly view) and `ncurses` (for the terminal user interface).

## Building
If you are using the Nix package manager or NixOS, there is a flake available in this repo. Running it is trivial (`nix run . path-to-your-executable`).

If you do not have Nix, you can build the project with `cmake`. You need the library packages for `ncurses` (usually installed by default) and `zydis` (instructions [here](https://github.com/zyantific/zydis#package-managers)) installed on your system. Then, while in the repo directory, build with `cmake`:

```sh
cmake -B build
cmake --build build
```

If you want to build the debugger with debug symbols, do the above but swap the first command for `cmake -B build -DCMAKE_BUILD_TYPE=Debug`.

## Usage
**NOTE:** This is a personal toy project, I do not recommend using this in production and certainly not for debugging suspicious executables. If you want to debug a production application, please use GDB or LLDB instead, or the many frontends available for those. Obviously you shouldn't execute suspicious executables at all.

If you are using `gcc` or `clang`, compile the program you want to debug with the additional flags `-gdwarf-5 -g3 -Og`.
Then run `dabugger path-to-your-executable`.

### 64 bit DWARF Support
`dabugger` supports the 64 bit DWARF format, however if you are compiling with `gcc` with `-gdwarf64` you must also supply `-gno-as-loc-support`. This is because the GNU assembler does not emit `.debug_line` in DWARF64, but `gcc` can do so internally. If you want to keep track of this issue, you can find my bug report [here](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124847). Alternatively, `clang` appears to generate 64 bit DWARF without issue.

Regardless, the DWARF specification recommends using the 32 bit DWARF format anyway. You will most likely never need 64 bit DWARF; it does not refer to the word size of the executable but rather the offset address sizes in the `.debug_*` sections, and these sections are highly unlikely to exceed 4GiB even for a massive codebase (compiling the `gcc` version 16.0.1 compiler itself with debug symbols, you find that all `.debug_*` sections are less than 12MiB).

## Todo?
- Parse other DWARF sections (`.debug_info`, etc) to implement variable inspection.
- Attach to process mode (requires handling position independent executables and ASLR)
