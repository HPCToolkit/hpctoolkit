#ifndef blame_shift_h
#define blame_shift_h

#include <cct/cct.h>

typedef void (*bs_fn_t)(int metric_id, cct_node_t *node, int metric_incr);

typedef struct bs_fn_entry_s {
 struct bs_fn_entry_s *next;
 bs_fn_t fn;
} bs_fn_entry_t;


void blame_shift_register(bs_fn_entry_t *entry);
void blame_shift_apply(int metric_id, cct_node_t *node, int metric_incr);

#endif
