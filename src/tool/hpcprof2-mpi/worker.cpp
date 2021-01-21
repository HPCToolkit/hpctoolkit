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

#include "tree.hpp"
#include "sparse.hpp"
#include "../hpcprof2/args.hpp"

#include "lib/profile/pipeline.hpp"
#include "lib/profile/source.hpp"
#include "lib/profile/packedids.hpp"
#include "lib/profile/sinks/packed.hpp"
#include "lib/profile/sinks/hpctracedb2.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/util/log.hpp"
#include "lib/profile/mpi/all.hpp"

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

int rankN(ProfArgs&& args) {
  ProfilePipeline::Settings pipelineB1;
  ProfilePipeline::Settings pipelineB2;

  // We use (mostly) the same Sources for both Pipelines.
#ifdef ENABLE_VG_ANNOTATIONS
  char start_arc;
  char end_arc;
#endif
  ANNOTATE_HAPPENS_BEFORE(&start_arc);
  #pragma omp parallel num_threads(args.threads)
  {
    ANNOTATE_HAPPENS_AFTER(&start_arc);
    std::vector<std::unique_ptr<ProfileSource>> my_sources;
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < args.sources.size(); i++)
      my_sources.emplace_back(ProfileSource::create_for(args.sources[i].second));
    #pragma omp critical
    for(auto& s: my_sources) pipelineB1 << std::move(s);
    ANNOTATE_HAPPENS_BEFORE(&end_arc);
  }
  ANNOTATE_HAPPENS_AFTER(&end_arc);
  for(auto& sp: args.sources) pipelineB2 << std::move(sp.first);

  std::size_t threadIdOffset;

  // Prep our role in the massive reduction tree
  std::vector<std::uint8_t> stash;
  RankTree tree{std::max<std::size_t>(args.threads, 2)};

  // Fire off the first Pipeline, let rank 0 know all about our data.
  {
    Sender sender{tree, stash};
    pipelineB1 << sender;

    Receiver::append(pipelineB1, tree);

    struct ThreadIDUniquer : public ProfileSink {
      ThreadIDUniquer(std::size_t& t) : threadIdOffset(t) {};
      ExtensionClass requires() const noexcept override { return {}; }
      DataClass accepts() const noexcept override { return data::threads; }
      void write() override {
        threadIdOffset = mpi::exscan(src.threads().size(), mpi::Op::sum()).value();
      }
      std::size_t& threadIdOffset;
    } tidsink{threadIdOffset};
    pipelineB1 << tidsink;

    ProfilePipeline pipeline(std::move(pipelineB1), args.threads);
    pipeline.run();
  }

  // Fire off the second pipeline, integrating the new data from rank 0
  {
    // The Statistics need to be consistent between all the ranks, or else
    // bad things happen
    ProfArgs::StatisticsExtender se(args);
    pipelineB2 << se;

    // Most of the IDs can be pulled from the void, only the Context IDs
    // need to be adjusted.
    finalizers::DenseIds dids;
    pipelineB2 << dids;

    // Get the actual Context ids from rank 0, and unpack them into our space.
    IdUnpacker unpacker(mpi::bcast<std::vector<uint8_t>>(0));
    IdUnpacker::Expander eunpacker(unpacker);
    IdUnpacker::Finalizer funpacker(unpacker);
    pipelineB2 << eunpacker << funpacker;

    // Adjust the Thread ids to be unique among the team.
    struct ThreadIDAdjuster : public ProfileFinalizer {
      ThreadIDAdjuster(std::size_t t) : threadIdOffset(t) {};
      ExtensionClass provides() const noexcept override { return extensions::identifier; }
      ExtensionClass requires() const noexcept override { return {}; }
      void thread(const Thread&, unsigned int& id) noexcept override {
        id += threadIdOffset;
      }
      std::size_t threadIdOffset;
    } tidfinal{threadIdOffset};
    pipelineB2 << tidfinal;

    // When we're done, we need to send the final metrics up towards rank 0
    MetricSender msender{tree, mpi::bcast<uint8_t>(0) > 0};
    pipelineB2 << msender;

    // For unpacking metrics, we need to be able to map the IDs back to their
    // Contexts. This does the magic for us.
    sources::Packed::ctx_map_t cmap;
    sources::Packed::ContextTracker ctracker(cmap);
    pipelineB2 << ctracker;

    // Our children will come back to us with the metric data, so we have to be
    // ready to accept it.
    MetricReceiver::append(pipelineB2, tree, cmap, stash);

    ProfilePipeline::WavefrontOrdering mpiDep;

    // We only emit our part of the MetricDB and TraceDB.
    std::unique_ptr<SparseDB> sdb;
    switch(args.format) {
    case ProfArgs::Format::sparse:
      if(args.include_traces)
        pipelineB2 << make_unique_x<sinks::HPCTraceDB2>(args.output) >> mpiDep;      
      pipelineB2 << *(sdb = make_unique_x<SparseDB>(args.output)) << mpiDep;
      break;
    }

    ProfilePipeline pipeline(std::move(pipelineB2), args.threads);
    pipeline.run();
    if(sdb) sdb->merge(args.threads, args.sparse_debug);

    if(args.valgrindUnclean) {
      mpi::World::finalize();
      std::exit(0);
    }
  }

  return 0;
}
