/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
#include <assert.h>

#include "general.h"
#include "mem.h"
#include "pmsg.h"

#include "list.h"

static csprof_list_node_t *csprof_list_node_alloc(csprof_list_pool_t *pool);
static void csprof_list_node_free(csprof_list_pool_t *, csprof_list_node_t *);
static void csprof_list_pool_grow(csprof_list_pool_t *, unsigned int);

static void csprof_list_node_link(csprof_list_node_t *, csprof_list_node_t *);

csprof_list_pool_t *
csprof_list_pool_new(unsigned int initial_size)
{
    csprof_list_pool_t *pool;

    /* initialize pool */
    TMSG(MALLOC," list_pool_new");
    pool = csprof_malloc(sizeof(csprof_list_pool_t));

    if(pool == NULL) {
        ERRMSG("Couldn't allocate memory for list pool", __FILE__, __LINE__);
        return NULL;
    }

    memset(pool, 0, sizeof(csprof_list_pool_t));

    csprof_list_pool_grow(pool, initial_size);

    return pool;
}

void
csprof_list_pool_grow(csprof_list_pool_t *pool, unsigned int numnodes)
{
    csprof_list_node_t *node_storage;
    int i;

    TMSG(MALLOC," list_pool_grow");
    node_storage = csprof_malloc(numnodes * sizeof(csprof_list_node_t));

    if(node_storage == NULL) {
        ERRMSG("Could not allocate memory for list pool", __FILE__, __LINE__);
        return;
    }

    memset(node_storage, 0, numnodes * sizeof(csprof_list_node_t));

    for(i=0; i<numnodes; ++i) {
        csprof_list_node_t *node = node_storage;
        ++node_storage;
        /* push node onto the freelist */
        node->next = pool->freelist;
        pool->freelist = node;
    }

    pool->count += numnodes;
}

/* functions on lists */

csprof_list_t *
csprof_list_new(csprof_list_pool_t *pool)
{
    csprof_list_node_t *sentinel = csprof_list_node_alloc(pool);

    sentinel->next = sentinel->prev = sentinel;
    sentinel->ip = 0;
    sentinel->sp = 0;

    return sentinel;
}

/* deallocate all nodes in `list' */
void
csprof_list_destroy(csprof_list_pool_t *pool, csprof_list_t *list)
{
    csprof_list_node_t *sentinel = (csprof_list_node_t *)list;

    /* deallocate everything in one fell swoop */
    sentinel->prev->next = pool->freelist;
    pool->freelist = sentinel;
}

int
csprof_list_isempty(csprof_list_t *list)
{
    return list->next == list;
}

/* tack `list2' onto the end of `list1'.  when this function returns,
   `list2' has no elements and should be destroyed */
void
csprof_list_append(csprof_list_t *list1, csprof_list_t *list2)
{
    if(!csprof_list_isempty(list2)) {
        csprof_list_node_t *sentinel1 = (csprof_list_node_t *)list1;
        csprof_list_node_t *tail1 = sentinel1->prev;
        csprof_list_node_t *sentinel2 = (csprof_list_node_t *)list2;
        csprof_list_node_t *head2 = sentinel2->next;
        csprof_list_node_t *tail2 = sentinel2->prev;

        csprof_list_node_link(tail1, head2);
        csprof_list_node_link(tail2, sentinel1);

        sentinel2->next = sentinel2->prev = sentinel2;
    }
}

/* remove the first node of `list' */
void
csprof_list_remove_first(csprof_list_pool_t *pool, csprof_list_t *list)
{
    csprof_list_node_t *sentinel = (csprof_list_node_t *)list;

    /* if there's actually a node to remove */
    if(!csprof_list_isempty(list)) {
        csprof_list_node_t *head = sentinel->next;
        csprof_list_node_t *newhead = head->next;

        csprof_list_node_link(sentinel, newhead);

        csprof_list_node_free(pool, head);
    }
}

/* remove the last node of `list' */
void
csprof_list_remove_tail(csprof_list_pool_t *pool, csprof_list_t *list)
{
    csprof_list_node_t *sentinel = (csprof_list_node_t *)list;

    /* if there's actually a node to remove */
    if(!csprof_list_isempty(list)) {
        csprof_list_node_t *tail = sentinel->prev;
        csprof_list_node_t *newtail = tail->prev;

        csprof_list_node_link(newtail, sentinel);

        csprof_list_node_free(pool, tail);
    }
}

void
csprof_list_add_head(csprof_list_pool_t *pool, csprof_list_t *list,
                     void *ip, void *sp, void *treenode)
{
    csprof_list_node_t *sentinel = (csprof_list_node_t*)list;
    csprof_list_node_t *head = sentinel->next;
    csprof_list_node_t *newnode = csprof_list_node_alloc(pool);

    newnode->ip = ip;
    newnode->sp = sp;
    newnode->node = treenode;

    csprof_list_node_link(sentinel, newnode);
    csprof_list_node_link(newnode, head);
}

void
csprof_list_add_tail(csprof_list_pool_t *pool, csprof_list_t *list,
                     void *ip, void *sp, void *treenode)
{
    csprof_list_node_t *sentinel = (csprof_list_node_t*)list;
    csprof_list_node_t *tail = sentinel->prev;
    csprof_list_node_t *newnode = csprof_list_node_alloc(pool);

    newnode->ip = ip;
    newnode->sp = sp;
    newnode->node = treenode;

    csprof_list_node_link(newnode, sentinel);
    csprof_list_node_link(tail, newnode);
}

/* make `y' come after `x' */
static void
csprof_list_node_link(csprof_list_node_t *x, csprof_list_node_t *y)
{
    x->next = y;
    y->prev = x;
}

/* managing node memory */

/* return a new node from the free list */
static csprof_list_node_t *
csprof_list_node_alloc(csprof_list_pool_t *pool)
{
    csprof_list_node_t *node;

    if(pool == NULL) {
        DIE("Null pool", __FILE__, __LINE__);
    }

    if(pool->freelist == NULL) {
        /* no free nodes.  add some new ones */
        csprof_list_pool_grow(pool, 16);
    }

    /* sanity check */
    if(pool->freelist == NULL) {
        DIE("Could not grow list pool", __FILE__, __LINE__);
    }

    node = pool->freelist;
    /* pool->freelist == node at this point; pop the node */
    pool->freelist = pool->freelist->next;
    node->next = NULL;
    node->prev = NULL;
    pool->count--;

    /* zero out the treenode to avoid wacky problems */
    node->node = NULL;

    return node;
}

/* return `node' to `pool' */
static void
csprof_list_node_free(csprof_list_pool_t *pool, csprof_list_node_t *node)
{
    node->next = pool->freelist;
    pool->freelist = node;
    pool->count++;
}
