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

#include <assert.h>
#include <atomic>
#include <limits>
#include <functional>
#include <shared_mutex>
#include <thread>
#include <variant>
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

namespace detail {

/// Core implementation mixin behind all the ParallelFor worksharing classes.
///
/// This class implements a simple worksharing construct, where worker threads
/// are able to (attempt to) contribute() and perform work that a master thread
/// has supplied with rawFill(). "Work" here is simply unsigned integer indices,
/// derived classes may provide a cleaner interface, see ParallelForEach.
///
/// Conceptually, a ParallelFor object can be in one of three states:
///   - The "initial" state, before rawFill() has provided work to be performed,
///   - The "working" state, when work is actively being processed by worker
///     threads via contribute(), and
///   - The "completed" state, after all work has been processed and no new work
///     will be added.
/// To assist the description of parallel operations, we define the state as
/// "stable" from a thread's perspective if that thread has performed the
/// appropriate synchronization (waitForStable()) to ensure that no worker
/// threads are still performing work. Before waitForStable() is called, the
/// object (from a thread's perspective) may be in one of two pseudo-states:
///   - The "unstable initial" psuedo-state for the "initial" state, and
///   - The "unstable completed" pseudo-state for the "completed" state.
/// Note that the the "working" state is always considered "unstable."
///
/// The methods on this class transition the object as a whole between these
/// three states, while remaining thread-safe between workers and the master
/// thread(s). Derived classes are allowed to choose the transition behavior of
/// contribute() for their own needs, see the documentation there for details.
///
/// \mermaid
///   %% Copy the below into https://mermaid.live for a live preview
///   stateDiagram-v2
///   [*] --> Initial
///   Initial --> Working: rawFill()
///
///   state if <<choice>>
///   Working --> if: contribute()
///   if --> Working
///   if --> UnstableInitial
///   if --> UnstableCompleted
///
///   UnstableInitial: Unstable Initial
///   UnstableInitial --> Initial: waitForStable()
///   UnstableCompleted: Unstable Completed
///   UnstableCompleted --> Completed: waitForStable()
///
///   Initial --> Completed: forceComplete()
///   Completed --> [*]
/// \endmermaid
template<class Derived, class Counter = std::size_t>
class ParallelForCore {
private:
  static_assert(std::is_integral_v<Counter> && !std::numeric_limits<Counter>::is_signed,
      "CounterType must be an unsigned integral value!");

  /// Reserved value for #inCounter indicating the "initial" state
  static const inline Counter cntInitial = 0;
  /// Reserved value for #inCounter indicating the "completed" state
  static const inline Counter cntCompleted = 1;
  /// First value for #inCounter and #outCounter, larger than any reserved value
  static const inline Counter cntOffset = 2;

  /// Counter for worker threads entering the workshare. Counts up to
  /// #maxCounter - 1 as work items are allocated to worker threads.
  ///
  /// May also take on the special values #cntInitial and #cntCompleted to
  /// indicate when the workshare is in the "initial" or "completed" states.
  /// Otherwise is >= cntOffset.
  std::atomic<Counter> inCounter = cntInitial;

  /// Counter for worker threads exiting the workshare. Counts up to #maxCounter
  /// as worker threads complete work items.
  std::atomic<Counter> outCounter = cntInitial;

  /// Maximum value for #inCounter and #outCounter. Indicates the number of
  /// workitems that have been added via a call to rawFill(), plus #cntOffset.
  Counter maxCounter = cntInitial;

  /// Number of work-items a worker thread will attempt to allocate for itself
  /// at once.
  Counter blockSize = 1;

protected:
  ParallelForCore() = default;
  ~ParallelForCore() = default;

  ParallelForCore(ParallelForCore&&) = delete;
  ParallelForCore(const ParallelForCore&) = delete;
  ParallelForCore& operator=(ParallelForCore&&) = delete;
  ParallelForCore& operator=(const ParallelForCore&) = delete;

  /// Add work to the workshare, to be done by worker threads calling contribute().
  /// Must have called waitForStable() before this to ensure no worker threads
  /// are working on work from a previous call.
  ///
  /// Cannot be called after forceComplete() or after shouldWorkingToInitial()
  /// returns `false`. See contribute() for details.
  ///
  /// State transition matrix:
  ///   - "initial" -> "working"
  ///   - Any other state -> error
  // MT: Externally Synchronized, Internally Synchronized with contribute()
  void rawFill(Counter numItems, Counter newBlockSize = 0) {
    assert(inCounter.load(std::memory_order_relaxed) == cntInitial
           && "Attempt to call rawFill() in a non-initial state!");
    assert(outCounter.load(std::memory_order_relaxed) >= maxCounter
           && "Attempt to call rawFill() before waitForStable()!");

    // Set the parameters for this batch of work
    if(newBlockSize > 0) blockSize = newBlockSize;
    maxCounter = cntOffset + numItems;

    // Reset outCounter to cntOffset, to count up to maxCounter after numItems
    // work-items have been processed.
    outCounter.store(cntOffset, std::memory_order_relaxed);

    // Allow workers to work by transitioning to the "working" state
    ANNOTATE_HAPPENS_BEFORE(&inCounter);
    inCounter.store(cntOffset, std::memory_order_release);
  }

private:
  /// Allocate some work for the calling thread, or fail if no work is available.
  ///
  /// The last caller of this function marks that no more work is available by
  /// transitioning the state to "initial" or "completed", depending on whether
  /// work may be added later. The derived class implements
  /// `bool shouldWorkingToInitial()` to indicate which of these two is used.
  ///
  /// \return
  ///    A begin/end pair of allocated work-items (equal on failure)
  ///    and a boolean, true if the workshare is "completed", false otherwise.
  ///
  /// State transition matrix:
  ///   - "working" -> "working", "unstable initial", "unstable completed"
  ///   - Any other state -> unchanged
  // MT: Internally Synchronized
  [[nodiscard]] std::tuple<Counter, Counter, bool> acquireWork() noexcept {
    Counter endCount;
    Counter nextCounter;
    Counter beginCount = inCounter.load(std::memory_order_acquire);
    do {
      ANNOTATE_HAPPENS_BEFORE(&inCounter);
      if(beginCount == cntInitial) {
        // No work is available, but we haven't "completed" yet
        return {0, 0, false};
      }
      if(beginCount == cntCompleted) {
        // No work is available, we have "completed" now.
        return {0, 0, true};
      }
      assert(blockSize > 0 && maxCounter >= cntOffset);

      // Allocate up to blockSize work-items for myself, limited by maxCounter
      nextCounter = endCount = std::min(beginCount + blockSize, maxCounter);

      if(nextCounter == maxCounter) {
        // We are taking the last of the work-items. Indicate to everyone else
        // that no more work is available.
        nextCounter = static_cast<Derived*>(this)->shouldWorkingToInitial()
                      ? cntInitial : cntCompleted;
      }

      // NB: We acquire here on failure, since we have return cases inside this
      // loop that may expose synchronizations.
      // We don't strictly need to acquire on success, but GCC doesn't like it
      // when the failure model is stronger than the success model.
    } while(!inCounter.compare_exchange_weak(beginCount, nextCounter,
        std::memory_order_acquire));

    return {beginCount - cntOffset, endCount - cntOffset, nextCounter == cntCompleted};
  }

  /// Contribute to the workshare by (attempting to) perform some work.
  ///
  /// State transition matrix: see acquireWork()
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contribute() noexcept {
    auto [begin, end, completed] = acquireWork();
    if(begin < end) {
      // Perform the work allocated to us
      for(Counter i = begin; i < end; ++i)
        static_cast<Derived*>(this)->doWorkItem(i);

      // Report that we have completed some work
      ANNOTATE_HAPPENS_BEFORE(&outCounter);
      outCounter.fetch_add(end - begin, std::memory_order_release);
    }
    return {begin < end, completed};
  }

protected:
  /// Contribute to the workshare by performing work, so long as there's work
  /// available to be done.
  ///
  /// \return A WorkshareResult indicating the result of contribution.
  ///
  /// State transition matrix:
  ///   - "working" -> "unstable initial" if `.completed == false`
  ///   - "working" -> "unstable completed" if `.completed == true`
  ///   - "unstable initial", "initial" -> unchanged (`.completed == false`)
  ///   - "unstable completed", "completed" -> unchanged (`.completed == true`)
  // MT: Internally Synchronized
  [[nodiscard]] WorkshareResult contributeWhileAble() noexcept {
    bool anyContributed = false;
    WorkshareResult res{false, false};
    do {
      res = contribute();
      if(res.contributed) anyContributed = true;
    } while(res.contributed);
    res.contributed = anyContributed;
    return res;
  }

  /// Wait until the currently available work has completed processing.
  /// Contributes to the workshare while waiting.
  ///
  /// This may return immediately if rawFill() has not yet been called. To wait
  /// for all calls to rawFill() to occur, see contributeUntilComplete().
  ///
  /// \return A WorkshareResult indicating the result of contribution.
  ///
  /// State transition matrix:
  ///   - "working" -> "initial" if `.completed == false`
  ///   - "working" -> "completed" if `.completed == true`
  ///   - "unstable initial", "initial" -> "initial" (`.completed == false`)
  ///   - "unstable completed", "completed" -> "completed" (`.completed == true`)
  // MT: Internally Synchronized
  WorkshareResult contributeUntilEmpty() noexcept {
    auto res = contributeWhileAble();
    waitUntilStable();
    return res;
  }

  /// Contribute until all work that will ever be added via rawFill() has
  /// completed processing.
  ///
  /// This will block until another thread calls rawFill() and/or
  /// forceComplete(). To return immediately before these are called, see
  /// contributeUntilEmpty().
  ///
  /// State transition matrix:
  ///   - Any state -> "completed"
  // MT: Internally Synchronized
  void contributeUntilComplete() noexcept {
    WorkshareResult res(false, false);
    do {
      res = contribute();
      if(!res.contributed) std::this_thread::yield();
    } while(!res.completed);
    waitUntilStable();
  }

  /// Wait for all active worker threads to complete their work. Allows entry
  /// into the stable "initial" or "completed" states.
  ///
  /// State transition matrix:
  ///   - "unstable initial", "initial" -> "initial"
  ///   - "unstable completed", "completed" -> "completed"
  ///   - "working" -> "initial" or "completed"
  // MT: Externally Synchronized, Internally Synchronized with contribute()
  void waitUntilStable() noexcept {
    while(outCounter.load(std::memory_order_acquire) < maxCounter)
      std::this_thread::yield();
    ANNOTATE_HAPPENS_AFTER(&outCounter);
    assert(inCounter.load(std::memory_order_relaxed) < cntOffset
           && "inCounter is not cntInitial or cntCompleted, failed to stabilize state!");
  }

  /// Mark the workshare as "completed," indicating that no new work will be
  /// added later. Must have called waitForStable() before this to ensure
  /// that no worker threads are actively working.
  ///
  /// State transition matrix:
  ///   - "initial", "completed" -> "completed"
  ///   - Any other state -> error
  // MT: Externally Synchronized, Internally Synchronized with contribute()
  void forceComplete() noexcept {
    assert(inCounter.load(std::memory_order_relaxed) < cntOffset
           && "Attempt to call forceComplete() while work is available!");
    assert(outCounter.load(std::memory_order_relaxed) >= maxCounter
           && "Attempt to call forceComplete() before workers have finished!");

    inCounter.store(cntCompleted, std::memory_order_relaxed);
  }
};

}  // namespace detail

/// Parallel workshare for a simple for loop, starting at 0 and counting up.
///
/// This is a workshare that allows parallel worker threads to "contribute" by
/// doing some work provided by the caller of fill(). A worker thread can call
/// contributeWhileAble() at any time to (attempt to) perform work, or
/// (usually the master thread) calls contributeUntilComplete() to wait until
/// the entire batch of work has completed processing.
///
/// This most basic ParallelFor can only be used once. See RepeatingParallelFor
/// for a variant that can be used multiple times.
class ParallelFor : private detail::ParallelForCore<ParallelFor> {
private:
  friend class detail::ParallelForCore<ParallelFor>;
  bool shouldWorkingToInitial() const noexcept { return false; }
  void doWorkItem(std::size_t i) noexcept {
    return action(i);
  }

public:
  ParallelFor() = default;
  ~ParallelFor() = default;

  ParallelFor(ParallelFor&&) = delete;
  ParallelFor(const ParallelFor&) = delete;
  ParallelFor& operator=(ParallelFor&&) = delete;
  ParallelFor& operator=(const ParallelFor&) = delete;

  /// Start the loop with work counting from 0 to `n`, as well as set the action
  /// and the block size. Can only be called once.
  // MT: Externally Synchronized, Internally Synchronized with contribute*()
  void fill(std::size_t n, std::function<void(std::size_t)> f = nullptr,
            std::size_t bs = 0) noexcept {
    if(f) action = f;
    assert(action);
    this->rawFill(n, bs);  // "initial" -> "working"
  }

  using detail::ParallelForCore<ParallelFor>::contributeWhileAble;
  using detail::ParallelForCore<ParallelFor>::contributeUntilComplete;

private:
  std::function<void(std::size_t)> action;
};


/// Variant of ParallelFor that iterates over a given std::vector.
/// See ParallelFor for details.
template<class T>
class ParallelForEach : private detail::ParallelForCore<ParallelForEach<T>> {
private:
  friend class detail::ParallelForCore<ParallelForEach<T>>;
  bool shouldWorkingToInitial() const noexcept { return false; }
  void doWorkItem(std::size_t i) noexcept {
    return action(workitems.at(i));
  }

public:
  ParallelForEach() = default;
  ~ParallelForEach() = default;

  ParallelForEach(ParallelForEach&&) = delete;
  ParallelForEach(const ParallelForEach&) = delete;
  ParallelForEach& operator=(ParallelForEach&&) = delete;
  ParallelForEach& operator=(const ParallelForEach&) = delete;

  /// Fill the workqueue with work to be distributed among contributors,
  /// as well as set the action to perform on workitems and the blocking size.
  /// Can only be called once.
  // MT: Externally Synchronized, Internally Synchronized with contribute().
  void fill(std::vector<T> items, std::function<void(T&)> f = nullptr,
            std::size_t bs = 0) noexcept {
    workitems = std::move(items);
    if(f) action = f;
    assert(action);
    this->rawFill(workitems.size(), bs);  // "initial" -> "working"
  }

  using detail::ParallelForCore<ParallelForEach>::contributeWhileAble;
  using detail::ParallelForCore<ParallelForEach>::contributeUntilComplete;

private:
  std::function<void(T&)> action;
  std::vector<T> workitems;
};


/// Repeating parallel workshare for a simple for loop, starting at 0 and
/// counting up.
///
/// This is a workshare that allows parallel worker threads to "contribute" by
/// doing some work provided by the caller of fill(). A worker thread can call
/// contributeWhileAble() at any time to (attempt to) perform work, or
/// (usually the master thread) calls contributeUntilWait() to wait until
/// a batch of work has completed processing. fill() may be called again to
/// add a new batch of work, or complete() can be called to indicate that no
/// more work will be added.
///
/// This advanced RepeatingParallelFor is slightly tricky to use correctly. See
/// ParallelFor for a simpler variant that can only be used once.
class RepeatingParallelFor : private detail::ParallelForCore<RepeatingParallelFor> {
private:
  friend class detail::ParallelForCore<RepeatingParallelFor>;
  bool shouldWorkingToInitial() const noexcept {
    return !completed.load(std::memory_order_relaxed);
  }
  void doWorkItem(std::size_t i) noexcept {
    return action(i);
  }

public:
  RepeatingParallelFor() = default;
  ~RepeatingParallelFor() = default;

  RepeatingParallelFor(RepeatingParallelFor&&) = delete;
  RepeatingParallelFor(const RepeatingParallelFor&) = delete;
  RepeatingParallelFor& operator=(RepeatingParallelFor&&) = delete;
  RepeatingParallelFor& operator=(const RepeatingParallelFor&) = delete;

  /// Start the loop with work counting from 0 to `n`, as well as set the action
  /// and the blocking size. Cannot be called after complete().
  // MT: Externally Synchronized, Internally Synchronized with contribute*()
  void fill(std::size_t n, std::function<void(std::size_t)> f = nullptr,
            std::size_t bs = 0) noexcept {
    assert(!completed.load(std::memory_order_relaxed));
    this->contributeUntilEmpty();  // * -> "initial"

    if(f) action = f;
    assert(action);
    this->rawFill(n, bs);  // "initial" -> "working"
  }

  /// Mark the loop as completed and wait for all work to complete.
  // MT: Externally Synchronized, Internally Synchronized with contribute*()
  void complete() noexcept {
    completed.store(true, std::memory_order_relaxed);
    this->contributeUntilEmpty();  // * -> "initial" or "complete"
    this->forceComplete();  // "initial" or "complete" -> "complete"
  }

  using detail::ParallelForCore<RepeatingParallelFor>::contributeWhileAble;
  using detail::ParallelForCore<RepeatingParallelFor>::contributeUntilEmpty;

private:
  std::function<void(std::size_t)> action;
  std::atomic<bool> completed = false;
};


/// Variant of RepeatingParallelFor that iterates over a given std::vector.
/// See RepeatingParallelFor for details.
template<class T>
class RepeatingParallelForEach : private detail::ParallelForCore<RepeatingParallelForEach<T>> {
private:
  friend class detail::ParallelForCore<RepeatingParallelForEach>;
  bool shouldWorkingToInitial() const noexcept {
    return !completed.load(std::memory_order_relaxed);
  }
  void doWorkItem(std::size_t i) noexcept {
    return action(workitems.at(i));
  }

public:
  RepeatingParallelForEach() = default;
  ~RepeatingParallelForEach() = default;

  RepeatingParallelForEach(RepeatingParallelForEach&&) = delete;
  RepeatingParallelForEach(const RepeatingParallelForEach&) = delete;
  RepeatingParallelForEach& operator=(RepeatingParallelForEach&&) = delete;
  RepeatingParallelForEach& operator=(const RepeatingParallelForEach&) = delete;

  /// Fill the workqueue with work to be distributed among contributors,
  /// as well as set the action to perform on workitems and the blocking size.
  /// Cannot be called after complete().
  // MT: Externally Synchronized, Internally Synchronized with contribute().
  void fill(std::vector<T> items, std::function<void(T&)> f = nullptr,
            std::size_t bs = 0) noexcept {
    assert(!completed.load(std::memory_order_relaxed));
    this->contributeUntilEmpty();  // * -> "initial"

    workitems = std::move(items);
    if(f) action = f;
    assert(action);
    this->rawFill(workitems.size(), bs);  // "initial" -> "working"
  }

  /// Mark the loop as completed and wait for all work to complete.
  // MT: Externally Synchronized, Internally Synchronized with contribute*()
  void complete() noexcept {
    completed.store(true, std::memory_order_relaxed);
    this->contributeUntilEmpty();  // * -> "initial" or "complete"
    this->forceComplete();  // "initial" or "complete" -> "complete"
  }

  using detail::ParallelForCore<RepeatingParallelForEach>::contributeWhileAble;
  using detail::ParallelForCore<RepeatingParallelForEach>::contributeUntilEmpty;

private:
  std::function<void(T&)> action;
  std::vector<T> workitems;
  std::atomic<bool> completed = false;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_ONCE_H
