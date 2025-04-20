/* 
 * jedit the CLI tool to quickly
 * save, jump to, and edit 
 * files and directories in in the editor
 * of your choice
 *
*/
#include <assert.h>
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

	Cmd cmd;

	// instead of initializing whole array to 0
	// just set first char to null terminator saves CPU time 
	char jump_path[BUF_SIZE];		jump_path[0] = '\0';
	char jump_desc[BUF_SIZE];		jump_desc[0] = '\0';
	char edit_jump_desc[BUF_SIZE];	edit_jump_desc[0] = '\0';

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
			memcpy(jump_desc, buf, BUF_SIZE);
		}

		// stores user command as 
		if (i == 2) memcpy(edit_jump_desc, buf, BUF_SIZE);

		// don't need to null terminate again b/c buf is safe
		if (i == 3) memcpy(jump_path, buf, BUF_SIZE);

	}


	// Create/check persistence file path
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home == NULL){
		xdg_data_home = getenv("HOME");
		if (xdg_data_home == NULL) {
			fprintf(stderr, "Neither XDG_DATA_HOME nor HOME is set\n");
		}
	}


	// concat home to jedit directory	
	// snprintf returns # if chars it attempted to write
	char jedit_dir[BUF_SIZE];
	int char_written= snprintf(jedit_dir, BUF_SIZE, "%s/.local/share/jedit", xdg_data_home);
	assert(char_written < BUF_SIZE); // if attempted chars does not fit into buffer

	// concat jedit directory to jedit database file
	char jedit_gdbm_dir[BUF_SIZE];
	char_written = snprintf(jedit_gdbm_dir, BUF_SIZE, "%s/jedit.gdbm", jedit_dir);
	assert(char_written < BUF_SIZE);


	// create database directory
	int status = mkdir(jedit_dir, 0777);
	if (status == 0) {
		printf("Directory created: %s\n", jedit_dir);
	} else if(status == -1) {
		printf("Directory '%s' already exists\n", jedit_dir);
	} else {
		fprintf(stderr, "Error: File %s:%d could not make database directory\n", __FILE__, __LINE__);
		return 1;
	}
	
	
	// Set up database
	GDBM_FILE db = gdbm_open(jedit_gdbm_dir, 0, GDBM_WRCREAT, 0600, NULL);
	assert(db && "Failed to open database");


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
			printf("descriptor: %s\n", edit_jump_desc);
			printf("directory: %s\n", jump_path);

			if (*edit_jump_desc == '\0' ) {
				fprintf(stderr, "could not add jedit command, no descriptor provided\n");
				return 1;
			}

			if (*jump_path == '\0' ) {
				fprintf(stderr, "could not add jedit command, no directory provided\n");
				return 1;
			}


			datum add_desc_key = { (void*)edit_jump_desc, strlen(edit_jump_desc) };
			datum add_desc_val = { (void*)jump_path, strlen(jump_path) };

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
