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
#include <regex.h>
#include <gdbm.h>
#include <unistd.h> // chdir()

#define BUF_SIZE 1024

#if defined(__linux__)
	#define APP_DATA_DIR "/.local/share/je"
#elif defined(__APPLE__)
	#define APP_DATA_DIR "/Library/Application Support/je"
#endif

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
	if(!strcmp(buf, "default-editor"))  return CMD_EDITOR; 
	if(!strcmp(buf, "--help")) return CMD_HELP;
	return CMD_OTHER;
}

char *get_matches(const char *pattern, const char *string, int group_index, int n_groups) {
	regex_t regex;
	regmatch_t matches[n_groups + 1]; // Array for matches: [0] is whole match
	int ret;

	// compile regex
	ret = regcomp(&regex, pattern, REG_EXTENDED);
	if (ret) {
		fprintf(stderr, "Could not compile regex\n");
		return NULL;
	}

	ret = regexec(&regex, string, n_groups + 1, matches, 0);
	if (ret == REG_NOMATCH) {
		regfree(&regex);
		return NULL;
	} else if (ret != 0) {
		char errbuf[100];
		regerror(ret, &regex, errbuf, sizeof(errbuf));
		fprintf(stderr, "Regex match failed: %s\n", errbuf);
		regfree(&regex);
		return NULL;
	}

	if(matches[group_index].rm_so == -1) {
		fprintf(stderr, "Group does not exist\n");
		regfree(&regex);
		return NULL;
	}

	int start = matches[group_index].rm_so; //start of
	int end = matches[group_index].rm_eo;   // end of 

	size_t len = end - start;

	if (len <= 0) {
		fprintf(stderr, "invalid match length. end: %d, start %d\n", end, start);
		regfree(&regex);
		return NULL;
	}

	char *substr = malloc(len + 1);
	if (!substr) { 
		perror("malloc"); 
		regfree(&regex);
		return NULL;
	}

	strncpy(substr, string + start, len);
	substr[len] = '\0';

	regfree(&regex);
	return substr; // caller must free
}

int is_file(char *path) {

	struct stat st;

	if(stat(path, &st) < 0) {
		perror("stat");
		exit(1);
	}

	if (S_ISDIR(st.st_mode)) {
		return 0;
	} else if (S_ISREG(st.st_mode)) {
		return 1;
	} else {
		fprintf(stderr, "je: Error \n"
						" Path '%s' is not a valid file or directory\n",
						path
		);
		exit(1);
	}
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
	int char_written= snprintf(je_dir, BUF_SIZE, "%s%s", xdg_data_home, APP_DATA_DIR);
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

				// prepare jump path and shell dir for pattern matching
				char *valstr = malloc(fetched.dsize + 1);
				memcpy(valstr, fetched.dptr, fetched.dsize);
				valstr[fetched.dsize] = '\0';

				char *pattern = "^(.+):::(.+)$";
				char *pathstr = get_matches(pattern, valstr, 1, 2);
				char *dirstr = get_matches(pattern, valstr, 2, 2);

				// add quotes around each path as a guard against spacing
				// in path. Might as well just put quotes around every path
				// instead of checking if it has spaces for simplicity
				int needed = snprintf(NULL, 0, "\"%s\"", pathstr);
				char *quoted_pathstr = malloc(needed + 1);
				if (!quoted_pathstr) { perror("malloc"); exit(1); }
				snprintf(quoted_pathstr, needed + 1, "\"%s\"", pathstr);

				needed = snprintf(NULL, 0, "\"%s\"", dirstr);
				char *quoted_dirstr = malloc(needed + 1);
				if (!quoted_dirstr) { perror("malloc"); exit(1); }
				snprintf(quoted_dirstr, needed + 1, "\"%s\"", dirstr);

			
				// grab default editor from db
				datum default_editor_key = { (void*)"default-editor\0", 15 };
				datum fetched_editor = gdbm_fetch(db, default_editor_key);
				// free(default_editor_key.dptr);

				if (fetched_editor.dptr == NULL) {
					if (gdbm_errno == GDBM_ITEM_NOT_FOUND) {
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


				// calculate how many bytes we need for the next
				// snprintf. It's a more precise way of doing it
				// rather than just guessing a bigger buffer size.
				needed = snprintf(NULL, 0, "cd %s && %s %s", quoted_dirstr, default_editor, quoted_pathstr);
				if (needed < 0) { perror("malloc"); exit(1); }
				char run_cd_jump[needed + 1];
				snprintf(run_cd_jump, needed + 1, "cd %s && %s %s", quoted_dirstr, default_editor, quoted_pathstr);

				needed = snprintf(NULL, 0, "cd %s", quoted_dirstr);
				if (needed < 0) { perror("malloc"); exit(1); }
				char cd_only[needed + 1];
				snprintf(cd_only, needed + 1, "cd %s", quoted_dirstr);

				// open new shell
				// deprecated instead printing to stdout for bash shell to run
				
				/*
				execlp("bash", "bash",
						"-c",
						run_cd_jump, // editor_open_path does not cd into the path dir
						(char*)NULL
				);

				*/
		

				// this is read by the bash script and ran in the 
				// current shell
				if (!strcmp(arg2,"-j")) {

					printf("cd %s", quoted_dirstr);

				} else if (!strcmp(arg2, "-e")) {

					printf("%s %s", default_editor, quoted_pathstr);

				} else {

					printf("cd %s && %s %s\n", quoted_dirstr, default_editor, quoted_pathstr);

				}

				
				free(valstr);
				free(dirstr);
				free(pathstr);
				free(quoted_dirstr);
				free(quoted_pathstr);
				free(fetched_editor.dptr);
				free(default_editor);
			
			}

			// I love C
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

			printf("Default Editor: %s\n\n", default_editor);

			datum key = gdbm_firstkey(db);

			if (key.dptr == NULL) {
				if(gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "je: Error\n"
							" No default editor or jump descriptors in database.\n"
							" See 'je --help'\n");
					return 1;
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
					return 1;
				}
			}


			size_t num_desc = 0;
			int has_default_editor = 0;

			// maybe make this sort alphabetically later
			while (key.dptr != NULL) {

				
				/*
				 * because the datum's pointer holds the raw
				 * data without a null terminator, wee need to
				 * malloc room at the end for '\0' when storing
				*/
				char *keystr = malloc(key.dsize + 1);
				memcpy(keystr, key.dptr, key.dsize);
				keystr[key.dsize] = '\0';

				// fetched directory from key
				datum fetched = gdbm_fetch(db, key); 
				char *valstr = malloc(fetched.dsize + 1);
				memcpy(valstr, fetched.dptr, fetched.dsize);
				valstr[fetched.dsize] = '\0';

				/*
				 * since the jump path and the shell dir are stored in
				 * the database as one string ie "jump/path/somefile.c shell/dir"
				 * and separated by a space. We need to separate each path
				 * at the space and store them in their own variables
				 */
				char *pattern = "^(.+):::(.+)$"; // split the two paths at the space
				char *pathstr = get_matches(pattern, valstr, 1, 2);
				char *dirstr = get_matches(pattern, valstr, 2, 2);


				// because we are using the same database for storing the
				// default-editor we need to not display it like other
				// jump descriptors
				if(strcmp(keystr, "default-editor")) {
					num_desc++;
					printf("Descriptor: %s \n"
							" Jump Path: %s\n"
							" Shell Dir: %s\n\n"
							, keystr, pathstr, dirstr);
				} else {
					has_default_editor = 1;
				}

				free(keystr);
				free(pathstr);
				free(dirstr);
				free(fetched.dptr);

				datum oldkey = key;

				key = gdbm_nextkey(db, oldkey);

				free(oldkey.dptr);

			}
			
			// if there is a default editor but no added descriptors
			if(num_desc == 0 && has_default_editor) {
				printf("je: Error\n"
						" No jump descriptors in database.\n"
						" See 'je --help'\n");
			}

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


			char *dirstr = NULL;

			char *pathstr = strdup(arg3); // copies a valid null terminated string
			if (!pathstr) { perror("malloc"); exit(1); }


			/*
			 * if 
			 * there is a shell directory provided concat it to the jump path. 
			 * else if
			 * jump path is a file, store the shell directory as 
			 * the directory that holds the jump path file.
			 * else 
			 * jump path is a directory, make the shell directory 
			 * the same as jump path.
			 * 
			*/
			if(*arg4 != '\0') {	

				dirstr = strdup(arg4);
				if (!dirstr) { perror("malloc"); exit(1); }

			} else if (is_file(arg3)) {

				// match directory that file is in 
				char *pattern = ".*\\/";
				dirstr = get_matches(pattern, arg3, 0, 0);

			} else {

				dirstr = strdup(arg3);

			}


			// three colons '%s:::%s' separate the two paths in storage
			// this makes it less likely to have problems parsing 
			// paths on other operating systems that use spaces
			//
			// allocate exact buffer length for both strings
			int needed = snprintf(NULL, 0, "%s:::%s", pathstr, dirstr);
			char *temp_concat = malloc(needed + 1);
			if(!temp_concat) { perror("malloc"); exit(1); }
			snprintf(temp_concat, needed + 1, "%s:::%s", pathstr, dirstr);


			datum key = { 
				.dptr = (void*)arg2, 
				.dsize = strlen(arg2) 
			};

			datum val = {
				.dptr = (void*)temp_concat,
				.dsize = strlen(temp_concat),
			};

			int store_return = gdbm_store(db, key, val, GDBM_INSERT);


			if (store_return == -1) {
				fprintf(stderr, "%s: could not store value into database\n", 
						gdbm_strerror(gdbm_errno));
			} else if (store_return == 1) {
				printf("je: Error, cound not add jump descriptor '%s' because it already exist. "
						"Use 'je rm <descriptor>' first if you want to replace it\n",
						arg2);
			} else {
				printf("je: Success\n"
						" New Descriptor: '%s'\n"
						"      Jump Path: '%s'\n"
						"      Shell Dir: '%s'\n",
						arg2, pathstr, dirstr);
			}

			free(pathstr);
			if (dirstr) free(dirstr);
			free(temp_concat);

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
						 
			fprintf(stdout, "je (j)ump (e)dit help page\n\n"
					"Usage:\n"
					"   je <desc> .................... jump to user (desc)riptor jump path and \n"
					"                                  open editor.\n\n"
					"   je add <desc> <path> <shdir> . (add)s user (desc)riptor and jump (path)\n" 
					"                                  optional (sh)ell (dir)ectory, see below.\n\n"
					"   je rm  <desc> ................ (r)e(m)oves a user jump (desc)riptor\n\n"
					"   je default-editor <editor> ... specifies default editor\n" 
					"                                  when opening paths.\n\n"
					"   je list ...................... displays jump descriptor (list) with\n" 
					"                                  their paths and shell directories.\n\n"
					"   je --help .................... prints (help).\n\n"
					"Description:\n"
					"   - je (j)ump (e)dit allows user to save a jump (path) to an \n"
					"     alias a.k.a (desc)riptor\n\n"
					"   - A (desc)riptor has two components, a jump (path), and \n"
					"     a (sh)ell (dir)ectory\n\n" 
					"   - The jump (path) can point to a file or directory and\n"
					"     tells (j)ump (e)dit where to open the file or\n" 
					"     directory using the default editor\n\n"
					"   - je works by opening a new shell on top of the shell je was\n"
					"     called from. Because of this, the user has the option to \n"
					"     specify a (sh)ell (dir)ectory that will be the new shell's\n"
					"     current working directory.\n\n"
					"   - If no (shdir) is specified, je will infer the directory\n"
					"     in two ways\n"
					"        1) if jump (path) is a directory, (shdir) will be\n" 
					"           the same directory\n"
					"        2) if jump (path) is a file, (shdir) will be the \n"
					"           directory file is in\n\n"
					"   - User must specify a default editor, which will be \n"
					"     used by je to open all jump (path)s\n\n"
					"Important Information:\n"
					"   - As soon as user exits the editor, user will return\n"
					"     to the previous shell\n\n"
					"   - je was build for max typing efficiency, thus the base\n"
					"     command 'je <desc>' will be blocked by any sub \n"
					"     commands i.e.(list, add, rm, ...). This means user \n"
					"     can not name any user descriptors a name that is \n"
					"     already a je sub command \n\n"
					"Happy Jump Editing\n"
			);

			break;

		}
	}


	gdbm_close(db);
	return (EXIT_SUCCESS);

}
