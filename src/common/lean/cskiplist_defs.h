// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

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
