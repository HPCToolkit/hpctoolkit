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

#include "once.hpp"

#if ENABLE_VG_ANNOTATIONS == 1
#include <valgrind/helgrind.h>
#include <valgrind/drd.h>
#else
#define ANNOTATE_HAPPENS_BEFORE(X)
#define ANNOTATE_HAPPENS_AFTER(X)
#endif

#include <stdexcept>
#include <thread>

using namespace hpctoolkit::internal;

Once::Once() : callerId(), promise(), future(promise.get_future()) {};

Once::Caller::~Caller() {
  ANNOTATE_HAPPENS_BEFORE(&once);
  once.callerId.exchange(std::thread::id(), std::memory_order_relaxed);
  once.promise.set_value();
}

Once::Caller Once::signal() {
  std::thread::id old;
  if(!callerId.compare_exchange_strong(old, std::this_thread::get_id(),
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed))
    throw std::logic_error("Once cannot be signal()'d more than once!");
  return Caller(*this);
}

void Once::wait() const {
  if(callerId.load(std::memory_order_relaxed) == std::this_thread::get_id())
    throw std::logic_error("Single-thread deadlock detected on Once!");
  future.wait();
  ANNOTATE_HAPPENS_AFTER(this);
}

void hpctoolkit::internal::call_once_detail(std::once_flag& flag,
                                            const std::function<void(void)>& f) {
  std::call_once(flag, [&]{
    f();
    ANNOTATE_HAPPENS_BEFORE(&flag);
  });
  ANNOTATE_HAPPENS_AFTER(&flag);
}

struct Status {
  static constexpr std::uint32_t initial = 0;
  static constexpr std::uint32_t inprogress = 1;
  static constexpr std::uint32_t completed = 2;

  Status(std::uint32_t v) : val(v) {
    switch(v) {
    case initial:
    case inprogress:
    case completed:
      break;
    default:
      throw std::runtime_error("Memory corruption in Status!");
    }
  }
  ~Status() = default;

  operator std::uint32_t&() { return val; }

  std::uint32_t val;
};

OnceFlag::OnceFlag() : status(Status::initial) {};

void OnceFlag::call_detail(const std::function<void(void)>& f) {
  Status s = status.load(std::memory_order_acquire);
  while(true) {
    switch(s) {
    case Status::completed:
      ANNOTATE_HAPPENS_AFTER(&status);
      return;  // Nothing more we can add.
    case Status::inprogress:
      status.wait(s, std::memory_order_relaxed);
      s = status.load(std::memory_order_acquire);
      break;
    case Status::initial:
      if(status.compare_exchange_weak(s, Status::inprogress, std::memory_order_acquire)) {
        try {
          f();
          ANNOTATE_HAPPENS_BEFORE(&status);
          status.exchange(Status::completed, std::memory_order_release);
          status.notify_all();
        } catch(...) {
          status.exchange(Status::initial, std::memory_order_release);
          status.notify_all();
          throw;
        }
        return;  // All done!
      }
      break;
    }
  }
}

bool OnceFlag::query() {
  Status s = status.load(std::memory_order_relaxed);
  while(s == Status::inprogress) {
    status.wait(s, std::memory_order_relaxed);
    s = status.load(std::memory_order_relaxed);
  }
  return s == Status::completed;
}
