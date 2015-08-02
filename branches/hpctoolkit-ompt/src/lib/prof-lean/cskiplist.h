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



//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Define an API for a concurrent skip list.
//
//******************************************************************************

#ifndef __cskiplist_h__
#define __cskiplist_h__

#include "mcs-lock.h"



//******************************************************************************
// types
//******************************************************************************

typedef struct node_s {
  int value;
  int height;
  bool fully_linked;
  bool marked;

  mcs_lock_t lock;

  // memory allocated for a node will include space for its vector of  pointers 
  struct node_s *nexts[]; 
} node_t;


typedef struct cskiplist_s {
  node_t *left_sentinel;
  node_t *right_sentinel;
  int max_height;
} cskiplist_t;



//******************************************************************************
// interface operations
//******************************************************************************

cskiplist_t *cskiplist_new(int max_height);

node_t *cskiplist_find(cskiplist_t *l,   int value);

bool cskiplist_insert(cskiplist_t *l, int value);

bool cskiplist_delete(cskiplist_t *l, int value);

bool cskiplist_delete_bulk_unsynchronized(cskiplist_t *l, int low, int high);


#endif
