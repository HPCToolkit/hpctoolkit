// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "log.hpp"

#include "../mpi/core.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace hpctoolkit::util::log;
using namespace detail;

MessageBuffer::MessageBuffer(bool en)
  : std::ostream(), enabled(en), sbuf(std::ios_base::out) {
  if(enabled) this->init(&sbuf);
}
MessageBuffer::MessageBuffer(MessageBuffer&& o)
  : std::ostream(std::move(o)), enabled(o.enabled), sbuf(std::move(o.sbuf)) {
  if(enabled) this->init(&sbuf);
}
MessageBuffer& MessageBuffer::operator=(MessageBuffer&& o) {
  std::ostream::operator=(std::move(o));
  enabled = o.enabled;
  sbuf = std::move(o.sbuf);
  if(enabled) this->init(&sbuf);
  return *this;
}

bool MessageBuffer::empty() noexcept {
  return sbuf.pubseekoff(0, std::ios_base::cur, std::ios_base::out) <= 0;
}

static bool settings_set = false;
static Settings settings = Settings::none;
Settings Settings::get() {
  assert(settings_set && "Logging is not available before calling util::log::Settings::set!");
  return settings;
}
void Settings::set(Settings s) {
  settings = std::move(s);
  settings_set = true;
}

fatal::fatal() : detail::MessageBuffer(true) { (*this) << "FATAL: "; }
fatal::~fatal() {
  (*this) << '\n';
  std::cerr << sbuf.str();
  std::abort();
}

error::error() : detail::MessageBuffer(Settings::get().error()) { (*this) << "ERROR: "; }
error::~error() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

verror::verror() : detail::MessageBuffer(Settings::get().verbose()) { (*this) << "VERBOSE ERROR: "; }
verror::~verror() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

warning::warning() : detail::MessageBuffer(Settings::get().warning()) { (*this) << "WARNING: "; }
warning::~warning() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

vwarning::vwarning() : detail::MessageBuffer(Settings::get().verbose()) { (*this) << "VERBOSE WARNING: "; }
vwarning::~vwarning() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

argsinfo::argsinfo() : detail::MessageBuffer(true) { (*this) << "INFO: "; }
argsinfo::~argsinfo() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

info::info() : detail::MessageBuffer(Settings::get().info()) { (*this) << "INFO: "; }
info::~info() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

debug::debug() : detail::MessageBuffer(Settings::get().debug()) {
  (*this) << "DEBUG";
  if(mpi::World::size() > 0)
    (*this) << " [" << mpi::World::rank() << "]";
  (*this) << ": ";
}
debug::~debug() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}
