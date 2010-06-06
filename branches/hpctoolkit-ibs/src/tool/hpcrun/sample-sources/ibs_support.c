/*Support for buliding a splay tree for address intervals*/

#include <messages/messages.h>
#include <sample_event.h>
#include "splay.h"
#include "splay-interval.h"
#include "ibs_init.h"

static struct splay_interval_s *root;

int insert_splay_tree(interval_tree_node* node,  void* start, size_t size, int32_t id)
{
  interval_tree_node* dummy;
 
  memset(node, 0, sizeof(interval_tree_node));
  node->start = start;
  node->end = (void *)((unsigned long)start+size);
  node->cct_id = id;
  /*first delete and then insert*/
  interval_tree_delete(&root, &dummy, node->start, node->end);
  if(interval_tree_insert(&root, node)!=0)
    return -1;//insert fail
  TMSG(IBS_SAMPLE, "splay insert: start(%p), end(%p)", node->start, node->end);
  return 0;
}
 
interval_tree_node* splaytree_lookup(void* p)
{
  interval_tree_node* result_node = hpcrun_malloc(sizeof(interval_tree_node));
  result_node = interval_tree_lookup(&root, p);
  return result_node;
}
                                                     
