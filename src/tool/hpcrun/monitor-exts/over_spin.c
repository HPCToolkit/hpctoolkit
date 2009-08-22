// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
 * Override pthread spin lock functions.
 *
 */

#include <pthread.h>
#include <stdio.h>

#include "monitor_ext.h"

#include <messages/messages.h>

// tallent: Lock interception must be as fast as possible.  Therefore,
// I am obsoleting the needless layer of callbacks between the
// interception point and hpcrun.
#if 0 // OBSOLETE

typedef int spinlock_fcn(pthread_spinlock_t *);

#ifdef HPCRUN_STATIC_LINK
extern spinlock_fcn __real_pthread_spin_lock;
extern spinlock_fcn __real_pthread_spin_trylock;
extern spinlock_fcn __real_pthread_spin_unlock;
#endif

static spinlock_fcn *real_spin_lock = NULL;
static spinlock_fcn *real_spin_trylock = NULL;
static spinlock_fcn *real_spin_unlock = NULL;


int
MONITOR_EXT_WRAP_NAME(pthread_spin_lock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_EXT_GET_NAME_WRAP(real_spin_lock, pthread_spin_lock);
  monitor_thread_pre_spin_lock(lock);
  ret = (*real_spin_lock)(lock);
  monitor_thread_post_spin_lock(lock, ret);

  return (ret);
}

int
MONITOR_EXT_WRAP_NAME(pthread_spin_trylock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_EXT_GET_NAME_WRAP(real_spin_trylock, pthread_spin_trylock);
  //monitor_thread_pre_spin_trylock(lock); // tallent: needed?
  ret = (*real_spin_trylock)(lock);
  monitor_thread_post_spin_trylock(lock, ret);

  return (ret);
}

int
MONITOR_EXT_WRAP_NAME(pthread_spin_unlock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_EXT_GET_NAME_WRAP(real_spin_unlock, pthread_spin_unlock);
  //monitor_thread_pre_spin_unlock(lock); // tallent: needed?
  ret = (*real_spin_unlock)(lock);
  monitor_thread_post_spin_unlock(lock, ret);

  return (ret);
}

#endif // OBSOLETE
