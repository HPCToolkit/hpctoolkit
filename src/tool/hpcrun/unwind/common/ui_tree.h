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
// Copyright ((c)) 2002-2016, Rice University
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
 * The interface to the unwind interval tree.
 *
 */

#ifndef _UI_TREE_H_
#define _UI_TREE_H_

#include <sys/types.h>
#include <hpcrun/utilities/ip-normalized.h>
#include "splay-interval.h"
#include "splay.h"

void hpcrun_interval_tree_init(void);
void hpcrun_release_splay_lock(void);
void *hpcrun_ui_malloc(size_t ui_size);

splay_interval_t *hpcrun_addr_to_interval(void *addr,
					  void *ip, ip_normalized_t* ip_norm);
splay_interval_t *hpcrun_addr_to_interval_locked(void *addr);

void hpcrun_delete_ui_range(void *start, void *end);
void free_ui_node_locked(interval_tree_node *node);

void hpcrun_print_interval_tree(void);

#endif  /* !_UI_TREE_H_ */
