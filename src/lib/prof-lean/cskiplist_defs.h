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
// Copyright ((c)) 2002-2019, Rice University
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
// File: cskiplist_defs.h
//   $HeadURL$
//
// Purpose:
//   Define data structures for a concurrent skip list.
//
//******************************************************************************


#ifndef __CSKIPLIST_DEFS_H__
#define __CSKIPLIST_DEFS_H__

#include "mcs-lock.h"
#include "pfq-rwlock.h"

typedef struct csklnode_s {
  void *val;
  int height;
  volatile bool fully_linked;
  volatile bool marked;
  mcs_lock_t lock;
  // memory allocated for a node will include space for its vector of  pointers
  struct csklnode_s *nexts[];
} csklnode_t;

typedef struct cskiplist_s {
  csklnode_t *left_sentinel;
  csklnode_t *right_sentinel;
  int max_height;
  val_cmp compare;
  val_cmp inrange;
  pfq_rwlock_t lock;
} cskiplist_t;

#endif /* __CSKIPLIST_DEFS_H__ */
