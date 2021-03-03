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

#ifndef HPCTOOLKIT_STDSHIM_OPTIONAL_H
#define HPCTOOLKIT_STDSHIM_OPTIONAL_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritence
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <optional>.

#include "version.hpp"

#if STD_HAS(optional)
#include <optional>

namespace hpctoolkit {
namespace stdshim {
template<class T>
using optional = std::optional<T>;

using nullopt_t = std::nullopt_t;
inline constexpr nullopt_t nullopt = std::nullopt;
}
}

#else  // STD_HAS(filesystem)
#include <boost/optional.hpp>
#include <boost/none.hpp>

#include "../util/log.hpp"

namespace hpctoolkit {
namespace stdshim {
template<class T>
class optional : private boost::optional<T> {
public:
  template<class U>
  optional(U& a) : boost::optional<T>(std::forward<U&>(a)) {};
  template<class... Args>
  optional(Args... args) : boost::optional<T>(std::forward<Args>(args)...) {};

  using boost::optional<T>::operator=;
  using boost::optional<T>::operator->;
  using boost::optional<T>::operator*;
  using boost::optional<T>::operator bool;
  using boost::optional<T>::operator!;
  using boost::optional<T>::has_value;
  using boost::optional<T>::emplace;

  template<class U>
  constexpr bool operator==(const optional<U>& o) {
    if(!*this || !o) return (bool)*this == (bool)o;
    return **this == *o;
  }
  template<class U>
  constexpr bool operator!=(const optional<U>& o) { return !(*this == o); }

  constexpr T& value() & {
    if(!(bool)*this) util::log::fatal() << "Attempt to call value() on empty optional!";
    return boost::optional<T>::get();
  }
  constexpr const T& value() const& {
    if(!(bool)*this) util::log::fatal() << "Attempt to call value() on empty optional!";
    return boost::optional<T>::get();
  }
  constexpr T&& value() && {
    if(!(bool)*this) util::log::fatal() << "Attempt to call value() on empty optional!";
    return std::move(boost::optional<T>::get());
  }
  constexpr const T&& value() const&& {
    if(!(bool)*this) util::log::fatal() << "Attempt to call value() on empty optional!";
    return std::move(boost::optional<T>::get());
  }

  template<class U>
  constexpr T value_or(U&& d) const& {
    return (bool)*this ? **this : static_cast<T>(std::forward<U>(d));
  }
  template<class U>
  constexpr T value_or(U&& d) && {
    return (bool)*this ? std::move(**this) : static_cast<T>(std::forward<U>(d));
  }
};

using nullopt_t = boost::none_t;
const nullopt_t nullopt = boost::none;
}
}

#endif  // STD_HAS(filesystem)

#ifndef STDSHIM_DONT_UNDEF
#undef STD_HAS
#endif

#endif  // HPCTOOLKIT_STDSHIM_FILESYSTEM_H
