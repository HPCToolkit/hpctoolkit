//******************************************************************************
// a fair, phased reader-writer lock with local spinning
//
// Reference:
//
// Björn B. Brandenburg and James H. Anderson. 2010. Spin-based reader-writer 
// synchronization for multiprocessor real-time systems. Real-Time Systems
// 46, 1 (September 2010), 25-87. http://dx.doi.org/10.1007/s11241-010-9097-2
//
//******************************************************************************



//******************************************************************************
// local includes
//******************************************************************************

#include <include/hpctoolkit-config.h>

#include "pfq-rwlock.h"


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

#define HOST_BIG_ENDIAN


//******************************************************************************
// macros
//******************************************************************************

#define READER_INCREMENT 0x100
#define PHASE_BIT        0x001
#define WRITER_PRESENT   0x002
#define WRITER_MASK      (PHASE_BIT | WRITER_PRESENT)
#define TICKET_MASK      ~(WRITER_MASK)

#define PFQ_NIL          ((pfq_rwlock_node *) 0)
#define PFQ_WAIT         ((pfq_rwlock_node *) 1)

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


//------------------------------------------------------------------
// toggle PHASE_BIT by writing the low order byte
// non-atomicity is OK since there are no concurrent updates of the
// low-order byte containing the phase bit
//------------------------------------------------------------------
#define TOGGLE_PHASE_BIT(p) \
  do {\
    unsigned char *lsb = LSB_PTR(p);\
    lsb ^= PHASE_BIT;\
  } while(0)


//******************************************************************************
// interface operations
//******************************************************************************

void
pfq_rwlock_start_read(pfq_rwlock *l, pfq_rwlock_node *me)
{
  uint32_t ticket = fetch_and_add(&(l->rin), READER_INCREMENT);

  if (ticket & WRITER_PRESENT) {
    uint32_t phase = ticket & PHASE_BIT;
#if PFQ_ORIGINAL
    me->blocked = TRUE;
    pfq_rwlock_node *prev = fetch_and_store(&(l->rtail[phase]), me);
    if (prev != PFQ_NIL) {
      while (me->blocked);
      if (prev != PFQ_WAIT) {
        prev->blocked = FALSE;
      }
    } else {
      pfq_rwlock_node *prev = fetch_and_store(&(l->rtail[phase]), PFQ_NIL);
      prev->blocked = FALSE;
      while (me->blocked);
    }
  }
#else
    while (l->flag[phase] == l->sense);
#endif
}


void 
pfq_rwlock_end_read(pfq_rwlock *l, pfq_rwlock_node *me)
{
  uint32_t ticket = fetch_and_add(&(l->rout), READER_INCREMENT);
  if ((ticket & WRITER_PRESENT) && 
      ((ticket & TICKET_MASK) == l->last - 1)) {
    l->whead->blocked = FALSE;
  }
}


void
pfq_rwlock_start_write(pfq_rwlock *l, pfq_rwlock_node *me)
{
  pfq_rwlock_node *prev = fetch_and_store(&(l->wtail), me);

  //-------------------------------------------------------------
  // wait for writers ahead of me, if any
  //-------------------------------------------------------------
  if (prev != PFQ_NIL) {
    me->blocked = TRUE;
    prev->next = me;
    while (me->blocked);
  }

  //-------------------------------------------------------------
  // announce myself as next writer
  //-------------------------------------------------------------
  l->whead = me;

  //-------------------------------------------------------------
  // signal readers about the presence of a writer by setting
  // the WRITER_PRESENT bit
  //
  // wait for all active reads to drain
  //-------------------------------------------------------------
  uint32_t phase = l->rin & PHASE_BIT;

#ifdef ORIGINAL
  l->rtail[phase] = PFQ_WAIT;
#else
  // set the flag to block any readers in the next batch 
  l->flag[phase] = TRUE; 
#endif

  uint32_t ticket = fetch_and_add(&(l->rin), WRITER_PRESENT) & TICKET_MASK;

  l->last = ticket;

  uint32_t exit = fetch_and_add(&(l->rout), WRITER_PRESENT) & TICKET_MASK;

  if (exit != ticket) {
    while (me->blocked);
  }
}



void 
pfq_rwlock_end_write(pfq_rwlock *l, pfq_rwlock_node *me)
{
  l->rout &= TICKET_MASK; // clear WRITER_PRESENT and PHASE_BIT 

  //------------------------------------------------------------------
  // toggle PHASE_BIT by writing the low order byte
  // non-atomicity is OK since there are no concurrent updates of the
  // low-order byte containing the phase bit
  //------------------------------------------------------------------
  unsigned char *lsb = LSB_PTR(&(l->rin));
  uint32_t phase = *lsb;
  *lsb ^= PHASE_BIT;

#ifdef ORIGINAL
  pfq_rwlock_node *prev = fetch_and_store(&(l->rtail[phase]), PFQ_NIL);
  if (prev != PFQ_WAIT) {
    prev->blocked = FALSE;
  }
#else
  // clear the flag to release waiting readers in the current read phase 
  l->flag[phase] = FALSE; 
#endif

  if ((me->next == PFQ_NIL) &&
      (compare_and_swap(&(l->wtail), me, PFQ_NIL)) return;

  while (me->next == PFQ_NIL);

  me->next->blocked = FALSE;
}
