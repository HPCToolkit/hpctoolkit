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

#ifndef HPCTOOLKIT_PROFILE_MPI_SCAN_H
#define HPCTOOLKIT_PROFILE_MPI_SCAN_H

#include "core.hpp"

#include <array>
#include <optional>

namespace hpctoolkit::mpi {

namespace detail {
// NOTE: These are in-place operations, for efficiency.
void scan(void* data, std::size_t cnt, const Datatype&, const Op&);
void exscan(void* data, std::size_t cnt, const Datatype&, const Op&);
}  // namespace detail

/// Inclusive scan operation. Returns the accumulation of the values given
/// here and in all other lesser rank indices. Returns the resulting value.
template<class T, std::void_t<decltype(detail::asDatatype<T>())>* = nullptr>
T scan(T data, const Op& op) {
  detail::scan(&data, 1, detail::asDatatype<T>(), op);
  return data;
}

/// Exclusive scan operation. Effectively the inclusive scan without including
/// the current process's contribution. Note that no value is returned in rank
/// 0, thus the need for an optional return value.
template<class T, std::void_t<decltype(detail::asDatatype<T>())>* = nullptr>
std::optional<T> exscan(T data, const Op& op) {
  detail::exscan(&data, 1, detail::asDatatype<T>(), op);
  if(World::rank() == 0) return {};
  return data;
}

/// Inclusive scan operation. Variant to disable the usage of pointers.
template<class T>
T* scan(T*, const Op&) = delete;

/// Exclusive scan operation. Variant to disable the usage of pointers.
template<class T>
T* exscan(T*, const Op&) = delete;

/// Inclusive scan operation. Variant to allow for the usage of std::array.
template<class T, std::size_t N>
std::array<T, N> scan(std::array<T, N> data, const Op& op) {
  detail::scan(data.data(), N, detail::asDatatype<T>(), op);
  return data;
}

/// Exclusive scan operation. Variant to allow for the usage of std::array.
template<class T, std::size_t N>
std::optional<std::array<T, N>> exscan(std::array<T, N> data, const Op& op) {
  detail::exscan(data.data(), N, detail::asDatatype<T>(), op);
  if(World::rank() == 0) return {};
  return data;
}

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_REDUCE_H
