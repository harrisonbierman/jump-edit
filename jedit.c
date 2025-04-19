/* 
 * jedit the CLI tool to quickly
 * save, jump to, and edit 
 * files and directories in in the editor
 * of your choice
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1024

int main(int argsc, char **argsv) {

	// Extract Argument
	for(int i = 0; i < argsc; i++) {

		if (strnlen(argsv[i], BUF_SIZE) >= BUF_SIZE) {
			fprintf(stderr, "argument too long, truncated\n");
			exit(EXIT_FAILURE);
		}

		// Do not initialize buf to buf[] = {0} 
		// because it waste clock cycles
		// instead manually add '\0' to end of string
		char buf[BUF_SIZE];

		// BUF_SIZE - 1 leaves room for null terminator
		size_t arg_len = strnlen(argsv[i], BUF_SIZE - 1);

		// always null terminate after memory copy
		memcpy(buf, argsv[i], arg_len); 
		buf[arg_len] = '\0';

		printf("buf: %s\n", buf);

	}


	return 0;

}
