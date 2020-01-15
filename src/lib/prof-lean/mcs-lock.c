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
// Copyright ((c)) 2002-2020, Rice University
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
//   Implement an API for the MCS lock: a fair queue-based lock.
//
// Reference:
//   John M. Mellor-Crummey and Michael L. Scott. 1991. Algorithms for scalable
//   synchronization on shared-memory multiprocessors. ACM Transactions on
//   Computing Systems 9, 1 (February 1991), 21-65.
//   http://doi.acm.org/10.1145/103727.103729
//******************************************************************************



//******************************************************************************
// local includes
//******************************************************************************

#include "mcs-lock.h"

//******************************************************************************
// private operations
//******************************************************************************

//******************************************************************************
// interface operations
//******************************************************************************

void
mcs_lock(mcs_lock_t *l, mcs_node_t *me)
{
  //--------------------------------------------------------------------
  // initialize my queue node
  //--------------------------------------------------------------------
  atomic_init(&me->next, mcs_nil);

  //--------------------------------------------------------------------
  // install my node at the tail of the lock queue.
  // determine my predecessor, if any.
  //
  // note: the rel aspect of the ordering below ensures that
  // initialization of me->next completes before anyone sees my node
  //--------------------------------------------------------------------
  mcs_node_t *predecessor =
    atomic_exchange_explicit(&l->tail, me, memory_order_acq_rel);

  //--------------------------------------------------------------------
  // if I have a predecessor, wait until it signals me
  //--------------------------------------------------------------------
  if (predecessor != mcs_nil) {
    //------------------------------------------------------------------
    // prepare to block until signaled by my predecessor
    //------------------------------------------------------------------
    atomic_init(&me->blocked, true);

    //------------------------------------------------------------------
    // link behind my predecessor
    // note: use release to ensure that prior assignment to blocked
    //       occurs first
    //------------------------------------------------------------------
    atomic_store_explicit(&predecessor->next, me, memory_order_release);

    //------------------------------------------------------------------
    // wait for my predecessor to clear my flag
    // note: use acquire order to ensure that reads or writes in the
    //       critical section will not occur until after blocked is
    //       cleared
    //------------------------------------------------------------------
    while (atomic_load_explicit(&me->blocked, memory_order_acquire));
  }
}


bool
mcs_trylock(mcs_lock_t *l, mcs_node_t *me)
{
  //--------------------------------------------------------------------
  // initialize my queue node
  //--------------------------------------------------------------------
  atomic_store_explicit(&me->next, mcs_nil, memory_order_relaxed);

  //--------------------------------------------------------------------
  // if the tail pointer is nil, swap it with a pointer to me, which
  // acquires the lock and installs myself at the tail of the queue.
  // note: the acq_rel ordering ensures that
  // (1) rel: my store of me->next above completes before the exchange
  // (2) acq: any accesses after the exchange can't begin until after
  //     the exchange completes.
  //--------------------------------------------------------------------
  mcs_node_t *oldme = mcs_nil;
  return
    atomic_compare_exchange_strong_explicit(&l->tail, &oldme, me,
					    memory_order_acq_rel,
					    memory_order_relaxed);
}


void
mcs_unlock(mcs_lock_t *l, mcs_node_t *me)
{
  mcs_node_t *successor = atomic_load_explicit(&me->next, memory_order_acquire);

  if (successor == mcs_nil) {
    //--------------------------------------------------------------------
    // I don't currently have a successor, so I may be at the tail
    //--------------------------------------------------------------------

    //--------------------------------------------------------------------
    // if my node is at the tail of the queue, attempt to remove myself
    // note: release order below on success guarantees that all accesses
    //       above the exchange must complete before the exchange if the
    //       exchange unlinks me from the tail of the queue
    //--------------------------------------------------------------------
    mcs_node_t *oldme = me;

    if (atomic_compare_exchange_strong_explicit(&l->tail, &oldme, mcs_nil,
						memory_order_release,
						memory_order_relaxed)) {
      //------------------------------------------------------------------
      // I removed myself from the queue; I will never have a
      // successor, so I'm done
      //------------------------------------------------------------------
      return;
    }

    //------------------------------------------------------------------
    // another thread is writing me->next to define itself as our successor;
    // wait for it to finish that
    //------------------------------------------------------------------
    while (mcs_nil == (successor = atomic_load_explicit(&me->next, memory_order_acquire)));
  }

  atomic_store_explicit(&successor->blocked, false, memory_order_release);
}
