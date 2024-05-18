// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_ONCE_H
#define HPCTOOLKIT_PROFILE_UTIL_ONCE_H

#include "../stdshim/atomic.hpp"

#include <atomic>
#include <thread>
#include <stdexcept>
#include <future>
#include <functional>
#include <mutex>

namespace hpctoolkit::util {

/// Effectively a version of std::call_once that doesn't use a lambda and can
/// only be "called once" from a single thread.
class Once {
public:
  Once();
  ~Once() = default;

  /// Scope-class for handling the call_once semantics.
  class Caller {
  public:
    Caller() = delete;
    ~Caller();

  private:
    friend class Once;
    Caller(Once& o) : once(o) {};
    Once& once;
  };

  /// Get a Caller scope to use for this Once.
  Caller signal();

  /// Wait for the Once to signal.
  void wait() const;

private:
  std::atomic<std::thread::id> callerId;
  std::promise<void> promise;
  std::future<void> future;
};

/// Just your ordinary std::call_once with Valgrind annotation support.
void call_once_detail(std::once_flag&, const std::function<void(void)>&);
template<class F, class... Args>
void call_once(std::once_flag& flag, F&& f, Args&&... args) {
  call_once_detail(flag, [&]{
    std::forward<F>(f)(std::forward<Args>(args)...);
  });
}

/// Variant of std::call_once, that can be queried for the current status.
class OnceFlag {
public:
  OnceFlag();
  ~OnceFlag() = default;

  /// Call the function, once. All caveats of std::call_once apply.
  // MT: Internally Synchronized
  template<class F, class... Args>
  void call(F&& f, Args&&... args) {
    call_detail(true, [&]{
      std::forward<F>(f)(std::forward<Args>(args)...);
    });
  }

  /// Call the function, once. All caveats of std::call_once apply.
  /// Does not wait for the function to complete before returning.
  // MT: Internally Synchronized, Unsafe (provides no synchronization)
  template<class F, class... Args>
  void call_nowait(F&& f, Args&&... args) {
    call_detail(false, [&]{
      std::forward<F>(f)(std::forward<Args>(args)...);
    });
  }

  /// Check whether this flag has been call()'d.
  // MT: Internally Synchronized, Unstable
  bool query();

private:
  void call_detail(bool, const std::function<void(void)>&);
  enum class Status : std::uint32_t { initial, inprogress, completed };
  stdshim::atomic<Status> status;
};

};

#endif  // HPCTOOLKIT_PROFILE_UTIL_ONCE_H
