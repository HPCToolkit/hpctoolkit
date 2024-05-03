// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_MPI_CORE_H
#define HPCTOOLKIT_PROFILE_MPI_CORE_H

#include <cstdint>
#include <type_traits>

namespace hpctoolkit::mpi {

namespace detail {

struct Datatype;

// Conversion from C++ types to MPI type handles.
template<class T, typename std::enable_if<
    std::is_arithmetic<T>::value && std::is_same<
      typename std::remove_cv<typename std::remove_reference<T>::type>::type,
      T>::value
  >::type* = nullptr>
const Datatype& asDatatype();

}  // namespace detail

/// Operation handle. Represents a single binary operation.
class Op {
public:
  static const Op& max() noexcept;
  static const Op& min() noexcept;
  static const Op& sum() noexcept;
};

/// Singleton class representing the current MPI global communicator.
class World {
public:
  /// Fire up MPI. Needed before calling anything else.
  static void initialize() noexcept;

  /// Close down MPI. Needed after everything else is done.
  static void finalize() noexcept;

  /// Get the rank of the current process within the global communicator.
  static std::size_t rank() noexcept { return m_rank; }

  /// Get the number of processes within the global communicator.
  static std::size_t size() noexcept { return m_size; }

private:
  static std::size_t m_rank;
  static std::size_t m_size;
};

/// MPI tag enumeration, to make sure no one gets mixed up.
enum class Tag : int {
  // 0 is skipped
  ThreadAttributes_1 = 1,  // For attributes.cpp

  SparseDB_1, SparseDB_2,  // For sinks/sparsedb.cpp
  RankTree_1, RankTree_2,  // For hpcprof2-mpi/tree.cpp
};


}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_CORE_H
