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

#ifndef HPCTOOLKIT_PROFILE_UTIL_PARALLEL_WORK_H
#define HPCTOOLKIT_PROFILE_UTIL_PARALLEL_WORK_H

#include "vgannotations.hpp"

#include <atomic>
#include <limits>
#include <functional>
#include <thread>
#include <vector>

namespace hpctoolkit::util {

/// Helper structure for the results of a workshare contribution.
struct WorkshareResult final {
  WorkshareResult(bool a, bool b) : contributed(a), completed(b) {};
  ~WorkshareResult() = default;

  WorkshareResult(const WorkshareResult&) = default;
  WorkshareResult(WorkshareResult&&) = default;
  WorkshareResult& operator=(const WorkshareResult&) = default;
  WorkshareResult& operator=(WorkshareResult&&) = default;

  /// Whether the request managed to contribute any work to the workshare.
  bool contributed : 1;
  /// Whether any work remains in the workshare for later calls.
  bool completed : 1;
};

/// Parallel version of std::for_each, that allows for multiple threads to
/// contribute their cycles at will and on a whim.
template<class T>
class ParallelForEach {
public:
  ParallelForEach(std::function<void(T&)> f, std::size_t blockSize = 1)
    : action(std::move(f)), blockSize(blockSize),
      nextitem(std::numeric_limits<std::size_t>::max()),
      doneitemcnt(std::numeric_limits<std::size_t>::max()) {};
  ~ParallelForEach() = default;

  ParallelForEach(ParallelForEach&&) = delete;
  ParallelForEach(const ParallelForEach&) = delete;
  ParallelForEach& operator=(ParallelForEach&&) = delete;
  ParallelForEach& operator=(const ParallelForEach&) = delete;

  /// Fill the workqueue with work to be distributed among contributors.
  // MT: Externally Synchronized, Internally Synchronized with contribute().
  void fill(std::vector<T> items) noexcept {
    workitems = std::move(items);
    doneitemcnt.store(0, std::memory_order_relaxed);
    ANNOTATE_HAPPENS_BEFORE(&nextitem);
    nextitem.store(0, std::memory_order_release);
  }

  /// Reset the workshare, allowing it to be used again. Note that this function
  /// must be externally synchronized w.r.t. any contributing threads.
  // MT: Externally Synchronized
  void reset() noexcept {
    workitems.clear();
    nextitem.store(std::numeric_limits<std::size_t>::max(), std::memory_order_relaxed);
    doneitemcnt.store(std::numeric_limits<std::size_t>::max(), std::memory_order_relaxed);
  }

  /// Contribute to the workshare by processing a block of items, if any are
  /// currently available. Returns the result of this request.
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute() noexcept {
    auto val = nextitem.load(std::memory_order_acquire);
    ANNOTATE_HAPPENS_AFTER(&nextitem);
    if(val == std::numeric_limits<std::size_t>::max()) return {false, false};
    std::size_t end;
    do {
      if(val > workitems.size()) return {false, true};
      end = std::min(val+blockSize, workitems.size());
    } while(!nextitem.compare_exchange_weak(val, end,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed));
    for(std::size_t i = val; i < end; ++i) action(workitems[i]);
    ANNOTATE_HAPPENS_BEFORE(&doneitemcnt);
    doneitemcnt.fetch_add(end-val, std::memory_order_release);
    return {true, end >= workitems.size()};
  }

  struct loop_t {};
  constexpr inline loop_t loop() { return {}; }

  /// Contribute to the workshare until there is no more work to be shared.
  /// Note that this does not wait for contributors to complete, just that there
  /// is no more work to be allocated.
  // MT: Internally Synchronized
  void contribute(loop_t) noexcept {
    WorkshareResult res{false, false};
    do {
      res = contribute();
      if(!res.contributed) std::this_thread::yield();
    } while(!res.completed);
  }

  struct wait_t {};
  constexpr inline wait_t wait() { return {}; }

  /// Contribute to the workshare and wait until all work has completed.
  // MT: Internally Synchronized
  void contribute(wait_t) noexcept {
    contribute(loop());
    while(doneitemcnt.load(std::memory_order_acquire) < workitems.size())
      std::this_thread::yield();
    ANNOTATE_HAPPENS_AFTER(&doneitemcnt);
  }

private:
  const std::function<void(T&)> action;
  const size_t blockSize;
  std::vector<T> workitems;
  std::atomic<size_t> nextitem;
  std::atomic<size_t> doneitemcnt;
};

/// Variant of ParallelForEach that allows for `reset()` to be called in
/// parallel with `contribute()`. As such, this is only "complete" after
/// `complete()` has been called (usually by the main thread).
template<class T>
class ResettableParallelForEach : protected ParallelForEach<T> {
public:
  template<class... Args>
  ResettableParallelForEach(Args&&... args)
    : ParallelForEach<T>(std::forward<Args>(args)...), completed(false) {};
  ~ResettableParallelForEach() = default;

  ResettableParallelForEach(ResettableParallelForEach&&) = delete;
  ResettableParallelForEach(const ResettableParallelForEach&) = delete;
  ResettableParallelForEach& operator=(ResettableParallelForEach&&) = delete;
  ResettableParallelForEach& operator=(const ResettableParallelForEach&) = delete;

  using ParallelForEach<T>::fill;
  using ParallelForEach<T>::wait;
  using ParallelForEach<T>::loop;

  /// Reset this workshare, allowing it to be filled and used again.
  /// Calls `contribute(wait_t)` internally.
  // MT: Internally Synchronized
  void reset() noexcept {
    ParallelForEach<T>::contribute(wait());
    ParallelForEach<T>::reset();
  }

  /// Allow this workshare to complete. Do not call `fill` after this.
  /// Note that this still requires a call to `fill` if the workshare is empty
  /// (i.e. after construction or a call to `reset`).
  // MT: Internally Synchronized
  void complete() noexcept {
    completed.store(true, std::memory_order_release);
  }

  /// Contribute to the workshare by processing a block of items, if any are
  /// currently available. Returns the result of this request.
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute() noexcept {
    auto res = ParallelForEach<T>::contribute();
    if(res.completed) res.completed = completed.load(std::memory_order_acquire);
    return res;
  }

  /// Contribute to the workshare until there is no more currently available
  /// work to be shared.
  /// Note that this does not wait for contributors to complete, just that there
  /// is no more work to be allocated.
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute(typename ParallelForEach<T>::loop_t) noexcept {
    ParallelForEach<T>::contribute(loop());
    return {false, completed.load(std::memory_order_acquire)};
  }

  /// Contribute to the workshare and wait until all work has completed.
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute(typename ParallelForEach<T>::wait_t) noexcept {
    ParallelForEach<T>::contribute(wait());
    return {false, completed.load(std::memory_order_acquire)};
  }

private:
  std::atomic<bool> completed;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_ONCE_H
