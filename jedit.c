/* 
 * jedit the CLI tool to quickly
 * save, jump to, and edit 
 * files and directories in in the editor
 * of your choice
 *
*/
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdbm.h>

#define BUF_SIZE 1024

typedef enum { 
	CMD_OTHER,
	CMD_LIST,
	CMD_ADD, 
	CMD_REMOVE, 
	CMD_EDITOR,
	CMD_HELP,
}Cmd;

Cmd parse_cmd(const char *buf) {
	// strcmp returns 0 if strings are equal
	// returns int of the difference between the first 
	// non-same char. Good for lexicographically sorting
	if(!strcmp(buf, "list")) return CMD_LIST;
	if(!strcmp(buf, "ls")) return CMD_LIST;
	if(!strcmp(buf, "l")) return CMD_LIST;

	if(!strcmp(buf, "add")) return CMD_ADD;
	if(!strcmp(buf, "rm"))  return CMD_REMOVE; 
	if(!strcmp(buf, "editor"))  return CMD_EDITOR; 

	if(!strcmp(buf, "--help")) return CMD_HELP;
	if(!strcmp(buf, "-h")) return CMD_HELP;
	return CMD_OTHER;
}

int main(int argc, char **argv) {

	// components of CLI tool
	Cmd cmd;
	char dir[BUF_SIZE] = {0};
	char add_desc[BUF_SIZE] = {0};
	char jump_desc[BUF_SIZE] = {0};

	// Extract argument
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

		// directory is used when adding a new user jedit jump
		if (i == 1) {
			cmd = parse_cmd(buf);
			// need jump_desc for CMD_OTHER case
			memcpy(jump_desc, buf, sizeof(buf));
		}

		// don't need to null terminate again b/c buf is safe
		if (i == 2) memcpy(dir, buf, sizeof(buf));

		// stores user command as 
		if (i == 3) memcpy(add_desc, buf, sizeof(buf));

	}


	// Create/check persistence file path
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home == NULL){
		xdg_data_home = getenv("HOME");
		if (xdg_data_home == NULL) {
			fprintf(stderr, "Neither XDG_DATA_HOME nor HOME is set\n");
		}
	}


	// concat directory path
	char jedit_dir[BUF_SIZE];
	snprintf(jedit_dir, sizeof(jedit_dir), "%s/.local/share/jedit", xdg_data_home);


	int status = mkdir(jedit_dir, 0777);
	if (status == 0) {
		printf("Directory created: %s\n", jedit_dir);
	} else if(status == -1) {
		printf("Directory '%s' already exists\n", jedit_dir);
	} else {
		perror("Error creating direcory\n");
		return 1;
	}
	

	// concat jedit.gdbm to jedit directory
	char jedit_gdbm_dir[BUF_SIZE];
	snprintf(jedit_gdbm_dir, sizeof(jedit_gdbm_dir), "%s/.local/share/jedit/jedit.gdbm", xdg_data_home);

	
	// Set up database
	GDBM_FILE db = gdbm_open(jedit_gdbm_dir, 0, GDBM_WRCREAT, 0600, NULL);
	if (!db) {
		fprintf(stderr, "failed to open database\n");
		return 1;
	}


	// handle commands
	switch(cmd) {
		case CMD_OTHER:  // check db for user commands
			
			datum jump_desc_key = { (void*)jump_desc, strlen(jump_desc) };
			datum fetched = gdbm_fetch(db, jump_desc_key);

			if (fetched.dptr) {
				printf("jump command found: %s\n", jump_desc);
				printf("directory: %.*s\n", (int)fetched.dsize, fetched.dptr);
			} else {
				printf("jump command \'%s\' not found\n", jump_desc);
			}

			// I hate C
			free(fetched.dptr);

			break;

		case CMD_LIST:   // print list and directories

			break;

		case CMD_ADD:    // adds user command

			// if something was not written into it
			if (*dir == '\0' ) {
				fprintf(stderr, "could not add jedit command, no directory provided\n");
				return 1;
			}
			if (*add_desc == '\0' ) {
				fprintf(stderr, "could not add jedit command, no descriptor provided\n");
				return 1;
			}


			datum add_desc_key = { (void*)add_desc, strlen(add_desc) };
			datum add_desc_val = { (void*)dir, strlen(dir) };

			if (gdbm_store(db, add_desc_key, add_desc_val, GDBM_REPLACE) != 0) {
				fprintf(stderr, "Store error\n");
			}

			break;

		case CMD_REMOVE: // removes user command
						 
			break;

		case CMD_EDITOR: // set/change default editor
						
			break;

		case CMD_HELP:   // you know
						 
			break;

	}


	gdbm_close(db);
	return 0;

}
