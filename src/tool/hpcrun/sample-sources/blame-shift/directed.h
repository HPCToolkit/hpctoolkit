#ifndef __directed_h__
#define __directed_h__

#include <stdint.h>

#include "sample-sources/blame-shift/blame-map.h"


/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

typedef uint64_t (*directed_blame_target_fn)(void);

typedef struct directed_blame_info_t {
  directed_blame_target_fn get_blame_target;
  blame_entry_t* blame_table;

  int wait_metric_id;
  int blame_metric_id;

  int levels_to_skip;
} directed_blame_info_t;


void directed_blame_shift_start(void *arg, uint64_t obj) ;

void directed_blame_shift_end(void *arg);

void directed_blame_sample(void *arg, int metric_id, cct_node_t *node, 
                           int metric_incr);

void directed_blame_accept(void *arg, uint64_t obj);


#endif // __directed_h__
