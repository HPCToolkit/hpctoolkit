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

#include "all.hpp"

using namespace hpctoolkit::mpi;
using namespace detail;

struct detail::Datatype {};
static Datatype type = {};

template<class T, typename std::enable_if<
    std::is_arithmetic<T>::value && std::is_same<
      typename std::remove_cv<typename std::remove_reference<T>::type>::type,
      T>::value
  >::type* = nullptr>
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
void detail::send(const void*, std::size_t, const Datatype&, std::size_t, std::size_t) {};
void detail::recv(void*, std::size_t, const Datatype&, std::size_t, std::size_t) {};


namespace hpctoolkit::mpi::detail{ 
  struct Win {};
}
SharedAccumulator::SharedAccumulator(int) {};
SharedAccumulator::~SharedAccumulator() = default;

void SharedAccumulator::initialize(std::uint64_t data) {
  atom.store(data, std::memory_order_relaxed);
}

std::uint64_t SharedAccumulator::fetch_add(std::uint64_t val){
  return atom.fetch_add(val, std::memory_order_relaxed);
}
