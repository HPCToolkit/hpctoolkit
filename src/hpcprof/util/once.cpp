// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "vgannotations.hpp"

#include "../util/log.hpp"
#include "once.hpp"

#include <cassert>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace hpctoolkit::util;

Once::Once() : callerId(), promise(), future(promise.get_future()) {};

Once::Caller::~Caller() {
  ANNOTATE_HAPPENS_BEFORE(&once);
  once.callerId.exchange(std::thread::id(), std::memory_order_relaxed);
  once.promise.set_value();
}

Once::Caller Once::signal() {
  std::thread::id old;
  [[maybe_unused]] auto ok = callerId.compare_exchange_strong(old, std::this_thread::get_id(),
      std::memory_order_acquire, std::memory_order_relaxed);
  assert(ok && "Once cannot be signal()'d more than once!");
  return Caller(*this);
}

void Once::wait() const {
  assert(callerId.load(std::memory_order_relaxed) != std::this_thread::get_id()
         && "Single-thread deadlock detected on Once!");
  future.wait();
  ANNOTATE_HAPPENS_AFTER(this);
}

void hpctoolkit::util::call_once_detail(std::once_flag& flag,
                                            const std::function<void(void)>& f) {
  std::call_once(flag, [&]{
    f();
    ANNOTATE_HAPPENS_BEFORE(&flag);
  });
  ANNOTATE_HAPPENS_AFTER(&flag);
}

OnceFlag::OnceFlag() : status(Status::initial) {};

void OnceFlag::call_detail(bool block, const std::function<void(void)>& f) {
  Status s = status.load(std::memory_order_acquire);
  while(true) {
    switch(s) {
    case Status::completed:
      ANNOTATE_HAPPENS_AFTER(&status);
      return;  // Nothing more we can add.
    case Status::inprogress:
      if(!block) return;  // Its getting there.
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
