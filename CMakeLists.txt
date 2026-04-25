cmake_minimum_required(VERSION 3.30)

project(dabugger
	VERSION 0.1.0
	LANGUAGES C
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
	-Wall
	-Wextra
	-Wpedantic
	-Wshadow
	-Wconversion
	-gdwarf64 # TODO: Remove once 32 bit DWARF is implemented
)

add_executable(${PROJECT_NAME}
	src/dabugger.c
	src/elf.c
	src/dwarf.c
	src/reader.c
	src/tui.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
	$<$<CONFIG:Debug>:-g3 -Og>
	$<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# TODO: Allow non-wide or curses support (will require guards in tui.h)
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

if(Curses_FOUND AND NOT TARGET Curses::Curses)
	add_library(Curses::Curses INTERFACE IMPORTED)
	set_target_properties(
				Curses::Curses
				PROPERTIES
						INTERFACE_LINK_LIBRARIES "${CURSES_LIBRARIES}"
						INTERFACE_INCLUDE_DIRECTORIES "${CURSES_INCLUDE_DIRS}"
		)
endif()

find_library(PANEL_LIBRARY panelw REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Curses::Curses ${PANEL_LIBRARY})

set(ZYDIS_FEATURE_ENCODER OFF)
find_package(Zydis REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Zydis)

install(TARGETS ${PROJECT_NAME})
