//******************************************************************************
// MCS lock - a fair based queueing lock
//******************************************************************************



//******************************************************************************
// local includes
//******************************************************************************

#include "mcs-lock.h"
#include "fence.h"
#include "atomic-powerpc.h"



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
  mcs_node_t *predecessor = (mcs_node_t *) fetch_and_store(l, me);

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
mcs_unlock(mcs_lock_t l, mcs_node_t *me)
{
  if (!me->next) {            // I don't currently have a successor
    //--------------------------------------------------------------------
    // all accesses in the critical section must occur before releasing 
    // the lock by unlinking myself with a compare-and-swap
    //--------------------------------------------------------------------
    enforce_access_to_rmw_order(); 

    //--------------------------------------------------------------------
    // if my node is at the tail of the queue, remove myself
    //--------------------------------------------------------------------
    if (compare_and_swap_bool(&l, me, 0)) {
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
