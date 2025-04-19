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
#include <gdbm.h>

#define BUF_SIZE 1024

typedef enum { 
	CMD_UNKNOWN,
	CMD_ADD, 
	CMD_REMOVE, 
	CMD_HELP,
}Cmd;

Cmd parse_cmd(const char *buf) {
	// strcmp returns 0 if strings are equal
	// returns int of the difference between the first 
	// non-same char. Good for lexicographically sorting
	if(!strcmp(buf, "add")) return CMD_ADD;
	if(!strcmp(buf, "rm"))  return CMD_REMOVE; 
	if(!strcmp(buf, "--help")) return CMD_HELP;
	if(!strcmp(buf, "-h")) return CMD_HELP;
	return CMD_UNKNOWN;
}

int main(int argc, char **argv) {

	// Extract Argument
	// i is 1 b/c first arg is 'jedit'
	for(int i = 1; i < argc; i++) {

		if (strnlen(argv[i], BUF_SIZE) >= BUF_SIZE) {
			fprintf(stderr, "argument too long, truncated\n");
			exit(EXIT_FAILURE);
		}

		// Do not initialize buf to buf[] = {0} 
		// because it waste clock cycles
		// instead manually add '\0' to end of string
		char buf[BUF_SIZE];

		// BUF_SIZE - 1 leaves room for null terminator
		size_t arg_len = strnlen(argv[i], BUF_SIZE - 1);

		// always null terminate after memory copy
		memcpy(buf, argv[i], arg_len); 
		buf[arg_len] = '\0';

		printf("buf: %s\n", buf);
		
		// only if first argument
		if (i == 1) {
			Cmd cmd = parse_cmd(buf);

			switch(cmd) {
				case CMD_UNKNOWN:
					break;
				case CMD_ADD: 
					break;
				case CMD_HELP:
					break;
				case CMD_REMOVE:
					break;
			}
		}
	}


	return 0;

}
