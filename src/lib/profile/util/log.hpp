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

#ifndef HPCTOOLKIT_PROFILE_UTIL_LOG_H
#define HPCTOOLKIT_PROFILE_UTIL_LOG_H

#include <sstream>
#include <string>

namespace hpctoolkit::util::log {

namespace detail {
// Private common base for all message buffers.
class MessageBuffer : public std::ostream {
protected:
  // Subclasses should mess with these as needed.
  MessageBuffer(bool enabled = true);
  ~MessageBuffer() = default;

public:
  // Messages are movable but not copyable.
  MessageBuffer(MessageBuffer&&);
  MessageBuffer(const MessageBuffer&) = delete;
  MessageBuffer& operator=(MessageBuffer&&);
  MessageBuffer& operator=(const MessageBuffer&) = delete;

  // Attempts to output are disabled if the entire Message is disabled.
  template<class A>
  std::ostream& operator<<(A a) {
    return enabled ? (*(std::ostream*)this << std::forward<A>(a)) : *this;
  }

protected:
  bool enabled;
  std::stringbuf sbuf;
};
}

/// Fatal error message buffer. When destructed, the program is terminated after
/// the message is written. For cases where things have gone horribly wrong.
struct fatal final : public detail::MessageBuffer {
  fatal();
  [[noreturn]] ~fatal();

  fatal(fatal&&) = default;
  fatal(const fatal&) = delete;
  fatal& operator=(fatal&&) = default;
  fatal& operator=(const fatal&) = delete;
};

/// Non-fatal error message buffer. For cases where things probably aren't going
/// to plan, but its not fatal enough to crash... yet.
struct error final : public detail::MessageBuffer {
  error();
  ~error();

  error(error&&) = default;
  error(const error&) = delete;
  error& operator=(error&&) = default;
  error& operator=(const error&) = delete;
};

/// Warning message buffer. For cases where something went wrong, but it should
/// all work out in the end. Probably.
struct warning final : public detail::MessageBuffer {
  warning();
  ~warning();

  warning(warning&&) = default;
  warning(const warning&) = delete;
  warning& operator=(warning&&) = default;
  warning& operator=(const warning&) = delete;
};

/// Info message buffer. To describe what's going on when its going on, for
/// the curious observer. Not for developers, that's what debug is for.
struct info final : public detail::MessageBuffer {
  info();
  ~info();

  info(info&&) = default;
  info(const info&) = delete;
  info& operator=(info&&) = default;
  info& operator=(const info&) = delete;
};

/// Debug message buffer. To give as much data as possible about what's
/// happening when its happening. For developers.
/// For now, controlled by a compile-time switch. Eventually that will change.
struct debug final : public detail::MessageBuffer {
  debug(bool);
  ~debug();

  // Debug messages can be disabled prior to the end of scope
  void disable() { enabled = false; }

  debug(debug&&) = default;
  debug(const debug&) = delete;
  debug& operator=(debug&&) = default;
  debug& operator=(const debug&) = delete;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_LOG_H
