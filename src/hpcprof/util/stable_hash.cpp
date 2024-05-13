// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "stable_hash.hpp"

#include "../stdshim/bit.hpp"
#include <cassert>
#include <stdexcept>

using namespace hpctoolkit::util;

stable_hash_state::stable_hash_state()
  : state(XXH3_createState()) {
  if(!state) throw std::bad_alloc();
  XXH3_64bits_reset(state.get());
}

stable_hash_state::stable_hash_state(const stable_hash_state& other)
  : stable_hash_state() {
  XXH3_copyState(state.get(), other.state.get());
}

stable_hash_state& stable_hash_state::operator=(const stable_hash_state& other) {
  XXH3_copyState(state.get(), other.state.get());
  return *this;
}

void stable_hash_state::deleter::operator()(XXH3_state_t* state) {
  XXH3_freeState(state);
}

std::uint64_t stable_hash_state::squeeze() noexcept {
  return XXH3_64bits_digest(state.get());
}

stable_hash_state& stable_hash_state::inject(const char* data, std::size_t size) noexcept {
  XXH3_64bits_update(state.get(), data, size);
  return *this;
}

stable_hash_state& stable_hash_state::inject(const std::uint8_t* data, std::size_t size) noexcept {
  XXH3_64bits_update(state.get(), data, size);
  return *this;
}
