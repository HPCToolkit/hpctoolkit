// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
