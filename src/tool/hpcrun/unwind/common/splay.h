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
 * Declarations used by interval splay tree code.
 *
 */

#ifndef _SPLAY_H_
#define _SPLAY_H_

#include "splay-interval.h"


/*
 * As a slicky trick, interval_tree_node overlays with
 * unwind_interval_t where next = right and prev = left.
 * (May not want to keep this.)
 */

typedef struct splay_interval_s interval_tree_node;

#define START(n)   (n)->start
#define END(n)     (n)->end
#define RIGHT(n)   (n)->next
#define LEFT(n)    (n)->prev

#define SSTART(n)  (n).start
#define SEND(n)    (n).end
#define SRIGHT(n)  (n).next
#define SLEFT(n)   (n).prev

// FIXME: STOP embedding pointer info in type!! 
//
typedef interval_tree_node *interval_tree_node_t;

interval_tree_node *interval_tree_lookup(interval_tree_node **root, void *addr);
int  interval_tree_insert(interval_tree_node **root, interval_tree_node *node);
void interval_tree_delete(interval_tree_node **root, interval_tree_node **del_tree,
			  void *start, void *end);
void interval_tree_verify(interval_tree_node *root, const char *label);


#define SUCCESS  0
#define FAILURE  1

#endif  /* !_SPLAY_H_ */
