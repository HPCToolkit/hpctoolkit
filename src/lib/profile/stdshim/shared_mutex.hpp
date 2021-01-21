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

#ifndef HPCTOOLKIT_STDSHIM_SHARED_MUTEX_H
#define HPCTOOLKIT_STDSHIM_SHARED_MUTEX_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritence
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <shared_mutex>

#include "version.hpp"

#if STD_HAS(shared_lock)
#include <shared_mutex>
#else
#include <mutex>
#include <stdexcept>
#endif

#if STD_HAS(shared_mutex) || STD_HAS(shared_timed_mutex)
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
#if STD_HAS(shared_mutex)
using shared_mutex = std::shared_mutex;
#else  // STD_HAS(shared_mutex)
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
  char arc_rw;
  char arc_wr;
};


}  // namespace hpctoolkit::stdshim

#ifndef STDSHIM_DONT_UNDEF
#undef STD_HAS
#endif

#endif  // HPCTOOLKIT_STDSHIM_SHARED_MUTEX_H
