#ifndef _IBS_INIT
#define _IBS_INIT
#include "splay-interval.h"
#include "splay.h"
#include <cct/cct.h>
/*this is used to check whether hpcrun is init*/
//inline bool hpcrun_is_initialized();

/*create static data table*/
//void bss_partition(int);

int hpcrun_dc_malloc_id();
int hpcrun_dc_calloc_id();
int hpcrun_dc_realloc_id();

int hpcrun_datacentric_active();

/*this is used to lookup addr in addr splay tree*/
struct splay_interval_s* splaytree_lookup(void*); //cannot use interval_tree_node* type (error generated)
int insert_splay_tree(interval_tree_node* node,  void* start, size_t size, int32_t id);

/*global variable*/
//static struct splay_interval_s *root;

/*epoch bit for use-reuse*/
bool is_use_reuse;

#endif
