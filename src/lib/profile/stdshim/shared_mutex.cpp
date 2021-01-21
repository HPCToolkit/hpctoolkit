// -*-Mode: C++;-*-

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
// Copyright ((c)) 2019-2020, Rice University
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

#include "../util/vgannotations.hpp"

#define STDSHIM_DONT_UNDEF
#include "shared_mutex.hpp"

using namespace hpctoolkit::stdshim;

// Valgrind wrapper for stdshim::shared_mutex. Presumes that the real lock is
// visible to Valgrind, and so only adds the missing arcs.
shared_mutex::shared_mutex() {}
shared_mutex::~shared_mutex() {}
void shared_mutex::lock() {
  real.lock();
  ANNOTATE_HAPPENS_AFTER(&arc_rw);  // The readers are done as of now.
}
bool shared_mutex::try_lock() {
  if(!real.try_lock()) return false;
  ANNOTATE_HAPPENS_AFTER(&arc_rw);  // The readers are done as of now.
  return true;
}
void shared_mutex::unlock() {
  ANNOTATE_HAPPENS_BEFORE(&arc_wr);
  real.unlock();
}
void shared_mutex::lock_shared() {
  real.lock_shared();
  ANNOTATE_HAPPENS_AFTER(&arc_wr);  // The (last) writer is done as of now.
}
bool shared_mutex::try_lock_shared() {
  if(!real.try_lock_shared()) return false;
  ANNOTATE_HAPPENS_AFTER(&arc_wr);  // The (last) writer is done as of now.
  return true;
}
void shared_mutex::unlock_shared() {
  ANNOTATE_HAPPENS_BEFORE(&arc_rw);
  real.unlock_shared();
}

#if !STD_HAS(shared_mutex) && !STD_HAS(shared_timed_mutex)

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
  // For simplicity, at this point we're commited
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
