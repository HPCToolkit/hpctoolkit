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

#include "core.hpp"
#include "bcast.hpp"
#include "reduce.hpp"

using namespace hpctoolkit::mpi;
using namespace detail;

struct detail::Datatype {};
static detail::Datatype type = {};

template<> const Datatype& detail::asDatatype<char>() { return type; }
template<> const Datatype& detail::asDatatype<int8_t>() { return type; }
template<> const Datatype& detail::asDatatype<int16_t>() { return type; }
template<> const Datatype& detail::asDatatype<int32_t>() { return type; }
template<> const Datatype& detail::asDatatype<int64_t>() { return type; }
template<> const Datatype& detail::asDatatype<uint8_t>() { return type; }
template<> const Datatype& detail::asDatatype<uint16_t>() { return type; }
template<> const Datatype& detail::asDatatype<uint32_t>() { return type; }
template<> const Datatype& detail::asDatatype<uint64_t>() { return type; }
template<> const Datatype& detail::asDatatype<float>() { return type; }
template<> const Datatype& detail::asDatatype<double>() { return type; }

std::size_t World::m_rank = 0;
std::size_t World::m_size = 1;

void World::initialize() noexcept {};
void World::finalize() noexcept {};

void detail::bcast(void*, std::size_t, const Datatype&, std::size_t) {};
void detail::reduce(void*, std::size_t, const Datatype&, std::size_t, const ReductionOp&) {};
void detail::allreduce(void*, std::size_t, const Datatype&, std::size_t, const ReductionOp&) {};

