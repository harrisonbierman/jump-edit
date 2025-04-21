/* 
 * je (Jump Edit the CLI tool to quickly
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
	if(!strcmp(buf, "add")) return CMD_ADD;
	if(!strcmp(buf, "rm"))  return CMD_REMOVE; 
	if(!strcmp(buf, "editor"))  return CMD_EDITOR; 
	if(!strcmp(buf, "--help")) return CMD_HELP;
	return CMD_OTHER;
}

int main(int argc, char **argv) {
	
	// arg1 will be a sub_command or a jump descriptor
	Cmd cmd;
	char jump_desc[BUF_SIZE];	jump_desc[0] = '\0';

	// instead of initializing whole array to 0
	// just set first char to null terminator saves CPU time 
	char arg2[BUF_SIZE];		arg2[0] = '\0';
	char arg3[BUF_SIZE];		arg3[0] = '\0';

	// Extract argument and make them safe
	// i is 1 b/c first arg is 'je'
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

		// directory is used when adding a new user je jump
		if (i == 1) {
			// arg1 is either a command or a jump descriptor
			cmd = parse_cmd(buf);
			memcpy(jump_desc, buf, BUF_SIZE);
		}

		// stores user command as 
		if (i == 2) memcpy(arg2, buf, BUF_SIZE);

		// don't need to null terminate again b/c buf is safe
		if (i == 3) memcpy(arg3, buf, BUF_SIZE);

		if (i == 4) {
			fprintf(stderr, "Too many agruments\n");
			return 1;
		}

	}


	// Create/check persistence file path
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home == NULL){
		xdg_data_home = getenv("HOME");
		if (xdg_data_home == NULL) {
			fprintf(stderr, "Neither XDG_DATA_HOME nor HOME is set\n");
		}
	}


	// concat home to je directory	
	// snprintf returns # if chars it attempted to write
	char je_dir[BUF_SIZE];
	int char_written= snprintf(je_dir, BUF_SIZE, "%s/.local/share/je", xdg_data_home);
	assert(char_written < BUF_SIZE); // if attempted chars does not fit into buffer

	// concat je directory to je database file
	char je_gdbm_dir[BUF_SIZE];
	char_written = snprintf(je_gdbm_dir, BUF_SIZE, "%s/je.gdbm", je_dir);
	assert(char_written < BUF_SIZE);


	// create database directory
	int status = mkdir(je_dir, 0777);
	if (status == 0) {
		printf("Directory created: %s\n", je_dir);
	} else if (status == -1) {
		printf("Directory '%s' already exists\n", je_dir);
	} else {
		fprintf(stderr, "Error: File %s:%d could not make database directory\n",
				__FILE__, __LINE__);
		return 1;
	}
	
	
	// Set up database
	GDBM_FILE db = gdbm_open(je_gdbm_dir, 0, GDBM_WRCREAT, 0600, NULL);
	if(db == NULL) fprintf(stderr, "Can't open database: %s\n",
			gdbm_strerror(gdbm_errno));

	// handle commands
	switch(cmd) {
		case CMD_OTHER:  // check db for user commands
			
			datum jump_desc_key = { (void*)jump_desc, strlen(jump_desc) };
			datum fetched = gdbm_fetch(db, jump_desc_key);

			if (fetched.dptr == NULL) {
				if (gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "je: \'%s\' is not a je command. ", jump_desc);
					fprintf(stderr, "See 'je --help' or 'je list' for a list of user jumps\n");
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
				}
			} else {
				printf("jump command found: %s\n", jump_desc);
				printf("directory: %.*s\n", (int)fetched.dsize, fetched.dptr);
			}

			// I hate C
			free(fetched.dptr);



			break;

		case CMD_LIST:   // print list and directories

			datum firstkey = gdbm_firstkey(db);
			if (firstkey.dptr == NULL) {
				if(gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "je: No jump descripors in database. Use 'je add [descriptor]'\n");
					return 1;
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
					return 1;
				}
			}


			// extra room for null terminator
			char *keystr = malloc(firstkey.dsize + 1);
			memcpy(keystr, firstkey.dptr, firstkey.dsize);
			keystr[firstkey.dsize] = '\0'; // b/c it's index

			// fetched directory from key
			fetched = gdbm_fetch(db, firstkey); 
			char *dirstr = malloc(fetched.dsize + 1);
			memcpy(dirstr, fetched.dptr, fetched.dsize);
			dirstr[fetched.dsize] = '\0';

			printf("List of jump descriptors and paths\n");
			printf("Desc: %s :: Path: %s\n", keystr, dirstr);

			free(keystr);
			free(dirstr);

			datum nextkey = gdbm_nextkey(db, firstkey);

			free(firstkey.dptr);

			int count = 0;

			while (nextkey.dptr != NULL) {

				char *keystr = malloc(nextkey.dsize + 1);
				memcpy(keystr, nextkey.dptr, nextkey.dsize);
				keystr[nextkey.dsize] = '\0';

				fetched = gdbm_fetch(db, nextkey); 
				char *dirstr = malloc(fetched.dsize + 1);
				memcpy(dirstr, fetched.dptr, fetched.dsize);
				dirstr[fetched.dsize] = '\0';

				printf("Desc: %s :: Path: %s\n", keystr, dirstr);

				free(keystr);
				free(dirstr);
				free(fetched.dptr);

				datum oldkey = nextkey;

				nextkey = gdbm_nextkey(db, oldkey);

				free(oldkey.dptr);

				count++;
			}

			free(nextkey.dptr);


			break;

		case CMD_ADD:    // adds user command

			// if something was not written into it

			if (*arg2 == '\0' ) {
				fprintf(stderr, "could not add je command, no descriptor provided\n");
				return 1;
			}

			if (*arg3 == '\0' ) {
				fprintf(stderr, "could not add je command, no directory provided\n");
				return 1;
			}


			datum add_desc_key = { (void*)arg2, strlen(arg2) };
			datum add_desc_val = { (void*)arg3, strlen(arg3) };

			int store_return = gdbm_store(db, add_desc_key, add_desc_val, GDBM_INSERT);
			if (store_return == -1) {
				fprintf(stderr, "%s: could not store value into database\n", 
						gdbm_strerror(gdbm_errno));
			} else if (store_return == 1) {
				printf("je: Cound not add jump descriptor '%s' because it already exist. ", arg2);
				printf("Use 'je rm [descriptor]' first if you want to replace it\n");
			} else {
				printf("je: Storing descriptor '%s' with file path '%s' successful\n",
						arg2, arg3);
			}

			break;

		case CMD_REMOVE: // removes user command

			datum remove_desc_key = { (void*)arg2, strlen(arg2) };

			int delete_return = gdbm_delete(db, remove_desc_key);
			if (delete_return == -1) {
				fprintf(stderr, "je: Could not remove descriptor '%s', not found in database\n",
						arg2);
			} else {
				printf("je: Jump descriptor '%s' successfully removed\n", arg2);
			}


			break;

		case CMD_EDITOR: // set/change default editor
						
			break;

		case CMD_HELP:   // you know
						 
			break;

	}


	gdbm_close(db);
	return 0;

}
