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
#include "fence.h"
#include "atomics.h"



//******************************************************************************
// private operations
//******************************************************************************

static inline mcs_node_t *
fas(volatile mcs_node_t **p, mcs_node_t *n)
{
  return (mcs_node_t *) fetch_and_store((volatile void **) p, n);
}


static inline bool
cas_bool(volatile mcs_node_t **p, mcs_node_t *o, mcs_node_t *n)
{
  return compare_and_swap_bool((volatile void **) p, o, n);
}


//******************************************************************************
// interface operations
//******************************************************************************

void
mcs_lock(mcs_lock_t *l, mcs_node_t *me)
{
  //--------------------------------------------------------------------
  // initialize my queue node
  //--------------------------------------------------------------------
  me->next = 0;           

  //--------------------------------------------------------------------
  // initialization must complete before anyone sees my node
  //--------------------------------------------------------------------
  enforce_store_to_rmw_order(); 

  //--------------------------------------------------------------------
  // install my node at the tail of the lock queue. 
  // determine my predecessor, if any.
  //--------------------------------------------------------------------
  mcs_node_t *predecessor = fas(&(l->tail), me);

  //--------------------------------------------------------------------
  // if I have a predecessor, wait until it signals me
  //--------------------------------------------------------------------
  if (predecessor) {
    //------------------------------------------------------------------
    // prepare to block until signaled by my predecessor
    //------------------------------------------------------------------
    me->blocked = true; 

    //------------------------------------------------------------------
    // blocked field must be set before linking behind my predecessor
    //------------------------------------------------------------------
    enforce_store_to_store_order(); 

    predecessor->next = me; // link behind my predecessor

    while (me->blocked);    // wait for my predecessor to clear my flag
  
    //------------------------------------------------------------------
    // no reads or writes in the critical section may occur until my 
    // flag is set
    //------------------------------------------------------------------
    enforce_load_to_access_order(); 
  } else {
    //------------------------------------------------------------------
    // no reads or writes in the critical section may occur until after 
    // my fetch-and-store
    //------------------------------------------------------------------
    enforce_rmw_to_access_order();
  }
}


void
mcs_unlock(mcs_lock_t *l, mcs_node_t *me)
{
  if (!me->next) {            
    //--------------------------------------------------------------------
    // I don't currently have a successor, so I may be at the tail
    //--------------------------------------------------------------------

    //--------------------------------------------------------------------
    // all accesses in the critical section must occur before releasing 
    // the lock by unlinking myself from the tail with a compare-and-swap
    //--------------------------------------------------------------------
    enforce_access_to_rmw_order(); 

    //--------------------------------------------------------------------
    // if my node is at the tail of the queue, attempt to remove myself
    //--------------------------------------------------------------------
    if (cas_bool(&(l->tail), me, 0)) {
      //------------------------------------------------------------------
      // I removed myself from the queue; I will never have a 
      // successor, so I'm done 
      //------------------------------------------------------------------
      return;
    }

    while (!me->next);       // wait for arriving successor
  } else {
    //--------------------------------------------------------------------
    // all accesses in the critical section must occur before passing the
    // lock to a successor by clearing its flag.
    //--------------------------------------------------------------------
    enforce_access_to_store_order(); 
  }

  me->next->blocked = false; // signal my successor
}
