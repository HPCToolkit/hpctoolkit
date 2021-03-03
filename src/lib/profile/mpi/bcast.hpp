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

#ifndef HPCTOOLKIT_PROFILE_MPI_BCAST_H
#define HPCTOOLKIT_PROFILE_MPI_BCAST_H

#include "core.hpp"

#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace hpctoolkit::mpi {

namespace detail {
void bcast(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank);
}  // namespace detail

/// Barrier operation. Ensures all ranks reach this point before any continue.
void barrier();

/// Broadcast operation. Copies the given data from the root rank to all other
/// processes in the team. Returns the given data.
template<class T, std::void_t<decltype(detail::asDatatype<T>())>* = nullptr>
T bcast(T data, std::size_t root) {
  detail::bcast(&data, 1, detail::asDatatype<T>(), root);
  return data;
}

/// Broadcast operation. Variant to disable the usage of pointers.
template<class T>
T* bcast(T*, std::size_t) = delete;

/// Broadcast operation. Variant to allow for the usage of std::array.
template<class T, std::size_t N>
std::array<T, N> bcast(std::array<T, N> data, std::size_t root) {
  detail::bcast(data.data(), N, detail::asDatatype<T>(), root);
  return data;
}

/// Broadcast operation. Variant to allow for the usage of std::vector.
template<class T, class A>
std::vector<T, A> bcast(std::vector<T, A> data, std::size_t root) {
  unsigned long long sz = data.size();
  detail::bcast(&sz, 1, detail::asDatatype<unsigned long long>(), root);
  data.resize(sz);
  bcast(data.data(), sz, detail::asDatatype<T>(), root);
  return data;
}

/// Broadcast operation. Variant to allow for the usage of std::string.
template<class C, class T, class A>
std::basic_string<C,T,A> bcast(std::basic_string<C,T,A> data, std::size_t root) {
  unsigned long long sz = data.size();
  detail::bcast(&sz, 1, detail::asDatatype<unsigned long long>(), root);
  data.resize(sz);
  detail::bcast(&data[0], sz, detail::asDatatype<C>(), root);
  return data;
}

/// Broadcast operation. Variant to allow for the usage of std::vector<std::string>
template<class C, class T, class AS, class AV>
std::vector<std::basic_string<C,T,AS>,AV> bcast(std::vector<std::basic_string<C,T,AS>,AV> data, std::size_t root) {
  std::vector<unsigned long long> sizes;
  if(World::rank() == root) {
    sizes.reserve(data.size());
    for(const auto& s: data) sizes.push_back(s.size());
  }
  sizes = bcast(std::vector<unsigned long long>{sizes}, root);
  std::vector<unsigned long long> ends(sizes.size(), 0);
  std::partial_sum(sizes.begin(), sizes.end(), ends.begin());
  std::vector<C> buffer;
  if(World::rank() == root) {
    buffer.reserve(ends.back());
    for(const auto& s: data) buffer.insert(buffer.end(), s.begin(), s.end());
  } else buffer.resize(ends.back());
  bcast(buffer.data(), buffer.size(), detail::asDatatype<C>(), root);
  if(World::rank() == root) return data;
  data.resize(sizes.size());
  for(std::size_t i = 0; i < sizes.size(); i++)
    data[i] = std::basic_string<C,T,AS>(&buffer[ends[i] - sizes[i]], sizes[i]);
  return data;
}

/// Broadcast operation. Variant to skip the first argument, when you know
/// you're not the root. Relies on default initialization.
template<class T>
T bcast(std::size_t root) { return bcast(T{}, root); }

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_BCAST_H
