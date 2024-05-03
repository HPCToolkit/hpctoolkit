// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_MPI_ONE2ONE_H
#define HPCTOOLKIT_PROFILE_MPI_ONE2ONE_H

#include "core.hpp"

#include <array>
#include <optional>
#include <vector>

namespace hpctoolkit::mpi {

namespace detail {
// NOTE: These are in-place operations, for efficiency.
void send(const void* data, std::size_t cnt, const Datatype&,
          Tag tag, std::size_t dst);
void recv(void* data, std::size_t cnt, const Datatype&,
          Tag tag, std::size_t src);
std::optional<std::size_t> recv_server(void* data, std::size_t cnt,
    const Datatype&, Tag tag);
void cancel_server(const Datatype&, Tag tag);
}  // namespace detail

/// Reduction operation. Reduces the given data from all processes in the team
/// to the root rank, using the given reduction operation. Returns the reduced
/// value in the root, returns the given data in all others.

/// Single send operation. Sends a message to a matching receive on another
/// process within the team.
template<class T>
void send(const T& data, std::size_t dst, Tag tag) {
  detail::send(&data, 1, detail::asDatatype<T>(), tag, dst);
}

/// Single receive operation. Waits for and receives a message from a matching
/// send on another process within the team. Returns the resulting value.
template<class T>
T receive(std::size_t src, Tag tag) {
  T data;
  detail::recv(&data, 1, detail::asDatatype<T>(), tag, src);
  return data;
}

/// Single send operation. Variant to disable the usage of pointers.
template<class T>
void send(T*, std::size_t, Tag) = delete;

/// Single send operation. Variant to allow for the usage of std::vector.
template<class T, class A>
void send(const std::vector<T, A>& data, std::size_t dst, Tag tag) {
  send(data.size(), dst, tag);
  detail::send(data.data(), data.size(), detail::asDatatype<T>(), tag, dst);
}

/// Single receive operation. Variant to allow for the usage of std::vector.
template<class T>
std::vector<T> receive_vector(std::size_t src, Tag tag) {
  auto sz = receive<std::size_t>(src, tag);
  std::vector<T> result(sz);
  detail::recv(result.data(), sz, detail::asDatatype<T>(), tag, src);
  return result;
}

/// Variant of receive designed for "server" threads.
///
/// This will still block the caller until a message is received, however it
/// can be canceled via a call to cancel() with the matching tag. If canceled
/// the call will return an empty optional, making it possible to loop like:
///     while(auto mesg = recieve_server(...)) {
///       auto [payload, src] = *mesg;
///
/// Returns both the payload and the sender, or an empty optional if canceled.
template<class T>
std::optional<std::pair<T, std::size_t>> receive_server(Tag tag) {
  T data;
  if(auto src = detail::recv_server(&data, 1, detail::asDatatype<T>(), tag))
    return std::make_pair(data, *src);
  return std::nullopt;
}

/// Cancel a concurrent receive_server call, with the matching tag and datatype.
template<class T>
void cancel_server(Tag tag) {
  return detail::cancel_server(detail::asDatatype<T>(), tag);
}

}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_ONE2ONE_H
