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
// Copyright ((c)) 2019-2022, Rice University
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

  /// Combine two Results, as if they came from a single (combined) Workshare.
  WorkshareResult operator+(const WorkshareResult& o) const noexcept {
    return {contributed || o.contributed, completed && o.completed};
  }

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
  ParallelForEach() = default;
  ~ParallelForEach() = default;

  ParallelForEach(ParallelForEach&&) = delete;
  ParallelForEach(const ParallelForEach&) = delete;
  ParallelForEach& operator=(ParallelForEach&&) = delete;
  ParallelForEach& operator=(const ParallelForEach&) = delete;

  /// Fill the workqueue with work to be distributed among contributors,
  /// as well as set the action to perform on workitems and the blocking size.
  // MT: Externally Synchronized, Internally Synchronized with contribute().
  void fill(std::vector<T> items, std::function<void(T&)> f = nullptr,
            std::size_t bs = 0) noexcept {
    workitems = std::move(items);
    if(f) action = f;
    if(bs > 0) blockSize = bs;
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
  std::function<void(T&)> action;
  std::size_t blockSize = 1;
  std::vector<T> workitems;
  std::atomic<std::size_t> nextitem{std::numeric_limits<std::size_t>::max()};
  std::atomic<std::size_t> doneitemcnt{std::numeric_limits<std::size_t>::max()};
};

/// Parallel version of a simple for loop starting at 0 and incrementing.
/// In effect, a simplified version of ParallelForEach that doesn't spend extra
/// memory for no reason.
class ParallelFor {
public:
  ParallelFor() = default;
  ~ParallelFor() = default;

  ParallelFor(ParallelFor&&) = delete;
  ParallelFor(const ParallelFor&) = delete;
  ParallelFor& operator=(ParallelFor&&) = delete;
  ParallelFor& operator=(const ParallelFor&) = delete;

  /// Start the loop with work counting from 0 to `n`, as well as set the action
  /// and the blocking size.
  // MT: Externally Synchronized, Internally Synchronized with contribute().
  void fill(std::size_t n, std::function<void(std::size_t)> f = nullptr,
            std::size_t bs = 0) noexcept {
    workitems = n;
    if(f) action = f;
    if(bs > 0) blockSize = bs;
    doneitemcnt.store(0, std::memory_order_relaxed);
    ANNOTATE_HAPPENS_BEFORE(&nextitem);
    nextitem.store(0, std::memory_order_release);
  }

  /// Reset the workshare, allowing it to be used again. Note that this function
  /// must be externally synchronized w.r.t. any contributing threads.
  // MT: Externally Synchronized
  void reset() noexcept {
    workitems = 0;
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
      if(val > workitems) return {false, true};
      end = std::min(val+blockSize, workitems);
    } while(!nextitem.compare_exchange_weak(val, end,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed));
    for(std::size_t i = val; i < end; ++i) action(i);
    ANNOTATE_HAPPENS_BEFORE(&doneitemcnt);
    doneitemcnt.fetch_add(end-val, std::memory_order_release);
    return {true, end >= workitems};
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
    while(doneitemcnt.load(std::memory_order_acquire) < workitems)
      std::this_thread::yield();
    ANNOTATE_HAPPENS_AFTER(&doneitemcnt);
  }

private:
  std::function<void(std::size_t)> action;
  std::size_t blockSize = 1;
  std::size_t workitems;
  std::atomic<std::size_t> nextitem{std::numeric_limits<std::size_t>::max()};
  std::atomic<std::size_t> doneitemcnt{std::numeric_limits<std::size_t>::max()};
};

/// Variant of ParallelForEach that allows for `reset()` to be called in
/// parallel with `contribute()`. As such, this is only "complete" after
/// `complete()` has been called (usually by the main thread).
template<class T>
class ResettableParallelForEach : protected ParallelForEach<T> {
public:
  ResettableParallelForEach() = default;
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
  std::atomic<bool> completed{false};
};

/// Resettable variant of ParallelFor, identical to ResettableForEach.
class ResettableParallelFor : protected ParallelFor {
public:
  ResettableParallelFor() = default;
  ~ResettableParallelFor() = default;

  ResettableParallelFor(ResettableParallelFor&&) = delete;
  ResettableParallelFor(const ResettableParallelFor&) = delete;
  ResettableParallelFor& operator=(ResettableParallelFor&&) = delete;
  ResettableParallelFor& operator=(const ResettableParallelFor&) = delete;

  using ParallelFor::fill;
  using ParallelFor::wait;
  using ParallelFor::loop;

  /// Reset this workshare, allowing it to be filled and used again.
  /// Calls `contribute(wait_t)` internally.
  // MT: Internally Synchronized
  void reset() noexcept {
    ParallelFor::contribute(wait());
    ParallelFor::reset();
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
    auto res = ParallelFor::contribute();
    if(res.completed) res.completed = completed.load(std::memory_order_acquire);
    return res;
  }

  /// Contribute to the workshare until there is no more currently available
  /// work to be shared.
  /// Note that this does not wait for contributors to complete, just that there
  /// is no more work to be allocated.
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute(typename ParallelFor::loop_t) noexcept {
    ParallelFor::contribute(loop());
    return {false, completed.load(std::memory_order_acquire)};
  }

  /// Contribute to the workshare and wait until all work has completed.
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute(typename ParallelFor::wait_t) noexcept {
    ParallelFor::contribute(wait());
    return {false, completed.load(std::memory_order_acquire)};
  }

private:
  std::atomic<bool> completed{false};
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_ONCE_H
