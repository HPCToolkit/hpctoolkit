// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2015, Rice University
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
 * Interface to unwind recipe map.
 *
 */

#ifndef _UI_TREE_H_
#define _UI_TREE_H_

#include <sys/types.h>
#include "binarytree_uwi.h"
#include "ilmstat_btuwi_pair.h"
#include "std_unw_cursor.h"


void
uw_recipe_map_init(void);

/*
 * add to the unwind recipe map a "poisoned" bounding interval with a NULL load
 * module and NULL bitree_uiw_t tree: ({([start, end), NULL), NEVER}, NULL)
 * return true if the insertion is successful, false otherwise.
 */
bool
uw_recipe_map_poison(uintptr_t start, uintptr_t end);

bool
uw_recipe_map_unpoison(uintptr_t start, uintptr_t end);

bool
uw_recipe_map_repoison(uintptr_t start, uintptr_t end);

/*
 * if addr is found in range in the map, return true and
 *   *unwr_info is the ilmstat_btuwi_pair_t ( ([start, end), ldmod, status), btuwi ),
 *   where the root of btuwi is the uwi_t for addr
 * else return false
 */
bool
uw_recipe_map_lookup(void *addr, unwindr_info_t *unwr_info);


void
uw_recipe_map_delete_range(void *start, void *end);


void
uw_recipe_map_print(void);

#endif  /* !_UI_TREE_H_ */
