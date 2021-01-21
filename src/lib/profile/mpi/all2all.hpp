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

#ifndef HPCTOOLKIT_PROFILE_MPI_ALL2ALL_H
#define HPCTOOLKIT_PROFILE_MPI_ALL2ALL_H

#include "core.hpp"

#include "../util/log.hpp"

#include <array>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace hpctoolkit::mpi {

namespace detail {
void gather_root(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank);
void gather(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank);
void gatherv_root(void* data, const std::size_t* cnts, const Datatype&, std::size_t rootRank);
void gatherv(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank);
void scatter_root(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank);
void scatter(void* send, std::size_t cnt, const Datatype&, std::size_t rootRank);
void scatterv_root(void* data, const std::size_t* cnts, const Datatype&, std::size_t rootRank);
void scatterv(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank);
}  // namespace detail

/// Gather operation. Copies the data given in all other processes in the team
/// into a single vector on the root rank. Only returns the result in the root.
template<class T, std::void_t<decltype(detail::asDatatype<T>())>* = nullptr>
std::optional<std::vector<T>> gather(T data, std::size_t root) {
  if(World::rank() == root) {
    std::vector<T> result(World::size());
    result[root] = std::move(data);
    detail::gather_root(result.data(), 1, detail::asDatatype<T>(), root);
    return std::move(result);
  }

  detail::gather(&data, 1, detail::asDatatype<T>(), root);
  return {};
}

/// Scatter operation. Copies the data for each rank from the root process
/// to all other ranks. Returns the allocated data in each rank.
template<class T, class A>
T scatter(std::vector<T, A> data, std::size_t root) {
  if(World::rank() == root) {
    assert(data.size() == World::size() && "Invalid data argument to mpi::scatter!");
    detail::scatter_root(data.data(), 1, detail::asDatatype<T>(), root);
    return std::move(data[root]);
  }

  T result;
  detail::scatter(&result, 1, detail::asDatatype<T>(), root);
  return result;
}

/// Gather operation. Variant to disable the usage of pointers.
template<class T>
std::optional<std::vector<T*>> gather(T*, std::size_t) = delete;

/// Scatter operation. Variant to disable the usage of pointers.
template<class T, class A>
T* scatter(std::vector<T*, A>, std::size_t) = delete;

/// Gather operation. Variant to allow for the usage of std::array.
template<class T, std::size_t N>
std::optional<std::vector<std::array<T, N>>> gather(std::array<T, N> data, std::size_t root) {
  if(World::rank() == root) {
    std::vector<T> buffer(N * World::size());
    for(std::size_t i = 0; i < N; i++) buffer[N*root + i] = std::move(data[i]);
    detail::gather_root(buffer.data(), N, detail::asDatatype<T>(), root);
    std::vector<std::array<T, N>> result(World::size());
    for(std::size_t r = 0, idx = 0; r < World::size(); r++)
      for(std::size_t i = 0; i < N; i++, idx++)
        result[r][i] = std::move(buffer[idx]);
    return std::move(result);
  }

  detail::gather(data.data(), N, detail::asDatatype<T>(), root);
  return {};
}

/// Scatter operation. Variant to allow for the usage of std::array.
template<class T, class A, std::size_t N>
std::array<T, N> scatter(std::vector<std::array<T, N>, A> data, std::size_t root) {
  if(World::rank() == root) {
    assert(data.size() == World::size() && "Invalid data argument to mpi::scatter!");
    std::vector<T> buffer(N * World::size());
    for(std::size_t r = 0, idx = 0; r < World::size(); r++)
      for(std::size_t i = 0; i < N; i++, idx++)
        buffer[idx] = std::move(data[r][i]);
    detail::scatter_root(buffer.data(), N, detail::asDatatype<T>(), root);
    std::array<T, N> result;
    for(std::size_t i = 0; i < N; i++)
      result[i] = std::move(buffer[N*root + i]);
    return std::move(result);
  }

  std::array<T, N> result;
  detail::scatter(result.data(), N, detail::asDatatype<T>(), root);
  return std::move(result);
}

/// Gather operation. Variant to allow for the usage of std::vector.
/// This allows for ranks to contribute different numbers of elements.
template<class T>
std::optional<std::vector<std::vector<T>>> gather(std::vector<T> data, std::size_t root) {
  if(World::rank() == root) {
    auto cnts = gather((std::size_t)0, root);
    std::size_t total = 0;
    for(std::size_t r = 0; r < World::size(); r++) total += (*cnts)[r];
    std::vector<T> buffer(total);
    detail::gatherv_root(buffer.data(), cnts.value().data(), detail::asDatatype<T>(), root);
    std::vector<std::vector<T>> result(World::size());
    total = 0;
    for(std::size_t r = 0; r < World::size(); r++) {
      result[r].reserve(cnts.value()[r]);
      for(std::size_t i = 0; i < cnts.value()[r]; i++, total++)
        result[r].emplace_back(std::move(buffer[total]));
    }
    result[root] = std::move(data);
    return std::move(result);
  }

  gather(data.size(), root);
  detail::gatherv(data.data(), data.size(), detail::asDatatype<T>(), root);
  return {};
}

/// Scatter operation. Variant to allow for the usage of std::vector.
/// This allows for ranks to contribute different numbers of elements.
template<class T>
std::vector<T> scatter(std::vector<std::vector<T>> data, std::size_t root) {
  if(World::rank() == root) {
    assert(data.size() == World::size() && "Invalid data argument to mpi::scatter!");
    std::vector<std::size_t> cnts(World::size());
    cnts[root] = 0;
    std::vector<T> buffer;
    std::size_t totalsz = 0;
    for(std::size_t r = 0; r < World::size(); r++)
      totalsz += r == root ? 0 : (cnts[r] = data[r].size());
    buffer.reserve(totalsz);
    for(std::size_t r = 0; r < World::size(); r++)
      if(r != root)
        for(auto& x: data[r]) buffer.emplace_back(std::move(x));
    scatter(cnts, root);
    detail::scatterv_root(buffer.data(), cnts.data(), detail::asDatatype<T>(), root);
    return std::move(data[root]);
  }

  auto cnt = scatter(std::vector<std::size_t>{}, root);
  std::vector<T> result(cnt);
  detail::scatterv(result.data(), cnt, detail::asDatatype<T>(), root);
  return result;
}

/// Gather operation. Variant to allow for the usage of std::string.
template<class C, class T>
std::optional<std::vector<std::basic_string<C,T>>> gather(std::basic_string<C,T> data, std::size_t root) {
  auto output = gather(std::vector<C>{data.begin(), data.end()}, root);
  if(!output) return {};
  std::vector<std::basic_string<C,T>> result(World::size());
  for(std::size_t r = 0; r < World::size(); r++)
    result[r] = std::basic_string<C,T>{(*output)[r].data(), (*output)[r].size()};
  return result;
}

/// Scatter operation. Variant to allow for the usage of std::string.
template<class C, class T>
std::basic_string<C,T> scatter(std::vector<std::basic_string<C,T>> data, std::size_t root) {
  assert(data.size() == World::size() && "Invalid data argument to mpi::scatter!");
  std::vector<std::vector<C>> vdata(World::size());
  for(std::size_t r = 0; r < World::size(); r++)
    vdata[r] = std::vector<C>{data[r].begin(), data[r].end()};
  auto result = scatter(vdata, root);
  return std::basic_string<C,T>{result.data(), result.size()};
}

/// Gather operation. Variant to allow for the usage for std::vector<std::string>
template<class C, class T>
std::optional<std::vector<std::vector<std::basic_string<C,T>>>> gather(std::vector<std::basic_string<C,T>> data, std::size_t root) {
  if(World::rank() == root) {
    auto lengths = gather(std::vector<std::size_t>{}, root).value();
    auto strips = gather(std::vector<C>{}, root).value();
    std::vector<std::vector<std::basic_string<C,T>>> result(World::size());
    result[root] = std::move(data);
    for(std::size_t r = 0; r < World::size(); r++) {
      result[r].reserve(lengths[r].size());
      for(std::size_t i = 0, idx = 0; i < lengths[r].size(); idx += lengths[r][i], i++)
        result[r].emplace_back(&strips[r][idx], lengths[r][i]);
    }
    return std::move(result);
  }

  std::vector<std::size_t> lengths(data.size());
  std::size_t totalsize = 0;
  for(std::size_t i = 0; i < data.size(); i++)
    totalsize += (lengths[i] = data[i].size());
  gather(std::move(lengths), root);

  std::vector<C> strip;
  strip.reserve(totalsize);
  for(std::size_t i = 0; i < data.size(); i++)
    strip.insert(strip.end(), data[i].begin(), data[i].end());
  gather(std::move(strip), root);
  return {};
}

/// Scatter operation. Variant to allow for the usage of std::vector<std::string>
template<class C, class T>
std::vector<std::basic_string<C,T>> scatter(std::vector<std::vector<std::basic_string<C,T>>> data, std::size_t root) {
  if(World::rank() == root) {
    assert(data.size() == World::size() && "Invalid data argument to mpi::scatter!");
    std::vector<std::vector<std::size_t>> lengths(World::size());
    std::vector<std::vector<C>> strips(World::size());
    for(std::size_t r = 0; r < World::size(); r++) {
      if(r != root) {
        lengths[r].reserve(data[r].size());
        std::size_t totalsz = 0;
        for(std::size_t i = 0; i < data[r].size(); i++) {
          lengths[r].push_back(data[r][i].size());
          totalsz += data[r][i].size();
        }
        strips[r].reserve(totalsz);
        for(std::size_t i = 0; i < data[r].size(); i++)
          strips[r].insert(strips[r].end(), data[r][i].begin(), data[r][i].end());
      }
    }
    scatter(std::move(lengths), root);
    scatter(std::move(strips), root);
    return std::move(data[root]);
  }

  auto lengths = scatter(std::vector<std::vector<std::size_t>>{}, root);
  auto strip = scatter(std::vector<std::vector<C>>{}, root);
  std::vector<std::basic_string<C,T>> result;
  result.reserve(lengths.size());
  for(std::size_t i = 0, idx = 0; i < lengths.size(); idx += lengths[i], i++)
    result.emplace_back(&strip[idx], lengths[i]);
  return result;
}

/// Scatter operation. Variant to allow skipping the first argument if you
/// know you're not the root.
template<class T>
T scatter(std::size_t root) { return scatter(std::vector<T>{}, root); }

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_BCAST_H
