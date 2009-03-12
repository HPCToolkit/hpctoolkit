/*
 * The interface to the unwind interval tree.
 *
 * $Id$
 */

#ifndef _UI_TREE_H_
#define _UI_TREE_H_

#include <sys/types.h>
#include "splay-interval.h"

void csprof_interval_tree_init(void);
void csprof_release_splay_lock(void);
void *csprof_ui_malloc(size_t ui_size);

splay_interval_t *csprof_addr_to_interval(void *addr);
splay_interval_t *csprof_addr_to_interval_locked(void *addr);
void csprof_delete_ui_range(void *start, void *end);

void csprof_print_interval_tree(void);

#endif  /* !_UI_TREE_H_ */
