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
#include "atomic-powerpc.h"

#if 0
uint32_t 
fetch_and_add(uint32_t *p, uint32_t val)
{
  uint32_t prev = *p;
  *p += val;
  return prev;
}


pfq_rwlock_node *
fetch_and_store(pfq_rwlock_node **p, pfq_rwlock_node *val)
{
  pfq_rwlock_node *prev = *p;
  *p = val;
  return prev;
}


uint32_t 
compare_and_swap(pfq_rwlock_node **p, pfq_rwlock_node *old, pfq_rwlock_node *val)
{
  uint32_t match = *p == old;
  if (match) *p = val;
  return match;
}
#endif



//******************************************************************************
// macros
//******************************************************************************

#define READER_INCREMENT 0x100

#define PHASE_BIT        0x001
#define WRITER_PRESENT   0x002

#define WRITER_MASK      (PHASE_BIT | WRITER_PRESENT)
#define TICKET_MASK      ~(WRITER_MASK)

#define PFQ_NIL          ((pfq_rwlock_node *) 0)

#define TRUE             1
#define FALSE            0


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
#error endianness must be configured. use --enable-endian to force configuration
#endif



//******************************************************************************
// interface operations
//******************************************************************************

void
pfq_rwlock_start_read(pfq_rwlock *l, pfq_rwlock_node *me)
{
  uint32_t ticket = fetch_and_add32(&(l->rin), READER_INCREMENT);

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
pfq_rwlock_end_read(pfq_rwlock *l, pfq_rwlock_node *me)
{
  //----------------------------------------------------------------------------
  // finish reads before the counter is incremented
  //----------------------------------------------------------------------------
  enforce_load_to_rmw_order();

  uint32_t ticket = fetch_and_add32(&(l->rout), READER_INCREMENT);

  if ((ticket & WRITER_PRESENT) && 
      ((ticket & TICKET_MASK) == l->last - 1)) {
    l->whead->blocked = FALSE;
  }
}


void
pfq_rwlock_start_write(pfq_rwlock *l, pfq_rwlock_node *me)
{
  pfq_rwlock_node *prev = fetch_and_store(&(l->wtail), me);

  //--------------------------------------------------------------------
  // if any writers precede me, wait for writers them to finish
  //--------------------------------------------------------------------
  if (prev != PFQ_NIL) {
    me->blocked = TRUE;
    prev->next = me;
    while (me->blocked);
  }

  //--------------------------------------------------------------------
  // announce myself as next writer
  //--------------------------------------------------------------------
  l->whead = me;

  //--------------------------------------------------------------------
  // set the flag to block any readers in the next batch 
  //--------------------------------------------------------------------
  uint32_t phase = l->rin & PHASE_BIT;
  l->flag[phase].flag = TRUE; 

  //--------------------------------------------------------------------
  // acquire an "in" sequence number to see how many readers arrived
  // set the WRITER_PRESENT bit so subsequent readers will wait
  //--------------------------------------------------------------------
  uint32_t in = fetch_and_add32(&(l->rin), WRITER_PRESENT) & TICKET_MASK;

  //--------------------------------------------------------------------
  // save the identity of the last reader 
  //--------------------------------------------------------------------
  l->last = in;

  //-------------------------------------------------------------
  // acquire an "out" sequence number to see how many readers left
  // set the WRITER_PRESENT bit so the last reader will know to signal
  // it is responsible for signaling the waiting writer
  //-------------------------------------------------------------
  uint32_t out = fetch_and_add32(&(l->rout), WRITER_PRESENT) & TICKET_MASK;

  //--------------------------------------------------------------------
  // if any reads are active, wait for last reader to signal me
  //--------------------------------------------------------------------
  if (in != out) {
    while (me->blocked); // wait for active reads to drain
  }

#if defined(__powerpc__)
  // prevent speculative reads from starting before the lock has been acquired
  __asm__ __volatile__ ("isync\n");
#endif
}


void 
pfq_rwlock_end_write(pfq_rwlock *l, pfq_rwlock_node *me)
{
  l->rout &= TICKET_MASK; // clear WRITER_PRESENT and PHASE_BIT 

  //--------------------------------------------------------------------
  // toggle PHASE_BIT by writing the low order byte
  // non-atomicity is OK since there are no concurrent updates of the
  // low-order byte containing the phase bit
  //--------------------------------------------------------------------
  unsigned char *lsb = LSB_PTR(&(l->rin));
  uint32_t phase = *lsb;
  *lsb ^= PHASE_BIT;

  //--------------------------------------------------------------------
  // clear the flag to release waiting readers in the current read phase 
  //--------------------------------------------------------------------
  l->flag[phase].flag = FALSE; 

  if ((me->next == PFQ_NIL) &&
      (compare_and_swap_bool(&(l->wtail), me, PFQ_NIL))) return;

  //--------------------------------------------------------------------
  // wait for arriving writer
  //--------------------------------------------------------------------
  while (me->next == PFQ_NIL);

  //--------------------------------------------------------------------
  // signal next writer 
  //--------------------------------------------------------------------
  me->next->blocked = FALSE;
}
