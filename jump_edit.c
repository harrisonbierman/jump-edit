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
#include <unistd.h> 
#include "include/arg_parser.h"

#define BUF_SIZE 1024
#define SEE_HELP "See 'je -h' or 'je --help' for more information\n"

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
	if(!strcmp(buf, "super-duper-help-page-yah")) return CMD_HELP;
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
		fprintf(stderr, "Error: Invalid match length. end: %d, start %d\n", end, start);
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
		fprintf(stderr, "Error: Path '%s' is not a valid file or directory\n", path); 
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	
	// arg1 will be a sub_command or a jump descriptor
	Cmd cmd;

	// create argument parse tree
	struct ap_arg *head = NULL;
	int rc = AP_parse(argc, argv, &head);
	if (rc != 0) {
		perror("parsing error");
		exit(EXIT_FAILURE);
	}
	size_t num_args = AP_len(head);

	struct ap_arg *sub_command = AP_get(head, 1);

		if (sub_command != NULL) {
			cmd = parse_cmd(sub_command->str);
		}

		// parse sub_command
		if (sub_command == NULL) {
			if (AP_has_flag(head, "-h", "--help")) {
					cmd = parse_cmd("super-duper-help-page-yah");
			} else if (AP_has_flag(head, NULL, NULL)){
				fprintf(stderr, "Error: je option(s) not found\n" SEE_HELP);
				exit(EXIT_FAILURE);
			} else {
				fprintf(stderr, "Error: no sub command or option(s) provided\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}

		}

	// Create/check persistence file path
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home == NULL){
		xdg_data_home = getenv("HOME");
		if (xdg_data_home == NULL) {
			fprintf(stderr, "Error: neither XDG_DATA_HOME nor HOME is set\n");
			exit(EXIT_FAILURE);
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
		fprintf(stderr, "Error: File %s:%d could not make database directory\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	
	
	// Set up database
	GDBM_FILE db = gdbm_open(je_gdbm_dir, 0, GDBM_WRCREAT, 0600, NULL);
	if(db == NULL) fprintf(stderr, "Can't open database: %s\n",
			gdbm_strerror(gdbm_errno));

	// handle commands
	switch(cmd) {
		case CMD_OTHER: { // check db for user commands

			struct ap_arg *label = AP_get(head, 1);

			if (num_args > 2) {
				fprintf(stderr, "Error: too many arguments\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}

			datum label_key;
				label_key.dptr = (void*)label->str; 
				label_key.dsize = strlen(label->str);

			datum fetched = gdbm_fetch(db, label_key);

			if (fetched.dptr == NULL) {
				if (gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "Error: \'%s\' is not a je label.\n", label->str);
					fprintf(stderr, "See 'je list' for a list of user jumps\n" SEE_HELP);
					exit(EXIT_FAILURE);
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
					exit(EXIT_FAILURE);
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
						fprintf(stderr, "Error: Could not run command because a default editor has not been set. ");
						fprintf(stderr, "use 'je default-editor [editor command]' to set\n");
						exit(EXIT_FAILURE);
					} else {
						fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
						exit(EXIT_FAILURE);
					}
				}


				size_t ed_len = fetched_editor.dsize;
				char *default_editor = malloc(ed_len + 1);
				if(!default_editor) {perror("malloc"); exit(1); }
				memcpy(default_editor, fetched_editor.dptr, ed_len);
				default_editor[ed_len] = '\0';


				// stdout will be read by bash script and executed 
				struct ap_arg *je = AP_get(head, 0);

				if (AP_has_flag(je, "-j", "--jump")) {

					printf("cd %s\n", quoted_dirstr);

				} else if (AP_has_flag(je, "-e", "--edit")) {

					printf("%s %s\n", default_editor, quoted_pathstr);

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
			
			struct ap_arg *list = AP_get(head, 1);
			if(list == NULL) {
				fprintf(stderr, "Error: out of bounds, %s %d\n", __FILE__, __LINE__);
				exit(EXIT_FAILURE);
			}

			size_t len = AP_len(head);
			if (len > 2) {
				fprintf(stderr, "Error: too many arguments\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}
			
			// need to display current default editor at the top
			// dont need to add null terminator because it was stored with one
			datum default_editor_key = { (void*)"default-editor\0", 15 };
			datum editor_fetched = gdbm_fetch(db, default_editor_key);
			char *default_editor = malloc(editor_fetched.dsize);
			memcpy(default_editor, editor_fetched.dptr, editor_fetched.dsize);

			printf("(L = Label), (JP = Jump Path), (SD = Shell Directory)\n");
			printf("Default Editor: %s\n\n", default_editor);

			datum key = gdbm_firstkey(db);

			if (key.dptr == NULL) {
				if(gdbm_errno == GDBM_ITEM_NOT_FOUND) {
					fprintf(stderr, "Error: No default editor or jump labels in database.\n" SEE_HELP);
					exit(EXIT_FAILURE);
				} else {
					fprintf(stderr, "Error: %s\n", gdbm_db_strerror(db));
					exit(EXIT_FAILURE);
				}
			}


			size_t num_label = 0;
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
				if(!strcmp(keystr, "default-editor")) {
					has_default_editor = 1;
				} else {
					num_label++;
					if(AP_has_flag(list, "-l", "--label")) {
						printf("%s, ", keystr);
					} else if (AP_has_flag(list, "-j", "--jump")) {
						printf("L: %s | JP: %s\n", keystr, pathstr);
					} else if (AP_has_flag(list, "-d", "--directory")) {
						printf("L: %s | SD: %s\n", keystr, dirstr);
					} else if (AP_has_flag(list, NULL, NULL)) { // if any other flag is present 
						fprintf(stderr, "Error: option(s) for list not found\n");
						exit(EXIT_FAILURE);
					} else if (!AP_has_flag(list, NULL, NULL)) { // if no flags are present
					printf("L: %s \n"
						   "├JP: %s\n"
						   "└SD: %s\n\n"
							, keystr, pathstr, dirstr);
					}
				}

				free(keystr);
				free(pathstr);
				free(dirstr);
				free(fetched.dptr);

				datum oldkey = key;

				key = gdbm_nextkey(db, oldkey);

				free(oldkey.dptr);

			}

			// extra new line so that things line up for this option
			if(AP_has_flag(list, "-l", "--label")) {
				printf("\n\n");
			}
			
			// if there is a default editor but no added labels 
			if(num_label == 0 && has_default_editor) {
				printf("je: Error\n"
						" No jump labels in database.\n"
						" See 'je --help'\n");
			}

			break;
		}

		case CMD_ADD: {   // adds user command
			
			struct ap_arg *add = AP_get(head, 1);
			struct ap_arg *label = AP_get(head, 2);
			struct ap_arg *path = AP_get(head, 3);
			struct ap_arg *dir = AP_get(head, 4);

			if (num_args > 5) {
				fprintf(stderr, "Error: too many arguments\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}

			// if something was not written after add command 
			if (label == NULL) {
				fprintf(stderr, "Error: could not add je command, no label provided\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}

			if (path == NULL) {
				fprintf(stderr, "Error: cound not add je command, no path provided\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}


			char *dirstr = NULL;

			/*
			*/
			if(dir != NULL) {	

				dirstr = strdup(dir->str);
				if (!dirstr) { perror("malloc"); exit(1); }

			} else if (is_file(path->str)) {

				// extract directory that file is in
				char *pattern = ".*\\/";
				dirstr = get_matches(pattern, path->str, 0, 0);

			} else {

				dirstr = strdup(path->str);

			}


			// three colons '%s:::%s' separate the two paths in storage
			// this makes it less likely to have problems parsing 
			// paths on other operating systems that use spaces
			//
			// allocate exact buffer length for both strings
			int needed = snprintf(NULL, 0, "%s:::%s", path->str, dirstr);
			char *temp_concat = malloc(needed + 1);
			if(!temp_concat) { perror("malloc"); exit(1); }
			snprintf(temp_concat, needed + 1, "%s:::%s", path->str, dirstr);

			datum key = { 
				.dptr = (void*)label->str, 
				.dsize = strlen(label->str) 
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
				printf("Error: cound not add jump label '%s' because it already exist. "
						"Use 'je rm <label>' first if you want to replace it\n",
						label->str);
			} else {
				printf("Success\n"
						" New Label: '%s'\n"
						" Jump Path: '%s'\n"
						" Shell Dir: '%s'\n",
						label->str, path->str, dirstr);
			}

			if (dirstr) free(dirstr);
			free(temp_concat);

			break;
		}

		case CMD_REMOVE: {// removes user command
			struct ap_arg *remove = AP_get(head, 1);
			struct ap_arg *label = AP_get(head, 2);

			if (num_args > 3) {
				fprintf(stderr, "Error: too many arguments\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}

			datum label_key = { (void*)label->str, strlen(label->str) };

			int delete_return = gdbm_delete(db, label_key);
			if (delete_return == -1) {
				fprintf(stderr, "Error: could not remove label '%s', not found in database\n",
						label->str);
			} else {
				printf("Success: jump label '%s' removed\n", label->str);
			}

			break;

		}

		case CMD_EDITOR: { // set/change default editor

			struct ap_arg *editor = AP_get(head, 2);

			if (num_args > 3) {
				fprintf(stderr, "Error: too many arguments\n" SEE_HELP);
				exit(EXIT_FAILURE);
			}

			datum default_editor_key = { (void*)"default-editor\0", 15 };
			datum default_editor_val = { (void*)editor->str, strlen(editor->str) };

			int store_return = gdbm_store(db, default_editor_key, default_editor_val, GDBM_REPLACE);
			if (store_return == -1) {
				fprintf(stderr, "%s: could not store value into database\n", 
						gdbm_strerror(gdbm_errno));
			} else {
				printf("Success: saving '%s' as default editor\n", editor->str);
			}

			break;

		}

		case CMD_HELP: { // you know
						 
			fprintf(stdout, "je (j)ump (e)dit help page\n\n"
					"Usage:\n"
					"   je [-j|-e] <label> ........... jump to labeled jump path and open editor\n"
					"                                  see example (5).\n"
					"      -j | --jump ............... [jump] only jump to label directory.\n"
					"      -e | --edit ............... [edit] only edit at label path.\n\n"
					"   je add <label> <path> <dir> .  adds user label and jump path with optional\n"
					"                                  shell directory. See description (4).\n\n"
					"   je rm  <label> ............... removes a user jump label.\n\n"
					"   je default-editor <editor> ... specifies default editor\n" 
					"                                  when opening paths.\n\n"
					"   je list [-l|-j|-d]............ displays labels with jumps and directories\n" 
					"      -l | --label ...............[label] labels in oneline\n"
					"      -j | --jump ................[jump] only labels with jump\n"
					"      -d | --directory ...........[directory] only labels with directories\n\n"
					"   je --help .................... prints help.\n\n"
					"Description:\n"
					"   1) je (jump edit) allows user to save a jump path to an \n"
					"     alias, a.k.a a label.\n\n"
					"   2) A label has two components, a jump path, and a shell directory.\n\n"
					"   3) The jump path can point to a file or a directory, which tells je\n"
					"     where to open the file or directory using the default editor.\n\n" 
					"   4) If no shell directory is specified, je will infer\n"
					"     the directory in two ways\n"
					"        a) if jump path is a file, the shell directory will be the \n"
					"           same directory the file is in. See example (2)\n"
					"        b) if jump path is a directory, the shell directory\n" 
					"           will be the same as the jump path. See example (3)\n\n"
					"   5) A User might want to set the shell directory to their project root\n"
					"      directory so that 'things' work as expected while editing.\n"
					"      See example (4).\n\n"
					"   6) User must specify a default editor, which will be \n"
					"     used by je to open all jump paths. See example (1).\n\n"
					"Examples\n"
					"   1) Add your editor of choice as default\n\n"
					"      'je default-editor vim'\n"
					"      'je default-editor nvim'\n"
					"      'je default-editor code'\n\n"
					"   2) Add ~/.bashrc file as path, je will infer the shell directory\n"
					"      as the home '~/' directory that .bashrc is in\n\n"
					"      'je add bash ~/.bashrc'\n\n"
					"   3) Add ~/.local/ directory as path, je will infer the shell\n"
					"      directory as the same directory\n\n"
					"      'je add loc ~/.local'\n\n"
					"   4) Add main.c as jump path and myproj-root as the shell directory\n\n"
					"      'je add myproj ~/c-programs/myproj-root/src/main.c ~/c-programs/myproj-root/'\n\n"
					"   5) Use myproj label with 3 options\n\n"
					"      'je myproj' // cd to label shell directory and open editor from jump path\n"
					"      'je -j myproj' // only cd to label shell directory does not open editor\n"
					"      'je -e myproj' // only opens editor, does not change shell directory\n\n"
					"   6) Display what jump labels user has added\n\n"
					"      'je list'\n\n"
					"   7) Remove label user does not want anymore\n\n"
					"      'je rm .bashrc\n"
					"      'je rm myproj\n\n"
					"Important Information:\n"
					"   - je was build for max typing efficiency, thus the base\n"
					"     command 'je <label>' will be blocked by any sub \n"
					"     commands i.e.(list, add, rm, ...). This means user \n"
					"     can not name any labels a name that is \n"
					"     already a je sub command \n\n"
					"Happy Jump Editing\n"
			);

			break;

		}
	}


	AP_free(head);
	gdbm_close(db);
	return (EXIT_SUCCESS);

}
