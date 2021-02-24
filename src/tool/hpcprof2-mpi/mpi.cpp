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

#include "lib/profile/mpi/all.hpp"

#include "lib/profile/util/log.hpp"

#include <mpi.h>
#include <mutex>

using namespace hpctoolkit::mpi;
using namespace detail;

struct detail::Datatype {
  Datatype(MPI_Datatype d) : value(d) {};
  MPI_Datatype value;
};

static Datatype d_char = MPI_CHAR;
template<> const Datatype& detail::asDatatype<char>() { return d_char; }
static Datatype d_int8 = MPI_INT8_T;
template<> const Datatype& detail::asDatatype<int8_t>() { return d_int8; }
static Datatype d_int16 = MPI_INT16_T;
template<> const Datatype& detail::asDatatype<int16_t>() { return d_int16; }
static Datatype d_int32 = MPI_INT32_T;
template<> const Datatype& detail::asDatatype<int32_t>() { return d_int32; }
static Datatype d_ll = MPI_LONG_LONG;
template<> const Datatype& detail::asDatatype<long long>() { return d_ll; }
static Datatype d_int64 = MPI_INT64_T;
template<> const Datatype& detail::asDatatype<int64_t>() { return d_int64; }
static Datatype d_uint8 = MPI_UINT8_T;
template<> const Datatype& detail::asDatatype<uint8_t>() { return d_uint8; }
static Datatype d_uint16 = MPI_UINT16_T;
template<> const Datatype& detail::asDatatype<uint16_t>() { return d_uint16; }
static Datatype d_uint32 = MPI_UINT32_T;
template<> const Datatype& detail::asDatatype<uint32_t>() { return d_uint32; }
static Datatype d_ull = MPI_UNSIGNED_LONG_LONG;
template<> const Datatype& detail::asDatatype<unsigned long long>() { return d_ull; }
static Datatype d_uint64 = MPI_UINT64_T;
template<> const Datatype& detail::asDatatype<uint64_t>() { return d_uint64; }
static Datatype d_float = MPI_FLOAT;
template<> const Datatype& detail::asDatatype<float>() { return d_float; }
static Datatype d_double = MPI_DOUBLE;
template<> const Datatype& detail::asDatatype<double>() { return d_double; }

struct BaseOp : hpctoolkit::mpi::Op {
  BaseOp(MPI_Op o) : op(o) {};
  MPI_Op op;
};

static BaseOp o_max = MPI_MAX;
const Op& Op::max() noexcept { return o_max; }
static BaseOp o_min = MPI_MIN;
const Op& Op::min() noexcept { return o_min; }
static BaseOp o_sum = MPI_SUM;
const Op& Op::sum() noexcept { return o_sum; }

std::size_t World::m_rank = 0;
std::size_t World::m_size = 0;

static bool done = false;
static void escape() {
  // We use std::exit when things go south, but MPI doesn't react very quickly
  // to that. So we issue an Abort ourselves.
  if(!done) MPI_Abort(MPI_COMM_WORLD, 0);
}

// We support serialized MPI if nessesary
static std::mutex lock;
static bool needsLock = true;
static std::unique_lock<std::mutex> mpiLock() {
  return needsLock ? std::unique_lock<std::mutex>(lock)
                   : std::unique_lock<std::mutex>();
}

void World::initialize() noexcept {
  int available;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &available);
  std::atexit(escape);
  if(available < MPI_THREAD_SERIALIZED)
    util::log::fatal{} << "MPI does not have sufficient thread support!";
  needsLock = available < MPI_THREAD_MULTIPLE;

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  m_rank = rank;

  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  m_size = size;
}
void World::finalize() noexcept {
  MPI_Finalize();
  done = true;
}

void hpctoolkit::mpi::barrier() {
  auto l = mpiLock();
  if(MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI barrier!";
}
void detail::bcast(void* data, std::size_t cnt, const Datatype& ty,
                   std::size_t root) {
  auto l = mpiLock();
  if(MPI_Bcast(data, cnt, ty.value, root, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI broadcast!";
}
void detail::reduce(void* data, std::size_t cnt, const Datatype& ty,
                    std::size_t root, const Op& op) {
  auto l = mpiLock();
  if(MPI_Reduce(World::rank() == root ? MPI_IN_PLACE : data, data, cnt, ty.value,
                static_cast<const BaseOp&>(op).op, root, MPI_COMM_WORLD)
     != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI reduction!";
}
void detail::allreduce(void* data, std::size_t cnt, const Datatype& ty,
                       const Op& op) {
  auto l = mpiLock();
  if(MPI_Allreduce(MPI_IN_PLACE, data, cnt, ty.value,
                   static_cast<const BaseOp&>(op).op, MPI_COMM_WORLD)
     != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI all-reduction!";
}
void detail::scan(void* data, std::size_t cnt, const Datatype& ty,
                  const Op& op) {
  auto l = mpiLock();
  if(MPI_Scan(MPI_IN_PLACE, data, cnt, ty.value,
              static_cast<const BaseOp&>(op).op, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI inclusive scan!";
}
void detail::exscan(void* data, std::size_t cnt, const Datatype& ty,
                    const Op& op) {
  auto l = mpiLock();
  if(MPI_Exscan(MPI_IN_PLACE, data, cnt, ty.value,
     static_cast<const BaseOp&>(op).op, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI exclusive scan!";
}
void detail::gather(void* data, std::size_t cnt, const Datatype& ty,
                    std::size_t rootRank) {
  auto l = mpiLock();
  if(World::rank() == rootRank) {
    if(MPI_Gather(MPI_IN_PLACE, 0, 0, data, cnt, ty.value, rootRank,
                  MPI_COMM_WORLD) != MPI_SUCCESS)
      util::log::fatal{} << "Error while performing an MPI gather!";
  } else {
    if(MPI_Gather(data, cnt, ty.value, nullptr, 0, 0, rootRank,
                  MPI_COMM_WORLD) != MPI_SUCCESS)
      util::log::fatal{} << "Error while performing an MPI gather!";
  }
}
void detail::gatherv(void* data, const std::size_t* cnts, const Datatype& ty,
                     std::size_t rootRank) {
  if(World::rank() != rootRank) util::log::fatal{} << "Only valid at root!";
  std::vector<int> icnts(World::size());
  std::vector<int> ioffsets(World::size());
  for(std::size_t i = 0, idx = 0; i < World::size(); i++) {
    icnts[i] = cnts[i];
    ioffsets[i] = idx;
    idx += cnts[i];
  }
  auto l = mpiLock();
  if(MPI_Gatherv(MPI_IN_PLACE, 0, 0, data, icnts.data(), ioffsets.data(),
                 ty.value, rootRank, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI vectorized gather (root)!";
}
void detail::gatherv(void* data, std::size_t cnt, const Datatype& ty,
                     std::size_t rootRank) {
  if(World::rank() == rootRank) util::log::fatal{} << "Not valid at root!";
  auto l = mpiLock();
  if(MPI_Gatherv(data, cnt, ty.value, nullptr, nullptr, nullptr, 0,
                 rootRank, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI vectorized gather (non-root)!";
}
void detail::scatter(void* data, std::size_t cnt, const Datatype& ty,
                    std::size_t rootRank) {
  auto l = mpiLock();
  if(World::rank() == rootRank) {
    if(MPI_Scatter(data, cnt, ty.value, MPI_IN_PLACE, 0, 0, rootRank,
                   MPI_COMM_WORLD) != MPI_SUCCESS)
      util::log::fatal{} << "Error while performing an MPI scatter!";
  } else {
    if(MPI_Scatter(nullptr, 0, 0, data, cnt, ty.value, rootRank,
                   MPI_COMM_WORLD) != MPI_SUCCESS)
      util::log::fatal{} << "Error while performing an MPI scatter!";
  }
}
void detail::scatterv(void* data, const std::size_t* cnts, const Datatype& ty,
                      std::size_t rootRank) {
  if(World::rank() != rootRank) util::log::fatal{} << "Only valid at root!";
  std::vector<int> icnts(World::size());
  std::vector<int> ioffsets(World::size());
  for(std::size_t i = 0, idx = 0; i < World::size(); i++) {
    icnts[i] = cnts[i];
    ioffsets[i] = idx;
    idx += cnts[i];
  }
  auto l = mpiLock();
  if(MPI_Scatterv(data, icnts.data(), ioffsets.data(), ty.value,
                  MPI_IN_PLACE, 0, 0, rootRank, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI vectorized scatter (root)!";
}
void detail::scatterv(void* data, std::size_t cnt, const Datatype& ty,
                      std::size_t rootRank) {
  if(World::rank() == rootRank) util::log::fatal{} << "Only valid at root!";
  auto l = mpiLock();
  if(MPI_Scatterv(nullptr, nullptr, nullptr, 0,
                  data, cnt, ty.value, rootRank, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI vectorized scatter (non-root)!";
}
void detail::send(const void* data, std::size_t cnt, const Datatype& ty,
                  std::size_t tag, std::size_t dst) {
  auto l = mpiLock();
  if(MPI_Send(data, cnt, ty.value, dst, tag, MPI_COMM_WORLD) != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI send!";
}
void detail::recv(void* data, std::size_t cnt, const Datatype& ty,
                  std::size_t tag, std::size_t src) {
  auto l = mpiLock();
  if(MPI_Recv(data, cnt, ty.value, src, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE)
     != MPI_SUCCESS)
    util::log::fatal{} << "Error while performing an MPI recieve!";
}
