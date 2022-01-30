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
// Copyright ((c)) 2002-2023, Rice University
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

#include "../hpcprof/args.hpp"
#include "tree.hpp"

#include "lib/profile/mpi/core.hpp"
#include "lib/profile/pipeline.hpp"
#include "lib/profile/packedids.hpp"
#include "lib/profile/source.hpp"
#include "lib/profile/sources/packed.hpp"
#include "lib/profile/sinks/hpctracedb2.hpp"
#include "lib/profile/sinks/metadb.hpp"
#include "lib/profile/sinks/metricsyaml.hpp"
#include "lib/profile/sinks/sparsedb.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/finalizers/logical.hpp"
#include "lib/profile/finalizers/struct.hpp"
#include "lib/profile/finalizers/kernelsyms.hpp"
#include "lib/profile/util/log.hpp"
#include "lib/profile/mpi/all.hpp"

#include <mpi.h>
#include <iostream>

std::mutex mpitex;

using namespace hpctoolkit;

namespace {

class ThreadIdExscan : public ProfileSink {
public:
  ThreadIdExscan(std::size_t& result) : result(result) {};

  void write() override {}
  DataClass accepts() const noexcept override { return DataClass::threads; }
  ExtensionClass requires() const noexcept override { return {}; }
  DataClass wavefronts() const noexcept override { return DataClass::threads; }
  void notifyWavefront(DataClass wave) override {
    if(wave.hasThreads()) {
      util::call_once(once, [this]{
        result = src.threads().size();
      });
    }
  }

private:
  std::once_flag once;
  std::size_t& result;
};
class ThreadIdProvider : public ProfileFinalizer {
public:
  ThreadIdProvider(std::size_t offset) : nextId((unsigned int)offset) {};

  ExtensionClass provides() const noexcept override { return ExtensionClass::identifier; }
  ExtensionClass requires() const noexcept override { return {}; }
  std::optional<unsigned int> identify(const Thread&) noexcept override {
    return nextId.fetch_add(1, std::memory_order_relaxed);
  }

private:
  std::atomic<unsigned int> nextId;
};

class ManyIdPacker : public IdPacker {
public:
  ManyIdPacker(std::vector<uint8_t>& result) : result(result) {};

  void notifyPacked(std::vector<uint8_t>&& block) override {
    result = std::move(block);
  }

private:
  std::vector<uint8_t>& result;
};

}

int main(int argc, char* const argv[]) {
  // Fire up MPI. We just use the WORLD communicator for everything.
  mpi::World::initialize();

  // Read in the arguments.
  ProfArgs args(argc, argv);

  // Add the base Sources to the two Pipelines we'll be using.
  ProfilePipeline::Settings pipelineB1;
  ProfilePipeline::Settings pipelineB2;
#ifndef NVALGRIND
  char start_arc;
  char end_arc;
#endif  // !NVALGRIND
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

  // Common state across the entire process
  RankTree tree(std::max<std::size_t>(args.threads, 2));
  std::size_t threadIdOffset;
  std::vector<std::uint8_t> packedIds;
  std::deque<std::vector<std::uint8_t>> receivedBlocks;

  // Phase 1: Reduction (towards rank 0) of the elements that need to have
  // consistent ids across the ranks, namely Contexts and Metrics.
  {
    // Perform an exscan across the ranks to ensure Thread ids don't overlap.
    ThreadIdExscan tidex(threadIdOffset);
    pipelineB1 << tidex;

    // Make sure the files are searched for as they should be.
    pipelineB1 << std::make_unique<ProfArgs::Prefixer>(args);

    // In this phase, rank 0 is the one that provides structural information
    // and decides the ids for everything.
    if(mpi::World::rank() == 0) {
      // Make sure all Metrics have their Statistics set properly
      pipelineB1 << std::make_unique<ProfArgs::StatisticsExtender>(args);

      // Load in the Finalizers for special cases
      pipelineB1 << std::make_unique<finalizers::LogicalFile>();
      for(auto& sp : args.ksyms) pipelineB1 << std::move(sp.first);

      // Load in the Finalizers for Structfiles.
      for(auto& sp: args.structs) pipelineB1 << std::move(sp.first);
      pipelineB1 << std::make_unique<ProfArgs::StructPartialMatch>(args);

      if(!args.foreign) {
        // Insert the proper Finalizer for drawing data directly from the Modules.
        // This is used as a fallback if the Structfiles aren't available.
        pipelineB1 << std::make_unique<finalizers::DirectClassification>(args.dwarfMaxSize);
      }

      // Ids for everything are pulled from the void. We call the shots here.
      pipelineB1 << std::make_unique<finalizers::DenseIds>();

      // Rank 0 is in charge of packing up the ids for everyone else
      pipelineB1 << std::make_unique<ManyIdPacker>(packedIds);

      // Output the parts we can, here where we have the full information
      switch(args.format) {
      case ProfArgs::Format::metadb:
        pipelineB1 << std::make_unique<sinks::MetaDB>(args.output, args.include_sources)
                   << std::make_unique<sinks::MetricsYAML>(args.output);
        break;
      }
    } else {
      // Prevent any kind of classification to make Packed work.
      pipelineB1 << std::make_unique<sinks::Packed::DontClassify>();

      // We still need the Structfiles for FlowGraph data, but nothing else.
      for(auto& sp: args.structs) pipelineB1 << std::move(sp.first);
    }

    // Receive any bits from below us in the tree. We save these blocks and
    // inject them into the second Pipeline to make everything consistent.
    Receiver::append(pipelineB1, tree, receivedBlocks);

    // Ship our bits up when we're done here.
    if(mpi::World::rank() > 0)
      pipelineB1 << std::make_unique<Sender>(tree);

    ProfilePipeline pipeline(std::move(pipelineB1), args.threads);
    pipeline.run();
  }

  // Phase 2: Collective operations to synchronize the common state
  threadIdOffset = mpi::exscan(threadIdOffset, mpi::Op::sum()).value_or(0);
  packedIds = mpi::bcast(std::move(packedIds), 0);

  // Phase 3: Reduction (towards rank 0) of the metric values and such.
  {
    // Restore the bits we got in the first Pipeline, to keep things consistent.
    for(auto& block: receivedBlocks)
      pipelineB2 << std::make_unique<Receiver>(block);

    // The Statistics need to be consistent between all the ranks, since we only
    // stabilize the Metric ids.
    ProfArgs::StatisticsExtender se(args);
    pipelineB2 << se;

    // Thread ids need to be unique across the ranks.
    ThreadIdProvider tidp(threadIdOffset);
    pipelineB2 << tidp;

    // The actual Context/Metric ids are decided by rank 0. Unpack them here.
    IdUnpacker unpacker(std::move(packedIds));
    pipelineB2 << unpacker;

    // Make sure the files are searched for as they should be.
    pipelineB2 << std::make_unique<ProfArgs::Prefixer>(args);

    // We need to recreate the Context expansions identified by rank 0 here.
    // Load in a copy of all the finalizers.
    finalizers::LogicalFile lf;
    pipelineB2 << lf;
    for(auto& sp: args.ksyms)
      pipelineB2 << std::make_unique<finalizers::KernelSymbols>(sp.second);
    for(auto& sp: args.structs)
      pipelineB2 << std::make_unique<finalizers::StructFile>(sp.second, nullptr);

    if(!args.foreign) {
      // Insert the proper Finalizer for drawing data directly from the Modules.
      // This is used as a fallback if the Structfiles aren't available.
      pipelineB2 << std::make_unique<finalizers::DirectClassification>(args.dwarfMaxSize);
    }

    // For unpacking metrics, we need to be able to map ids back to Contexts and
    // Metrics. This handles that little detail.
    sources::Packed::IdTracker tracker;
    pipelineB2 << tracker;

    // Receive any bits from below us in the tree, and ship our bits up.
    MetricReceiver::append(pipelineB2, tree, tracker);
    if(mpi::World::rank() > 0)
      pipelineB2 << std::make_unique<MetricSender>(tree);

    // Finally, we get to write stuff out
    switch(args.format) {
    case ProfArgs::Format::metadb:
      pipelineB2 << std::make_unique<sinks::SparseDB>(args.output);
      if(args.include_traces)
        pipelineB2 << std::make_unique<sinks::HPCTraceDB2>(args.output);
      break;
    }

    ProfilePipeline pipeline(std::move(pipelineB2), args.threads);
    pipeline.run();

    if(args.valgrindUnclean) {
      mpi::World::finalize();
      std::exit(0);
    }
  }

  // Clean up and close up.
  mpi::World::finalize();
  return 0;
}
