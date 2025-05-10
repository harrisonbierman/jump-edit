#ifndef ARG_PARSER_H
#define ARG_PARSER_H
#include <stdlib.h> // size_t


#define MAX_FLAGS 16

struct ap_arg{

	char *str;
	struct ap_arg *next;

	int flagc;
	char *flagv[MAX_FLAGS + 1]; // room for NULL terminate
};


int AP_parse(int tokc, char *tokv[], struct ap_arg **out_head); // use AP_free()

void AP_free(struct ap_arg *head);

#define AP_FOREACH(node, head) \
	for (struct ap_arg *node = (head); node != NULL; node = node->next)

// do not need to free pointer taken care of by AP_free
struct ap_arg* AP_get(struct ap_arg *head, size_t element);

int AP_has_flag(struct ap_arg *arg, char *flag_short, char *flag_long);

size_t AP_len(struct ap_arg *head);

#endif
