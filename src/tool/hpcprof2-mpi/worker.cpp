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
// Copyright ((c)) 2019-2020, Rice University
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

#include "lib/profile/util/vgannotations.hpp"

#include "sparse.hpp"
#include "mpi-strings.h"
#include "../hpcprof2/args.hpp"

#include "lib/profile/pipeline.hpp"
#include "lib/profile/source.hpp"
#include "lib/profile/packedids.hpp"
#include "lib/profile/sinks/lambda.hpp"
#include "lib/profile/sinks/packed.hpp"
#include "lib/profile/sinks/hpctracedb.hpp"
#include "lib/profile/sinks/hpcmetricdb.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/finalizers/lambda.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/util/log.hpp"

#include <mpi.h>
#include <iostream>

using namespace hpctoolkit;
using namespace hpctoolkit::literals;
namespace fs = stdshim::filesystem;

static constexpr unsigned int bits = std::numeric_limits<std::size_t>::digits;
static constexpr unsigned int mask = bits - 1;
static_assert(0 == (bits & (bits - 1)), "value to rotate must be a power of 2");
static constexpr std::size_t rotl(std::size_t n, unsigned int c) noexcept {
  return (n << (mask & c)) | (n >> (-(mask & c)) & mask);
}

template<class T, class... Args>
static std::unique_ptr<T> make_unique_x(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

int rankN(ProfArgs&& args, int world_rank, int world_size) {
  auto srcs = ScatterStrings::scatterN(0);

  ProfilePipeline::Settings pipelineB1;
  ProfilePipeline::Settings pipelineB2;

  // We use (mostly) the same Sources for both Pipelines.
  for(const auto& p: srcs) {
    pipelineB1 << ProfileSource::create_for(p);
    pipelineB2 << ProfileSource::create_for(p);
  }

  // Fire off the first Pipeline, let rank 0 know all about our data.
  {
    struct Sender : public sinks::Packed {
      ExtensionClass requires() const noexcept override { return {}; }
      DataClass accepts() const noexcept override {
        return data::attributes + data::references + data::contexts
               + data::metrics + data::timepoints;
      }
      void write() override {
        std::vector<std::uint8_t> block;
        packAttributes(block);
        packReferences(block);
        packContexts(block);
        packTimepoints(block);
        Send(block, 0, 1);
      }
    } sender;
    pipelineB1 << sender;

    ProfilePipeline pipeline(std::move(pipelineB1), args.threads);
    pipeline.run();
  }

  // Nab the total number of Context from rank 0. Since by now we know.
  std::size_t ctxcnt;
  MPI_Bcast(&ctxcnt, 1, mpi_data<std::size_t>::type, 0, MPI_COMM_WORLD);

  // Fire off the second pipeline, integrating the new data from rank 0
  {
    // Most of the IDs can be pulled from the void, only the Context IDs
    // need to be adjusted.
    finalizers::DenseIds dids;
    pipelineB2 << dids;

    // Get the actual Context ids from rank 0, and unpack them into our space.
    IdUnpacker unpacker(Bcast<uint8_t>::bcastN());
    IdUnpacker::Expander eunpacker(unpacker);
    IdUnpacker::Finalizer funpacker(unpacker);
    pipelineB2 << eunpacker << funpacker;

    // When we're done, we need to send the final metrics up to rank 0
    struct MetricSender : public sinks::Packed {
      DataClass accepts() const noexcept override {
        return data::attributes + data::contexts + data::metrics;
      }
      void write() override {
        std::vector<std::uint8_t> block;
        packAttributes(block);
        packMetrics(block);
        Send(block, 0, 3);
      }
    } msender;
    pipelineB2 << msender;

    // We only emit our part of the MetricDB and TraceDB.
    std::unique_ptr<SparseDB> sdb;
    switch(args.format) {
    case ProfArgs::Format::exmldb:
      if(args.include_traces)
        pipelineB2 << make_unique_x<sinks::HPCTraceDB>(args.output, false);
      if(args.include_thread_local)
        pipelineB2 << make_unique_x<sinks::HPCMetricDB>(args.output);
      break;
    case ProfArgs::Format::sparse:
      if(args.include_traces)
        pipelineB2 << make_unique_x<sinks::HPCTraceDB>(args.output, false);
      if(args.include_thread_local)
        pipelineB2 << *(sdb = make_unique_x<SparseDB>(args.output));
      break;
    }

    ProfilePipeline pipeline(std::move(pipelineB2), args.threads);
    pipeline.run();
    if(sdb) sdb->merge(args.threads, ctxcnt);
  }

  return 0;
}
