//******************************************************************************
// a fair, phased reader-writer lock with local spinning
//
// Reference:
//   Björn B. Brandenburg and James H. Anderson. 2010. Spin-based reader-writer 
//   synchronization for multiprocessor real-time systems. Real-Time Systems
//   46(1):25-87 (September 2010).  http://dx.doi.org/10.1007/s11241-010-9097-2
//
// Notes:
//   the reference uses a queue for arriving readers. on a cache coherent 
//   machine, the local spinning property for waiting readers can be achieved
//   by simply using a cacheble flag. the implementation here uses that
//   simplification.
//
//******************************************************************************



//******************************************************************************
// local includes
//******************************************************************************

#define HOST_BIG_ENDIAN

#include <include/hpctoolkit-config.h>

#include "pfq-rwlock.h"
#include "fence.h"
#include "atomics.h"



//******************************************************************************
// macros
//******************************************************************************

#define READER_INCREMENT 0x100

#define PHASE_BIT        0x001
#define WRITER_PRESENT   0x002

#define WRITER_MASK      (PHASE_BIT | WRITER_PRESENT)
#define TICKET_MASK      ~(WRITER_MASK)

#define PFQ_NIL          ((pfq_rwlock_node_t *) 0)


//------------------------------------------------------------------
// define a macro to point to the low-order byte of an integer type
// in a way that will work on both big-endian and little-endian 
// processors
//------------------------------------------------------------------
#ifdef HOST_BIG_ENDIAN
#define LSB_PTR(p) (((unsigned char *) p) + (sizeof(*p) - 1))
#endif

#ifdef HOST_LITTLE_ENDIAN
#define LSB_PTR(p) ((unsigned char *) p)
#endif

#ifndef LSB_PTR
#error "endianness must be configured. " \
       "use --enable-endian to force configuration"
#endif



//******************************************************************************
// interface operations
//******************************************************************************

void
pfq_rwlock_start_read(pfq_rwlock_t *l, pfq_rwlock_node_t *me)
{
  uint32_t ticket = fetch_and_add_i32(&(l->rin), READER_INCREMENT);

  if (ticket & WRITER_PRESENT) {
    uint32_t phase = ticket & PHASE_BIT;
    while (l->flag[phase].flag);

    //--------------------------------------------------------------------------
    // prevent speculative reads until after flag is set
    //--------------------------------------------------------------------------
    enforce_load_to_load_order();
  } else {
    //--------------------------------------------------------------------------
    // prevent speculative reads until after the counter is incremented
    //--------------------------------------------------------------------------
    enforce_rmw_to_load_order();
  }
}


void 
pfq_rwlock_end_read(pfq_rwlock_t *l, pfq_rwlock_node_t *me)
{
  //----------------------------------------------------------------------------
  // finish reads before the counter is incremented
  //----------------------------------------------------------------------------
  enforce_load_to_rmw_order();

  uint32_t ticket = fetch_and_add_i32(&(l->rout), READER_INCREMENT);

  if ((ticket & WRITER_PRESENT) && 
      ((ticket & TICKET_MASK) == l->last - 1)) {
    l->whead->blocked = false;
  }
}


void
pfq_rwlock_start_write(pfq_rwlock_t *l, pfq_rwlock_node_t *me)
{
  //--------------------------------------------------------------------
  // use MCS lock to enforce mutual exclusion with other writers
  //--------------------------------------------------------------------
  mcs_lock(&(l->wtail), me);

  //--------------------------------------------------------------------
  // always clear when at the head of the mcs queue
  //--------------------------------------------------------------------
  me->blocked = true;

  //--------------------------------------------------------------------
  // announce myself as next writer
  //--------------------------------------------------------------------
  l->whead = me;

  //--------------------------------------------------------------------
  // set the flag to block any readers in the next batch 
  //--------------------------------------------------------------------
  uint32_t phase = l->rin & PHASE_BIT;
  l->flag[phase].flag = true; 

  //----------------------------------------------------------------------------
  // store to flag must complete before incrementing rin
  //----------------------------------------------------------------------------
  enforce_store_to_rmw_order();

  //--------------------------------------------------------------------
  // acquire an "in" sequence number to see how many readers arrived
  // set the WRITER_PRESENT bit so subsequent readers will wait
  //--------------------------------------------------------------------
  uint32_t in = fetch_and_add_i32(&(l->rin), WRITER_PRESENT) & TICKET_MASK;

  //--------------------------------------------------------------------
  // save the identity of the last reader 
  //--------------------------------------------------------------------
  l->last = in;
  
  //----------------------------------------------------------------------------
  // store to last must complete before notifying readers of writer 
  // stores to whead and me->blocked also were already committed
  // before last fetch-and-add_i32
  // the LL of l->rout must not occur before before the LL of l->rin. 
  // the fence below enforces that too.
  //----------------------------------------------------------------------------
  enforce_load_to_load_and_store_to_store_order();

  //-------------------------------------------------------------
  // acquire an "out" sequence number to see how many readers left
  // set the WRITER_PRESENT bit so the last reader will know to signal
  // it is responsible for signaling the waiting writer
  //-------------------------------------------------------------
  uint32_t out = fetch_and_add_i32(&(l->rout), WRITER_PRESENT) & TICKET_MASK;

  //--------------------------------------------------------------------
  // if any reads are active, wait for last reader to signal me
  //--------------------------------------------------------------------
  if (in != out) {
    while (me->blocked); // wait for active reads to drain
  
    //--------------------------------------------------------------------------
    // store to flag must complete before notifying readers of writer 
    //--------------------------------------------------------------------------
    enforce_load_to_access_order();
  }
}


void 
pfq_rwlock_end_write(pfq_rwlock_t *l, pfq_rwlock_node_t *me)
{
  unsigned char *lsb;
  //----------------------------------------------------------------------------
  // finish accesses in the critical section before signalling next
  // readers or next writer  
  //----------------------------------------------------------------------------
  enforce_access_to_store_order();

  //--------------------------------------------------------------------
  // clear WRITER_PRESENT in rout
  //--------------------------------------------------------------------
  lsb = LSB_PTR(&(l->rout));
  *lsb = 0; 

  //----------------------------------------------------------------------------
  // no fence needed because readers don't modify LSB(rout) 
  // mcs_unlock will ensure this store commits before next writer proceeds 
  //----------------------------------------------------------------------------

  //--------------------------------------------------------------------
  // toggle PHASE_BIT by writing the low order byte
  // non-atomicity is OK since there are no concurrent updates of the
  // low-order byte containing the phase bit
  //--------------------------------------------------------------------
  lsb = LSB_PTR(&(l->rin));
  uint32_t phase = *lsb;

  //--------------------------------------------------------------------
  // toggle phase and clear WRITER_PRESENT in rin
  //--------------------------------------------------------------------
  *lsb = (~phase) & PHASE_BIT;

  //----------------------------------------------------------------------------
  // clearing writer present in rin can be reordered with flag set below
  // because any arriving reader will see the cleared flag and proceed.
  //----------------------------------------------------------------------------

  //--------------------------------------------------------------------
  // clear the flag to release waiting readers in the current read phase 
  //--------------------------------------------------------------------
  l->flag[phase].flag = false; 

  //--------------------------------------------------------------------
  // pass writer lock to next writer 
  //--------------------------------------------------------------------
  mcs_unlock(&(l->wtail), me);
}
