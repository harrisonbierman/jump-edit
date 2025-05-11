/**
 * Argument Parsing API for CLI tools
 */

#include "../include/arg_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int AP_parse(int tokc, char *tokv[], struct ap_arg **out_head) {
	int error = 0;

	// I need to remember that you can initiate
	// pointers as NULL if you don't want to use
	// them yet
	struct ap_arg *head = NULL, *tail = NULL;

	// each argument
	for (int i = 0; i < tokc;) {

		// calloc will zero out allocation 
		struct ap_arg *node = calloc(1, sizeof(struct ap_arg));

		node->next = NULL; // terminate by default

		// regular argument
		node->str = tokv[i];	

		i++; // move to next token

		// collect all flags after arg
		int count = 0;
		while (i < tokc && tokv[i][0] == '-' && count < MAX_FLAGS) {;
			node->flagv[count++] = tokv[i++];
		}

		node->flagc = count;  
		node->flagv[count] = NULL;

		// better solution than I had last commit
		if(!head) {
			// tail and head will be the same
			// pointer of there is no other arguments
			head = tail = node;
		} else {
			tail->next = node;
			tail = node;
		}

	}

	*out_head = head;
	return(error); // use AP_free();
}

void AP_free(struct ap_arg *head) {
	struct ap_arg *curr = head;
	while(curr){
		struct ap_arg *next = curr->next;
		free(curr);
		curr = next;
	}
}

struct ap_arg* AP_get(struct ap_arg *head, size_t element) {
	size_t i = 0;
	AP_FOREACH(curr, head) {
		if(i == element) 
			return curr;
		i++;
	}
	return NULL;
}

int AP_has_flag(struct ap_arg *arg, char *flag_short, char *flag_long) {

	// if inside loop means arg has flags
	for(int i = 0; i < arg->flagc; i++) {

		if (flag_short != NULL && !strcmp(arg->flagv[i], flag_short)) {
			return 1;
		}

		if (flag_long != NULL && !strcmp(arg->flagv[i], flag_long)) {
			return 1;
		}

		if(flag_short == NULL && flag_long == NULL) {
			return 1;
		}

		return 0;
	}

	return 0;
}

size_t AP_len(struct ap_arg *head) {
	size_t count = 0;
	AP_FOREACH(cur, head) {
		count++;
	}
	return count;
}


/**
 * notes:
 *
 * API functions I want 
 *
 * struct ap_arg ap = AP_new(argc, &argv);
 *
 *
 *
 *
 * switch(ap.arg[1]) {
 *		case(
 * }
 *
 * 
 * if (AP_has_opt(ap.arg[1], "--all", "-a")) {
 *		// handle option
 * }
 *
 * // can add NULL is there is no short or long option
 * if (AP_has_opt(ap.arg[2], "--oneline", NULL)) {
 *		// handle option
 * }
 *
 */ 

