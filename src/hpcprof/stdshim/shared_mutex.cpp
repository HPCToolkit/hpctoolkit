// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "../util/vgannotations.hpp"

#include "shared_mutex.hpp"

using namespace hpctoolkit::stdshim;

// Valgrind wrapper for stdshim::shared_mutex. Presumes that the real lock is
// visible to Valgrind, and so only adds the missing arcs.
shared_mutex::shared_mutex() {}
shared_mutex::~shared_mutex() {}
void shared_mutex::lock() {
  real.lock();
  (void)&arc_rw;  // Avoid compiler warnings
  ANNOTATE_HAPPENS_AFTER(&arc_rw);  // The readers are done as of now.
}
bool shared_mutex::try_lock() {
  if(!real.try_lock()) return false;
  (void)&arc_rw;  // Avoid compiler warnings
  ANNOTATE_HAPPENS_AFTER(&arc_rw);  // The readers are done as of now.
  return true;
}
void shared_mutex::unlock() {
  (void)&arc_wr;  // Avoid compiler warnings
  ANNOTATE_HAPPENS_BEFORE(&arc_wr);
  real.unlock();
}
void shared_mutex::lock_shared() {
  real.lock_shared();
  (void)&arc_wr;  // Avoid compiler warnings
  ANNOTATE_HAPPENS_AFTER(&arc_wr);  // The (last) writer is done as of now.
}
bool shared_mutex::try_lock_shared() {
  if(!real.try_lock_shared()) return false;
  (void)&arc_wr;  // Avoid compiler warnings
  ANNOTATE_HAPPENS_AFTER(&arc_wr);  // The (last) writer is done as of now.
  return true;
}
void shared_mutex::unlock_shared() {
  (void)&arc_rw;  // Avoid compiler warnings
  ANNOTATE_HAPPENS_BEFORE(&arc_rw);
  real.unlock_shared();
}

#if !defined(HPCTOOLKIT_STDSHIM_STD_HAS_shared_mutex) && !defined(HPCTOOLKIT_STDSHIM_STD_HAS_shared_timed_mutex)

static constexpr unsigned int phase  = 1 << 0;
static constexpr unsigned int writer = 1 << 1;
static constexpr unsigned int ticket = 1 << 2;

detail::shared_mutex::shared_mutex()
  : incoming(0), outgoing(0), unique(), rlock(), rcond(), rwakeup{false, false},
    wlock(), wcond(), wwakeup(false) {
  ANNOTATE_RWLOCK_CREATE(this);
}

detail::shared_mutex::~shared_mutex() {
  ANNOTATE_RWLOCK_DESTROY(this);
}

// Writer methods
void detail::shared_mutex::lock() {
  unique.lock();
  auto in = incoming.fetch_xor(phase | writer, std::memory_order_acquire);
  last = (in - ticket) ^ (phase | writer);
  auto out = outgoing.fetch_xor(phase | writer, std::memory_order_acq_rel);
  if(in != out) {
    std::unique_lock<std::mutex> l(wlock);
    wcond.wait(l, [this]{ return wwakeup; });
    wwakeup = false;
  }
  rwakeup[in & phase] = false;
  ANNOTATE_RWLOCK_ACQUIRED(this, 1);
}

bool detail::shared_mutex::try_lock() {
  if(!unique.try_lock()) return false;
  // For simplicity, at this point we're committed
  auto in = incoming.fetch_xor(phase | writer, std::memory_order_acquire);
  last = (in - ticket) ^ (phase | writer);
  auto out = outgoing.fetch_xor(phase | writer, std::memory_order_acq_rel);
  if(in != out) {
    std::unique_lock<std::mutex> l(wlock);
    wcond.wait(l, [this]{ return wwakeup; });
    wwakeup = false;
  }
  rwakeup[in & phase] = false;
  ANNOTATE_RWLOCK_ACQUIRED(this, 1);
  return true;
}

void detail::shared_mutex::unlock() {
  ANNOTATE_RWLOCK_RELEASED(this, 1);
  outgoing.fetch_xor(writer, std::memory_order_relaxed);
  auto p = incoming.fetch_xor(writer, std::memory_order_acq_rel) & phase;
  {
    std::unique_lock<std::mutex> l(rlock);
    rwakeup[p] = true;
  }
  rcond.notify_all();
  unique.unlock();
}

// Reader methods
void detail::shared_mutex::lock_shared() {
  auto t = incoming.fetch_add(ticket, std::memory_order_acquire);
  if(t & writer) {
    std::unique_lock<std::mutex> l(rlock);
    rcond.wait(l, [this,&t]{ return rwakeup[t & phase]; });
  }
  ANNOTATE_RWLOCK_ACQUIRED(this, 0);
}

bool detail::shared_mutex::try_lock_shared() {
  auto t = incoming.fetch_add(ticket, std::memory_order_acquire);
  if(!(t & writer)) {
    ANNOTATE_RWLOCK_ACQUIRED(this, 0);
    return true;
  }
  t = outgoing.fetch_add(ticket, std::memory_order_acq_rel);
  if((t & writer) && t == last) {
    {
      std::unique_lock<std::mutex> l(wlock);
      wwakeup = true;
    }
    wcond.notify_one();
  }
  return false;
}

void detail::shared_mutex::unlock_shared() {
  ANNOTATE_RWLOCK_RELEASED(this, 0);
  auto t = outgoing.fetch_add(ticket, std::memory_order_acq_rel);
  if((t & writer) && t == last) {
    {
      std::unique_lock<std::mutex> l(wlock);
      wwakeup = true;
    }
    wcond.notify_one();
  }
}

#endif
