cmake_minimum_required(VERSION 3.30)

project(dabugger
	VERSION 0.1.0
	LANGUAGES C
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
	-Wall
	-Wextra
	-Wpedantic
	-Wshadow
)

add_executable(${PROJECT_NAME}
	src/dabugger.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
	$<$<CONFIG:debug>:-g3 -O0 -fsanitize=address,undefined>
	$<$<CONFIG:release>:-O2>
)

target_link_options(${PROJECT_NAME} PRIVATE
  $<$<CONFIG:debug>:-fsanitize=address,undefined>
)

install(TARGETS ${PROJECT_NAME})
