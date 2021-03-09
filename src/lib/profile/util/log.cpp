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
// Copyright ((c)) 2020, Rice University
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

#include "log.hpp"

#include "mpi/core.hpp"

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

fatal::fatal() { (*this) << "FATAL: "; }
fatal::~fatal() {
  (*this) << '\n';
  std::cerr << sbuf.str();
  std::abort();
}

error::error() { (*this) << "ERROR: "; }
error::~error() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

warning::warning() { (*this) << "WARNING: "; }
warning::~warning() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

info::info() { (*this) << "INFO: "; }
info::~info() {
  if(!empty()) {
    (*this) << '\n';
    std::cerr << sbuf.str();
  }
}

debug::debug(bool enable) : MessageBuffer(enable) {
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
