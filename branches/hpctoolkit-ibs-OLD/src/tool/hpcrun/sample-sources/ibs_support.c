/*Support for buliding a splay tree for address intervals*/

#include <messages/messages.h>
#include <sample_event.h>
#include "splay.h"
#include "splay-interval.h"
#include "ibs_init.h"
#include <lib/prof-lean/spinlock.h>

static struct splay_interval_s *root;
spinlock_t splaylock = SPINLOCK_UNLOCKED;

int insert_splay_tree(interval_tree_node* node,  void* start, size_t size, int32_t id)
{
  spinlock_lock(&splaylock);
  interval_tree_node* dummy = NULL;
 
  memset(node, 0, sizeof(interval_tree_node));
  node->start = start;
  node->end = (void *)((unsigned long)start+size);
  node->cct_id = id;
  /*first delete and then insert*/
  interval_tree_delete(&root, &dummy, node->start, node->end);
  if(interval_tree_insert(&root, node)!=0)
  {
    spinlock_unlock(&splaylock);
    return -1;//insert fail
  }
  TMSG(IBS_SAMPLE, "splay insert: start(%p), end(%p)", node->start, node->end);
  spinlock_unlock(&splaylock);
  return 0;
}
 
interval_tree_node* splaytree_lookup(void* p)
{
  spinlock_lock(&splaylock);
  interval_tree_node* result_node;
  if (root == NULL) return NULL;
  result_node = interval_tree_lookup(&root, p);
  spinlock_unlock(&splaylock);
  return result_node;
}
 
interval_tree_node* delete_splay_tree(void* ptr)
{
  spinlock_lock(&splaylock);
  interval_tree_node* dummy=NULL;
  interval_tree_delete(&root, &dummy, ptr, ptr+1);
  spinlock_unlock(&splaylock);
  return dummy;
}  
                                                
