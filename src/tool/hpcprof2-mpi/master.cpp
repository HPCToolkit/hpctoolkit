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
#include "lib/profile/packedids.hpp"
#include "lib/profile/source.hpp"
#include "lib/profile/sources/packed.hpp"
#include "lib/profile/sinks/experimentxml4.hpp"
#include "lib/profile/sinks/hpctracedb2.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/util/log.hpp"
#include "lib/profile/mpi/all.hpp"

#include <iostream>
#include <stack>

using namespace hpctoolkit;
using namespace hpctoolkit::literals;
namespace fs = stdshim::filesystem;

template<class T, class... Args>
static std::unique_ptr<T> make_unique_x(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

int rank0(ProfArgs&& args) {
  // We only have one Pipeline, this is its builder.
  ProfilePipeline::Settings pipelineB;
  for(auto& sp: args.sources) pipelineB << std::move(sp.first);

  // Set up our reduction tree with our fellow peers
  RankTree tree{std::max<std::size_t>(args.threads, 2)};

  // When time, gather up the data from all our friends and emit it.
  Receiver::append(pipelineB, tree);

  // Make sure all Metrics have their Statistics set properly
  ProfArgs::StatisticsExtender se(args);
  pipelineB << se;

  // Load in the Finalizers for Structfiles.
  for(auto& sp: args.structs) pipelineB << std::move(sp.first);
  ProfArgs::StructWarner sw(args);
  pipelineB << sw;

  // Make sure the files are searched for as they should be
  ProfArgs::Prefixer pr(args);
  pipelineB << pr;

  // Insert the proper Finalizer for drawing data directly from the Modules.
  finalizers::DirectClassification dc(args.dwarfMaxSize);
  pipelineB << dc;

  // Now that Modules will be Classified during Finalization, add a Transformer
  // to expand the Contexts as they enter the Pipe.
  RouteExpansionTransformer retrans;
  ClassificationTransformer ctrans;
  pipelineB << retrans << ctrans;

  // Ids for everything are pulled from the void. We call the shots here.
  finalizers::DenseIds dids;
  pipelineB << dids;

  // Ensure that our Thread ids are unique with respect to the workers.
  // Since this is rank 0, we don't need to adjust our ids.
  struct ThreadIDUniquer : public ProfileSink {
    void write() override {}
    DataClass accepts() const noexcept override { return DataClass::threads; }
    ExtensionClass requires() const noexcept override { return {}; }
    DataClass wavefronts() const noexcept override { return DataClass::threads; }
    void notifyPipeline() noexcept override {
      src.registerOrderedWavefront();
    }
    void notifyWavefront(DataClass wave) override {
      if(!wave.hasThreads()) return;
      auto mpiSem = src.enterOrderedWavefront();
      mpi::exscan(src.threads().size(), mpi::Op::sum());
    }
  } tiduniquer;
  pipelineB << tiduniquer;

  // No-op Source to detect whether the Pipeline requires timepoints
  // It should be args.include_traces, but you can never be too certain
  struct Detector : public ProfileSource {
    DataClass provides() const noexcept override { return DataClass::timepoints; }
    DataClass finalizeRequest(const DataClass& d) const noexcept override { return d; }
    void read(const DataClass&) override {};
    bool needsTimepoints() const { return sink.limit().hasTimepoints(); }
  } detector;
  pipelineB << detector;

  // When everything is ready, ship off the block to the workers.
  struct Sender : public IdPacker {
    Sender(Detector& d) : detector(d) {};
    void notifyPipeline() noexcept override {
      IdPacker::notifyPipeline();
      src.registerOrderedWavefront();
    }
    void notifyPacked(std::vector<uint8_t>&& block) override {
      auto mpiSem = src.enterOrderedWavefront();
      mpi::bcast(std::move(block), 0);
      mpi::bcast((uint8_t)(detector.needsTimepoints() ? 1 : 0), 0);
    }
    Detector& detector;
  } spacker(detector);
  pipelineB << spacker;

  // For unpacking metrics, we need to be able to map the IDs back to their
  // Contexts. This does the magic for us.
  sources::Packed::ctx_map_t cmap;
  sources::Packed::ContextTracker ctracker(cmap);
  pipelineB << ctracker;

  // The workers will come back to us with the metric data, so we have to be
  // ready to accept it.
  MetricReceiver::append(pipelineB, tree, cmap);

  // Finally, eventually we get to actually write stuff out.
  std::unique_ptr<SparseDB> sdb;
  switch(args.format) {
  case ProfArgs::Format::sparse: {
    std::unique_ptr<sinks::HPCTraceDB2> tdb;
    if(args.include_traces)
      tdb = make_unique_x<sinks::HPCTraceDB2>(args.output);
    sdb = make_unique_x<SparseDB>(args.output, args.threads);
    auto exml = make_unique_x<sinks::ExperimentXML4>(args.output, args.include_sources,
                                                     tdb.get());
    pipelineB << std::move(tdb) << std::move(exml);
    if(sdb) pipelineB << *sdb;

    // ExperimentXML doesn't support instruction-level metrics, so we need a
    // line-merging transformer. Since this only changes the Scope, we don't
    // need to track it.
    if(!args.instructionGrain)
      pipelineB << make_unique_x<LineMergeTransformer>();
    break;
  }
  }

  // Create and drain the Pipeline, that's all we do.
  ProfilePipeline pipeline(std::move(pipelineB), args.threads);
  pipeline.run();
  if(sdb) sdb->merge(args.threads, args.sparse_debug);

  if(args.valgrindUnclean) {
    mpi::World::finalize();
    std::exit(0);
  }

  return 0;
}
