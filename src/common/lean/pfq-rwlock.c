// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Implement the API for a fair, phased reader-writer lock with local spinning
//
// Reference:
//   Björn B. Brandenburg and James H. Anderson. 2010. Spin-based reader-writer
//   synchronization for multiprocessor real-time systems. Real-Time Systems
//   46(1):25-87 (September 2010).  http://dx.doi.org/10.1007/s11241-010-9097-2
//
// Notes:
//   the reference uses a queue for arriving readers. on a cache coherent
//   machine, the local spinning property for waiting readers can be achieved
//   by simply using a cacheable flag. the implementation here uses that
//   simplification.
//
//******************************************************************************



//******************************************************************************
// local includes
//******************************************************************************

#include "pfq-rwlock.h"

//******************************************************************************
// macros
//******************************************************************************

#define READER_INCREMENT 0x100

#define PHASE_BIT        0x001
#define WRITER_PRESENT   0x002

#define WRITER_MASK      (PHASE_BIT | WRITER_PRESENT)
#define TICKET_MASK      ~(WRITER_MASK)

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
pfq_rwlock_init(pfq_rwlock_t *l)
{
  atomic_init(&l->r_in, 0);
  atomic_init(&l->r_out, 0);
  atomic_init(&l->last, 0);
  atomic_init(&l->writer_blocking_readers[0].bit, false);
  atomic_init(&l->writer_blocking_readers[1].bit, false);
  mcs_init(&l->wtail);
  l->whead = mcs_nil;
}

void
pfq_rwlock_read_lock(pfq_rwlock_t *l)
{
  uint32_t ticket = atomic_fetch_add_explicit(&l->r_in, READER_INCREMENT, memory_order_acq_rel);

  if (ticket & WRITER_PRESENT) {
    uint32_t phase = ticket & PHASE_BIT;
    while (atomic_load_explicit(&l->writer_blocking_readers[phase].bit, memory_order_acquire));
  }
}


void
pfq_rwlock_read_unlock(pfq_rwlock_t *l)
{
  uint32_t ticket = atomic_fetch_add_explicit(&l->r_out, READER_INCREMENT, memory_order_acq_rel);

  if (ticket & WRITER_PRESENT) {
    //----------------------------------------------------------------------------
    // finish reading counter before reading last
    //----------------------------------------------------------------------------
    if (ticket == atomic_load_explicit(&l->last, memory_order_acquire))
      atomic_store_explicit(&l->whead->blocked, false, memory_order_release);
  }
}


void
pfq_rwlock_write_lock(pfq_rwlock_t *l, pfq_rwlock_node_t *me)
{
  //--------------------------------------------------------------------
  // use MCS lock to enforce mutual exclusion with other writers
  //--------------------------------------------------------------------
  mcs_lock(&l->wtail, me);

  //--------------------------------------------------------------------
  // this may be false when at the head of the mcs queue
  //--------------------------------------------------------------------
  atomic_store_explicit(&me->blocked, true, memory_order_relaxed);

  //--------------------------------------------------------------------
  // announce myself as next writer
  //--------------------------------------------------------------------
  l->whead = me;

  //--------------------------------------------------------------------
  // set writer_blocking_readers to block any readers in the next batch
  //--------------------------------------------------------------------
  uint32_t phase = atomic_load_explicit(&l->r_in, memory_order_relaxed) & PHASE_BIT;
  atomic_store_explicit(&l->writer_blocking_readers[phase].bit, true, memory_order_release);

  //----------------------------------------------------------------------------
  // store to writer_blocking_headers bit must complete before incrementing r_in
  //----------------------------------------------------------------------------

  //--------------------------------------------------------------------
  // acquire an "in" sequence number to see how many readers arrived
  // set the WRITER_PRESENT bit so subsequent readers will wait
  //--------------------------------------------------------------------
  uint32_t in = atomic_fetch_or_explicit(&l->r_in, WRITER_PRESENT, memory_order_acq_rel);

  //--------------------------------------------------------------------
  // save the ticket that the last reader will see
  //--------------------------------------------------------------------
  atomic_store_explicit(&l->last, in - READER_INCREMENT + WRITER_PRESENT, memory_order_release);

  //-------------------------------------------------------------
  // update to 'last' must complete before others see changed value of r_out.
  // acquire an "out" sequence number to see how many readers left
  // set the WRITER_PRESENT bit so the last reader will know to signal
  // it is responsible for signaling the waiting writer
  //-------------------------------------------------------------
  uint32_t out = atomic_fetch_or_explicit(&l->r_out, WRITER_PRESENT, memory_order_acq_rel);

  //--------------------------------------------------------------------
  // if any reads are active, wait for last reader to signal me
  //--------------------------------------------------------------------
  if (in != out) {
    while (atomic_load_explicit(&me->blocked, memory_order_acquire));
    // wait for active reads to drain

    //--------------------------------------------------------------------------
    // store to writer_blocking headers bit must complete before notifying
    // readers of writer
    //--------------------------------------------------------------------------
  }
}


void
pfq_rwlock_write_unlock(pfq_rwlock_t *l, pfq_rwlock_node_t *me)
{
  //--------------------------------------------------------------------
  // toggle phase and clear WRITER_PRESENT in r_in. No synch issues
  // since there are no concurrent updates of the low-order byte
  //--------------------------------------------------------------------
  unsigned char *lsb = LSB_PTR(&l->r_in);
  uint32_t phase = *lsb & PHASE_BIT;
  *lsb ^= WRITER_MASK;

  //--------------------------------------------------------------------
  // toggle phase and clear WRITER_PRESENT in r_out. No synch issues
  // since the low-order byte modified here isn't modified again until
  // another writer has the mcs_lock.
  //--------------------------------------------------------------------
  lsb = LSB_PTR(&l->r_out);
  *lsb ^= WRITER_MASK;

  //----------------------------------------------------------------------------
  // clearing writer present in r_in can be reordered with writer_blocking_readers set below
  // because any arriving reader will see the cleared writer_blocking_readers and proceed.
  //----------------------------------------------------------------------------

  //--------------------------------------------------------------------
  // clear writer_blocking_readers to release waiting readers in the current read phase
  //--------------------------------------------------------------------
  atomic_store_explicit(&l->writer_blocking_readers[phase].bit, false, memory_order_release);

  //--------------------------------------------------------------------
  // pass writer lock to next writer
  //--------------------------------------------------------------------
  mcs_unlock(&l->wtail, me);
}
