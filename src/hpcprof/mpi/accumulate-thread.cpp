// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "all.hpp"

#include <thread>

using namespace hpctoolkit::mpi;
using namespace detail;

namespace hpctoolkit::mpi::detail {
  struct Win{
    Win(Tag tag) : tag(tag) {};
    const Tag tag;
    std::thread t;
  };
}

SharedAccumulator::SharedAccumulator(Tag tag)
  : detail(World::size() > 1 ? std::make_unique<detail::Win>(tag) : nullptr) {}

SharedAccumulator::~SharedAccumulator() {
  if(detail && detail->t.joinable()) {
    cancel_server<std::uint64_t>(detail->tag);
    detail->t.join();
  }
}

void SharedAccumulator::initialize(std::uint64_t init) {
  atom.store(init, std::memory_order_relaxed);
  if(detail && World::rank() == 0) {
    detail->t = std::thread([](detail::Win& w, std::atomic<std::uint64_t>& atom) {
      while(auto query = receive_server<std::uint64_t>(w.tag)) {
        auto [val, src] = *query;
        val = atom.fetch_add(val, std::memory_order_relaxed);
        send<std::uint64_t>(val, src, w.tag);
      }
    }, std::ref(*detail), std::ref(atom));
  }
}

std::uint64_t SharedAccumulator::fetch_add(std::uint64_t val) {
  if(World::rank() == 0 || !detail)
    return atom.fetch_add(val, std::memory_order_relaxed);

  send<std::uint64_t>(val, 0, detail->tag);
  return receive<std::uint64_t>(0, detail->tag);
}
