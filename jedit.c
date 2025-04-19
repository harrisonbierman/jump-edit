/* 
 * jedit the CLI tool to quickly
 * save, jump to, and edit 
 * files and directories in in the editor
 * of your choice
 *
*/

#include <stdio.h>
#include <string.h>

int main(int argsc, char **argsv) {
	for(int i = 0; i < argsc; i++) {

		printf("Pointer: %p\n", argsv[i]);
		printf("sizeof(Pointer) %zu\n", sizeof(argsv[i]));
		printf("distance to next pointer: %ld\n", argsv[i+1] - argsv[i]);

		char buf[64] = {0};
		size_t arg_len = strnlen(argsv[i], sizeof(buf) - 1);
		printf("arg_len: %zu \n", arg_len);
		for (size_t j = 0; j < arg_len; j++) {
			buf[j] = argsv[i][j];	
		}

		printf("Argument %d: %s\n\n", i, buf); 

	}


	return 0;

}
