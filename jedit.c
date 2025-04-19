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

	// Extract Argument
	for(int i = 0; i < argsc; i++) {
		char buf[1024] = {0};
		size_t arg_len = strnlen(argsv[i], sizeof(buf) - 1);
		for (size_t j = 0; j < arg_len; j++) {
			buf[j] = argsv[i][j]; 	
		}

	}


	return 0;

}
