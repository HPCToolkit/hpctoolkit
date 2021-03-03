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
  // MT: Internally Synchonized
  template<class F, class... Args>
  void call(F&& f, Args&&... args) {
    call_detail(true, [&]{
      std::forward<F>(f)(std::forward<Args>(args)...);
    });
  }

  /// Call the function, once. All caveats of std::call_once apply.
  /// Does not wait for the function to complete before returning.
  // MT: Internally Synchonized, Unsafe (provides no synchronization)
  template<class F, class... Args>
  void call_nowait(F&& f, Args&&... args) {
    call_detail(false, [&]{
      std::forward<F>(f)(std::forward<Args>(args)...);
    });
  }

  /// Check whether this flag has been call()'d.
  // MT: Internally Synchonized, Unstable
  bool query();

private:
  void call_detail(bool, const std::function<void(void)>&);
  enum class Status : std::uint32_t { initial, inprogress, completed };
  stdshim::atomic<Status> status;
};

};

#endif  // HPCTOOLKIT_PROFILE_UTIL_ONCE_H
