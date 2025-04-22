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
#include <unistd.h> // chdir()

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
	if(!strcmp(buf, "de"))  return CMD_EDITOR; 
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
	char arg4[BUF_SIZE];        arg4[0] = '\0';

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

		// add functionality to add as many arguments as I want
		if (i == 4) {
			memcpy(arg4, buf, BUF_SIZE);
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

	// concat je directory to je.gdbm database file
	char je_gdbm_dir[BUF_SIZE];
	char_written = snprintf(je_gdbm_dir, BUF_SIZE, "%s/je.gdbm", je_dir);
	assert(char_written < BUF_SIZE);


	// create database directory
	int status = mkdir(je_dir, 0777);
	if (status == 0) {
		printf("Directory created: %s\n", je_dir);
	} else if (status == -1) {
		// printf("Directory '%s' already exists\n", je_dir);
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
		case CMD_OTHER: { // check db for user commands
			
			datum jump_desc_key = { (void*)jump_desc, strlen(jump_desc) };
			datum fetched = gdbm_fetch(db, jump_desc_key);

			if (fetched.dptr == NULL) {
				if (gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "je: \'%s\' is not a je command. ", jump_desc);
					fprintf(stderr, "See 'je --help' or 'je list' for a list of user jumps\n");
					return 1;
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
					return 1;
				}
			} else {

				char *path = malloc(fetched.dsize + 1);
				memcpy(path, fetched.dptr, fetched.dsize);
				path[fetched.dsize] = '\0';


				// grab default editor from db
				datum default_editor_key = { (void*)"default-editor\0", 15 };

				datum fetched_editor = gdbm_fetch(db, default_editor_key);

				if(fetched_editor.dptr == NULL) {
					if(gdbm_errno == GDBM_ITEM_NOT_FOUND) {
						fprintf(stderr, "je: Could not run command because a default editor has not been set. ");
						fprintf(stderr, "use 'je default-editor [editor command]' to set\n");
						return 1;
					} else {
						fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
						return 1;
					}
				}


				size_t ed_len = fetched_editor.dsize;
				char *default_editor = malloc(ed_len + 1);
				if(!default_editor) {perror("malloc"); exit(1); }
				memcpy(default_editor, fetched_editor.dptr, ed_len);
				default_editor[ed_len] = '\0';


				// important to close db here or else it will continue 
				// to be open while editing a jump path. If db
				// stays open, other instances cannot write to the db.
				gdbm_close(db);


				// calculate how many bytes we need for the next
				// snprintf. It's a more precise way of doing it
				// rather than just guessing a bigger buffer size.
				int needed = snprintf(NULL, 0, "%s %s", default_editor, path);
				if (needed < 0) { perror("malloc"); exit(1); }

				char *command = malloc(needed + 1);
				if (!command) { perror("malloc"); exit(1); }

				char editor_open_path[needed + 1];

				// if a project root was enabled it will cd the new shell there
				if (arg4[0] != '\0') {
					printf("Project Root Dir: %s", arg4);
				}

				snprintf(editor_open_path, needed + 1, "%s %s", default_editor, path);

				printf("je: jump: %s | editor: %s\n", jump_desc, default_editor);
				fflush(stdout); // force write I/O buffer
				

				needed = snprintf(NULL, 0, "cd %s && %s %s", path, default_editor, path);
				if (needed < 0) { perror("malloc"); exit(1); }

				char new_shell_cmd[needed + 1];

				snprintf(new_shell_cmd, needed + 1, "cd %s && %s %s", path, default_editor, path);

				// need to figure out a way to detect if the path 
				// given is a file or directory and also maybe add
				// a way to add a root project directory if needed for
				// saving purposes.
				execlp("bash", "bash",
						"-c",
						editor_open_path, // editor_open_path does not cd into the path dir
						(char*)NULL);
				
				free(default_editor_key.dptr);
				free(default_editor);
			}

			// I hate C
			free(fetched.dptr);


			break;
		}

		case CMD_LIST: {   // print list and directories
			
			// need to display current default editor at the top
			// dont need to add null terminator because it was stored with one
			datum default_editor_key = { (void*)"default-editor\0", 15 };
			datum editor_fetched = gdbm_fetch(db, default_editor_key);
			char *default_editor = malloc(editor_fetched.dsize);
			memcpy(default_editor, editor_fetched.dptr, editor_fetched.dsize);

			printf("List of jump descriptors and paths\n\n");
			printf("Default Editor: %s\n\n", default_editor);


			datum firstkey = gdbm_firstkey(db);
			if (firstkey.dptr == NULL) {
				if(gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "je: No jump descriptors in database. Use 'je add [descriptor]'\n");
					return 1;
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
					return 1;
				}
			}



			/*
			 * because the datum's pointer holds the raw
			 * data without a null terminator, wee need to
			 * malloc room at the end for '\0' when storing
			*/
			char *keystr = malloc(firstkey.dsize + 1);
			memcpy(keystr, firstkey.dptr, firstkey.dsize);
			keystr[firstkey.dsize] = '\0'; // b/c it's index

			// fetched directory from key
			datum fetched = gdbm_fetch(db, firstkey); 
			char *dirstr = malloc(fetched.dsize + 1);
			memcpy(dirstr, fetched.dptr, fetched.dsize);
			dirstr[fetched.dsize] = '\0';

			// because we are using the same database for storing the
			// default-editor we need to not display it like other
			// jump descriptors
			if(strcmp(keystr, "default-editor")) {
				printf("Desc: %s :: Path: %s\n", keystr, dirstr);
			}

			datum nextkey = gdbm_nextkey(db, firstkey);

			free(keystr);
			free(dirstr);
			free(firstkey.dptr);
			free(fetched.dptr);


			// maybe make this sort alphabetically later
			while (nextkey.dptr != NULL) {

				char *keystr = malloc(nextkey.dsize + 1);
				memcpy(keystr, nextkey.dptr, nextkey.dsize);
				keystr[nextkey.dsize] = '\0';

				datum fetched = gdbm_fetch(db, nextkey); 
				char *dirstr = malloc(fetched.dsize + 1);
				memcpy(dirstr, fetched.dptr, fetched.dsize);
				dirstr[fetched.dsize] = '\0';

				if(strcmp(keystr, "default-editor")) {
					printf("Desc: %s :: Path: %s\n", keystr, dirstr);
				}

				free(keystr);
				free(dirstr);
				free(fetched.dptr);

				datum oldkey = nextkey;

				nextkey = gdbm_nextkey(db, oldkey);

				free(oldkey.dptr);

			}

			free(nextkey.dptr);

			break;
		}

		case CMD_ADD: {   // adds user command

			// if something was not written into it

			if (*arg2 == '\0' ) {
				fprintf(stderr, "could not add je command, no descriptor provided\n");
				return 1;
			}

			if (*arg3 == '\0' ) {
				fprintf(stderr, "could not add je command, no directory provided\n");
				return 1;
			}


			datum add_desc_key = { 
				.dptr = (void*)arg2, 
				.dsize = strlen(arg2) 
			};
			datum add_desc_val;

			char *temp_concat = NULL; // temp pointer for malloc'd concat

			// if there is a root project directory provided concat it to the
			// value to be parsed later when pull out of database
			if(*arg4 != '\0') {	

				// allocate exact buffer length for both strings
				int needed = snprintf(NULL, 0, "%s %s", arg3, arg4);

				temp_concat = malloc(needed + 1);
				if(!temp_concat) { perror("malloc"); exit(1); }

				snprintf(temp_concat, needed + 1, "%s %s", arg3, arg4);

				add_desc_val.dptr = (void*)temp_concat;
				add_desc_val.dsize = strlen(temp_concat);

			} else {

				add_desc_val.dptr = (void*)arg3;
				add_desc_val.dsize = strlen(arg3);

			}

			int store_return = gdbm_store(db, add_desc_key, add_desc_val, GDBM_INSERT);

			if(temp_concat) free(temp_concat);

			if (store_return == -1) {
				fprintf(stderr, "%s: could not store value into database\n", 
						gdbm_strerror(gdbm_errno));
			} else if (store_return == 1) {
				printf("je: Error, cound not add jump descriptor '%s' because it already exist. "
						"Use 'je rm [descriptor]' first if you want to replace it\n",
						arg2);
			} else if (*arg4 == '\0') {
				printf("\nje: Success\n"
						" New Descriptor:.'%s'\n"
						" Jump Path:......'%s'\n"
						" Project Root:...'none'\n",
						arg2, arg3);
			} else {
				printf("\nje: Success\n"
						" New Descriptor:.'%s'\n"
						" Jump Path:......'%s'\n"
						" Project Root:...'%s'\n",
						arg2, arg3, arg4);
			}

			break;
		}

		case CMD_REMOVE: {// removes user command

			datum remove_desc_key = { (void*)arg2, strlen(arg2) };

			int delete_return = gdbm_delete(db, remove_desc_key);
			if (delete_return == -1) {
				fprintf(stderr, "je: Error, could not remove descriptor '%s', not found in database\n",
						arg2);
			} else {
				printf("je: Success, jump descriptor '%s' removed\n", arg2);
			}

			break;

		}

		case CMD_EDITOR: { // set/change default editor
			// should I make another db to just hold the editor
			// or should I just make another unique command 
			// and store it into the db 

			datum default_editor_key = { (void*)"default-editor\0", 15 };
			datum default_editor_val = { (void*)arg2, strlen(arg2) };

			int store_return = gdbm_store(db, default_editor_key, default_editor_val, GDBM_REPLACE);
			if (store_return == -1) {
				fprintf(stderr, "%s: could not store value into database\n", 
						gdbm_strerror(gdbm_errno));
			} else {
				printf("je: Success, saving '%s' as default editor\n", arg2);
			}




			break;

		}

		case CMD_HELP: { // you know
						 
			fprintf(stdout, "Welcome to je (J)ump (E)dit\n\n"
					"Usage:\n"
					"   je <desc>             Jump to user (desc)riptor path and open editor\n"
					"   je add <desc> <path>  (Add) a user jump (desc)riptor and specify (path)\n"
					"   je rm  <desc>         (Re)moves a user jump (desc)riptor\n"
					"   je de  <editor>       Specifies (d)efault (e)ditor when opening paths\n"
					"   je list               Displays jump descriptor (list) and their paths\n"
					"   je --help             Prints help\n\n"
					"Description:\n"
					"   je (Jump Edit) allows you to save a file path and a corresponding \n"
					"   user made descriptor. A descriptor is a user made name to identify \n" 
					"   a specific path. je needs you to specify a default-editor \n"
					"   to open your path. After you have chosen a default-editor, you can \n"
					"   jump to any saved paths and je will open that path in your default-editor \n\n"
					"Important Information:\n"
					"   - je opens a new shell on top of your previous shell. \n"
					"     As soon as you exit the editor you will return to the previous shell\n"
					"   - je was build for max typing efficiency, thus the base command 'je <desc>' \n"
					"     will be blocked by any sub command i.e.(list, add, rm, ...). This means you \n"
					"     can not name any user descriptors a name that is already a je sub command \n"
					"Happy Jumping!\n"
			);

			break;

		}
	}


	gdbm_close(db);
	return 0;

}
