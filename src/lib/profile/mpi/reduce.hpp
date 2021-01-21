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

#ifndef HPCTOOLKIT_PROFILE_MPI_REDUCE_H
#define HPCTOOLKIT_PROFILE_MPI_REDUCE_H

#include "core.hpp"

#include <array>
#include <vector>

namespace hpctoolkit::mpi {

namespace detail {
// NOTE: These are in-place operations, for efficiency.
void reduce(void* data, std::size_t cnt, const Datatype&,
            std::size_t rootRank, const Op&);
void allreduce(void* data, std::size_t cnt, const Datatype&, const Op&);
}  // namespace detail

/// Reduction operation. Reduces the given data from all processes in the team
/// to the root rank, using the given reduction operation. Returns the reduced
/// value in the root, returns the given data in all others.
template<class T, std::void_t<decltype(detail::asDatatype<T>())>* = nullptr>
T reduce(T data, std::size_t root, const Op& op) {
  detail::reduce(&data, 1, detail::asDatatype<T>(), root, op);
  return data;
}

/// Broadcast reduction operation. Equivalent to a reduction operation followed
/// by a broadcast. Returns the reduced value in all processes.
template<class T, std::void_t<decltype(detail::asDatatype<T>())>* = nullptr>
T allreduce(T data, const Op& op) {
  detail::allreduce(&data, 1, detail::asDatatype<T>(), op);
  return data;
}

/// Reduction operation. Variant to disable the usage of pointers.
template<class T>
T* reduce(T*, std::size_t, const Op&) = delete;

/// Broadcast reduction operation. Variant to disable the usage of pointers.
template<class T>
T* allreduce(T*, const Op&) = delete;

/// Reduction operation. Variant to allow for the usage of std::array.
template<class T, std::size_t N>
std::array<T, N> reduce(std::array<T, N> data, std::size_t root, const Op& op) {
  detail::reduce(data.data(), N, detail::asDatatype<T>(), root, op);
  return data;
}

/// Broadcast reduction operation. Variant to allow for the usage of std::array.
template<class T, std::size_t N>
std::array<T, N> allreduce(std::array<T, N> data, const Op& op) {
  detail::allreduce(data.data(), N, detail::asDatatype<T>(), op);
  return data;
}

/// Reduction operation. Variant to allow for the usage of std::vector.
/// Note that the size of `data` on every rank must be the same.
template<class T, class A>
std::vector<T, A> reduce(std::vector<T, A> data, std::size_t root, const Op& op) {
  detail::reduce(data.data(), data.size(), detail::asDatatype<T>(), root, op);
  return data;
}

/// Broadcast reduction operation. Variant to allow for the usage of std::vector.
/// Note that the size of `data` on every rank must be the same.
template<class T, class A>
std::vector<T, A> allreduce(std::vector<T, A> data, const Op& op) {
  detail::allreduce(data.data(), data.size(), detail::asDatatype<T>(), op);
  return data;
}

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_REDUCE_H
