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

/// Singlton class representing the current MPI global communicator.
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

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_CORE_H
