// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
