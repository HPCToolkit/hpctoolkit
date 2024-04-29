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
// Copyright ((c)) 2002-2024, Rice University
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
