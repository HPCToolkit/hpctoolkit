#ifndef _IBS_INIT
#define _IBS_INIT
#include "splay-interval.h"
/*this is used to check whether hpcrun is init*/
inline bool hpcrun_is_initialized();

/*this is used to lookup addr in addr splay tree*/
struct splay_interval_s* splaytree_lookup(void*); //cannot use interval_tree_node* type (error generated)

/*global variable*/
static struct splay_interval_s *root;

#endif
