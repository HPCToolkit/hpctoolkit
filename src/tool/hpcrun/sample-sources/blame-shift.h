#ifndef blame_shift_h
#define blame_shift_h

#include <stdint.h>

#include <cct/cct.h>

typedef uint64_t (*bs_tfn_t)();

typedef void (*bs_fn_t)(int metric_id, cct_node_t *node, int metric_incr);


typedef struct bs_fn_entry_s {
 struct bs_fn_entry_s *next;
 bs_fn_t fn;
} bs_fn_entry_t;


typedef struct bs_tfn_entry_s {
 struct bs_tfn_entry_s *next;
 bs_tfn_t fn;
} bs_tfn_entry_t;


typedef enum bs_heartbeat_t {
  bs_heartbeat_timer,
  bs_heartbeat_cycles
} bs_heartbeat_t;


typedef enum bs_type_t {
  bs_type_directed = 0,
  bs_type_undirected = 1
} bs_type_t;


void blame_shift_target_register(bs_tfn_entry_t *entry);
void blame_shift_register(bs_fn_entry_t *entry, bs_type_t bt);

void blame_shift_apply(int metric_id, cct_node_t *node, int metric_incr);
void blame_shift_heartbeat_register(bs_heartbeat_t bst);
int blame_shift_heartbeat_available(bs_heartbeat_t bst);

void blame_shift_target_allow();

void register_blame_shift();
void register_lock();
#endif
