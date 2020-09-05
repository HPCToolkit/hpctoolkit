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

#ifndef HPCTOOLKIT_PROFILE_MPI_ONE2ONE_H
#define HPCTOOLKIT_PROFILE_MPI_ONE2ONE_H

#include "core.hpp"

#include <array>
#include <vector>

namespace hpctoolkit::mpi {

namespace detail {
// NOTE: These are in-place operations, for efficiency.
void send(const void* data, std::size_t cnt, const Datatype&,
          std::size_t tag, std::size_t dst);
void recv(void* data, std::size_t cnt, const Datatype&,
          std::size_t tag, std::size_t src);
}  // namespace detail

/// Reduction operation. Reduces the given data from all processes in the team
/// to the root rank, using the given reduction operation. Returns the reduced
/// value in the root, returns the given data in all others.

/// Single send operation. Sends a message to a matching receive on another
/// process within the team.
template<class T>
void send(const T& data, std::size_t dst, std::size_t tag) {
  detail::send(&data, 1, detail::asDatatype<T>(), tag, dst);
}

/// Single receive operation. Waits for and receives a message from a matching
/// send on another process within the team. Returns the resulting value.
template<class T>
T receive(std::size_t src, std::size_t tag) {
  T data;
  detail::recv(&data, 1, detail::asDatatype<T>(), tag, src);
  return data;
}

/// Single send operation. Variant to disable the usage of pointers.
template<class T>
void send(T*, std::size_t, std::size_t) = delete;

/// Single send operation. Variant to allow for the usage of std::vector.
template<class T, class A>
void send(const std::vector<T, A>& data, std::size_t dst, std::size_t tag) {
  send(data.size(), dst, tag);
  detail::send(data.data(), data.size(), detail::asDatatype<T>(), tag, dst);
}

/// Single receive operation. Variant to allow for the usage of std::vector.
template<class T>
std::vector<T> receive_vector(std::size_t src, std::size_t tag) {
  auto sz = receive<std::size_t>(src, tag);
  std::vector<T> result(sz);
  detail::recv(result.data(), sz, detail::asDatatype<T>(), tag, src);
  return result;
}

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_ONE2ONE_H
