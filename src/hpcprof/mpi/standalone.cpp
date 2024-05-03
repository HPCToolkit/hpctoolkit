// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "all.hpp"

using namespace hpctoolkit::mpi;
using namespace detail;

struct detail::Datatype {};
static Datatype type = {};

template<class T, typename std::enable_if<
    std::is_arithmetic<T>::value && std::is_same<
      typename std::remove_cv<typename std::remove_reference<T>::type>::type,
      T>::value
  >::type*>
const Datatype& detail::asDatatype() { return type; }
template const Datatype& detail::asDatatype<char>();
template const Datatype& detail::asDatatype<int8_t>();
template const Datatype& detail::asDatatype<int16_t>();
template const Datatype& detail::asDatatype<int32_t>();
template const Datatype& detail::asDatatype<long long>();
template const Datatype& detail::asDatatype<int64_t>();
template const Datatype& detail::asDatatype<uint8_t>();
template const Datatype& detail::asDatatype<uint16_t>();
template const Datatype& detail::asDatatype<uint32_t>();
template const Datatype& detail::asDatatype<unsigned long long>();
template const Datatype& detail::asDatatype<uint64_t>();
template const Datatype& detail::asDatatype<float>();
template const Datatype& detail::asDatatype<double>();

struct BaseOp : hpctoolkit::mpi::Op {};
static BaseOp op = {};
const Op& Op::max() noexcept { return op; }
const Op& Op::min() noexcept { return op; }
const Op& Op::sum() noexcept { return op; }

std::size_t World::m_rank = 0;
std::size_t World::m_size = 1;

void World::initialize() noexcept {};
void World::finalize() noexcept {};

void hpctoolkit::mpi::barrier() {};
void detail::bcast(void*, std::size_t, const Datatype&, std::size_t) {};
void detail::reduce(void*, std::size_t, const Datatype&, std::size_t, const Op&) {};
void detail::allreduce(void*, std::size_t, const Datatype&, const Op&) {};
void detail::scan(void*, std::size_t, const Datatype&, const Op&) {};
void detail::exscan(void*, std::size_t, const Datatype&, const Op&) {};
void detail::gather_root(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank) {};
void detail::gather(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank) {};
void detail::gatherv_root(void* data, const std::size_t* cnts, const Datatype&, std::size_t rootRank) {};
void detail::gatherv(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank) {};
void detail::scatter_root(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank) {};
void detail::scatter(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank) {};
void detail::scatterv_root(void* data, const std::size_t* cnts, const Datatype&, std::size_t rootRank) {};
void detail::scatterv(void* data, std::size_t cnt, const Datatype&, std::size_t rootRank) {};
void detail::send(const void*, std::size_t, const Datatype&, Tag, std::size_t) {};
void detail::recv(void*, std::size_t, const Datatype&, Tag, std::size_t) {};
std::optional<std::size_t> detail::recv_server(void*, std::size_t, const Datatype&, Tag) {
  return std::nullopt;
}
void detail::cancel_server(const Datatype&, Tag) {};
