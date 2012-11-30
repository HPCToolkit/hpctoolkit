// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-jvmti/src/tool/hpcrun/env.h $
// $Id: env.h 3680 2012-02-25 22:14:00Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

/*
  Copyright ((c)) 2002-2012, Rice University 
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


//***************************************************************************
// Header files
//***************************************************************************
#include <stdio.h>
#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>

#include "hpcrun_java.h"
#include "splay.h"
#include "loadmap.h"

static interval_tree_node *ui_java_tree_root 	= NULL;
static load_module_t	  *ui_lm_java_ptr	= NULL;

static hpcj_remove_leaf(interval_tree_node *node)
{
  if (node != NULL)
  {
     hpcj_remove_leaf(node->prev);
     hpcj_remove_leaf(node->next);
     free(node);
  }
  
}

void hpcjava_unwind_init()
{
  ui_java_tree_root 	= NULL;
  ui_lm_java_ptr	= (load_module_t*) hpcrun_malloc(sizeof(load_module_t));
  ui_lm_java_ptr->id 	= 0;
}


void hpcjava_unwind_finalize()
{
}

/*
 * adding a new interval address to the splay tree data structure
 * param
 *  addr_start: the start of java's virtual memory address
 *  addr_end:   the end
 */
void hpcjava_add_address_interval(const void *addr_start, const void *addr_end)
{
  TMSG(JAVA, "Adding a java interval address: addr %p - %p", (void*)addr_start, (void*) addr_end);

  interval_tree_node *p = (interval_tree_node*) hpcrun_malloc(sizeof(interval_tree_node));

  if (addr_start == NULL || addr_end == NULL) {
    return;
  }

  /* create an interval address node */
  p->start  = addr_start;
  p->end    = addr_end;
  p->lm     = ui_lm_java_ptr;

  /* add temporary load module for java  */
  p->lm->name = "javatmp.jo";

  int ret = interval_tree_insert(&ui_java_tree_root, p);

  if (ret != 0 ) {
    TMSG(JAVA, "Fail to insert into java tree: addr %p - %p", (void*)addr_start, (void*) addr_end);
  } else {
    TMSG(JAVA, "insert into java tree: addr %p - %p", (void*)addr_start, (void*) addr_end);
  }
}

interval_tree_node* hpcjava_get_interval(void *addr)
{
  return interval_tree_lookup(&ui_java_tree_root, addr);
}
