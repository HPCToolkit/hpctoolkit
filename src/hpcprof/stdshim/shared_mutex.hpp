// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_STDSHIM_SHARED_MUTEX_H
#define HPCTOOLKIT_STDSHIM_SHARED_MUTEX_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritance
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <shared_mutex>

#include "version.hpp"

#ifdef HPCTOOLKIT_STDSHIM_STD_HAS_shared_lock
#include <shared_mutex>
#else
#include <mutex>
#include <stdexcept>
#endif

#if defined(HPCTOOLKIT_STDSHIM_STD_HAS_shared_mutex) || defined(HPCTOOLKIT_STDSHIM_STD_HAS_shared_timed_mutex)
#include <shared_mutex>
#else
#include <atomic>
#include <mutex>
#include <condition_variable>
#endif

namespace hpctoolkit::stdshim {

namespace detail {

// Reader-writer locks have an interesting history, shared_timed_mutex became
// standard in C++14, but the untimed version only became standard in C++17.
// So we use the timed version if we don't have the untimed, and our own thing
// if neither are available (since Boost's implementation isn't great).
#ifdef HPCTOOLKIT_STDSHIM_STD_HAS_shared_mutex
using shared_mutex = std::shared_mutex;
#else  // HPCTOOLKIT_STDSHIM_STD_HAS_shared_mutex
class shared_mutex {
public:
  shared_mutex();
  shared_mutex(const shared_mutex&) = delete;
  ~shared_mutex();
  shared_mutex& operator=(const shared_mutex&) = delete;
  void lock();
  bool try_lock();
  void unlock();
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();
private:
  std::atomic<unsigned int> incoming;
  std::atomic<unsigned int> outgoing;
  unsigned int last;
  std::mutex unique;

  std::mutex rlock;
  std::condition_variable rcond;
  bool rwakeup[2];

  std::mutex wlock;
  std::condition_variable wcond;
  bool wwakeup;
};
#endif

}  // namespace detail

// In any event, whatever choice of shared_mutex above has to be wrapped for
// adding Valgrind annotations. Because Helgrind is missing the r->w->r h-b arcs
// that are needed to keep things working around here.
class shared_mutex {
public:
  shared_mutex();
  shared_mutex(const shared_mutex&) = delete;
  ~shared_mutex();
  shared_mutex& operator=(const shared_mutex&) = delete;
  void lock();
  bool try_lock();
  void unlock();
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();
private:
  detail::shared_mutex real;
  // These two variables are used for tracking the lock's state with Helgrind
  char arc_rw;
  char arc_wr;
};


}  // namespace hpctoolkit::stdshim

#endif  // HPCTOOLKIT_STDSHIM_SHARED_MUTEX_H
