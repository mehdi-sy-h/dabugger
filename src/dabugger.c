#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, [[maybe_unused]] char *argv[argc + 1]) {
	printf("Hello, world!\n");
	return EXIT_SUCCESS;
}
