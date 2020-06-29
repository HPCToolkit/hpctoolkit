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
#include "lib/profile/packedids.hpp"
#include "lib/profile/source.hpp"
#include "lib/profile/sources/packed.hpp"
#include "lib/profile/sinks/lambda.hpp"
#include "lib/profile/sinks/experimentxml.hpp"
#include "lib/profile/sinks/hpctracedb.hpp"
#include "lib/profile/sinks/hpcmetricdb.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/util/log.hpp"

#include <iostream>
#include <stack>

using namespace hpctoolkit;
using namespace hpctoolkit::literals;
namespace fs = stdshim::filesystem;

template<class T, class... Args>
static std::unique_ptr<T> make_unique_x(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

int rank0(ProfArgs&& args, int world_rank, int world_size) {
  // We only have one Pipeline, this is its builder.
  ProfilePipeline::Settings pipelineB;

  // Divvy up the inputs across the WORLD.
  {
    ScatterStrings paths;
    int rank = world_size == 1 ? 0 : 1;  // Allocate to rank 1 first.
    for(auto& sp: args.sources) {
      auto src = std::move(sp.first);
      if(rank == 0) pipelineB << std::move(src);
      else paths.add(rank, sp.second.string());
      rank++;
      if(rank >= world_size) rank = 0;
    }
    paths.scatter0(0);
  }

  // When time, gather up the data from all our friends and emit it.
  struct Receiver : public sources::Packed {
    int peer;
    bool done;
    Receiver(int p) : peer(p), done(false) {};
    DataClass provides() const noexcept override {
      return data::references + data::attributes + data::contexts
             + data::timepoints;
    }
    void read(const DataClass&) override {
      if(done) return;
      auto block = Receive<std::uint8_t>(peer, 1);
      iter_t it = block.begin();
      it = unpackAttributes(it);
      it = unpackReferences(it);
      it = unpackContexts(it);
      if(sink.limit().hasTimepoints()) it = unpackTimepoints(it);
      done = true;
    }
  };
  for(int peer = 1; peer < world_size; peer++)
    pipelineB << make_unique_x<Receiver>(peer);

  // Load in the Finalizers for Structfiles.
  for(auto& sp: args.structs) pipelineB << std::move(sp.first);
  ProfArgs::StructWarner sw(args);
  pipelineB << sw;

  // Make sure the files are searched for as they should be
  ProfArgs::Prefixer pr(args);
  pipelineB << pr;

  // Insert the proper Finalizer for drawing data directly from the Modules.
  finalizers::DirectClassification dc;
  pipelineB << dc;

  // Do the Classification-based expansions, but pack them up for later.
  IdPacker packer;
  IdPacker::Classifier cpacker(packer);
  pipelineB << cpacker;

  // Ids for everything are pulled from the void. We call the shots here.
  finalizers::DenseIds dids;
  pipelineB << dids;

  // When everything is ready, ship off the block to the workers.
  struct Sender : public IdPacker::Sink {
    Sender(IdPacker& s) : IdPacker::Sink(s), ctxcnt(0) {};
    std::size_t ctxcnt;
    void count(const Context& c) {
      ctxcnt += 1;
      for(const Context& cc: c.children().citerate()) count(cc);
    }
    void notifyPacked(std::vector<uint8_t>&& block) override {
      // We need to know the total number of Contexts in the system.
      // Easiest way to know is to scan through the Contexts quick
      count(src.contexts());
      MPI_Bcast(&ctxcnt, 1, mpi_data<std::size_t>::type, 0, MPI_COMM_WORLD);

      Bcast<uint8_t> bc;
      bc.contents = std::move(block);
      bc.bcast0();
    }
  } spacker(packer);
  pipelineB << spacker;

  // For unpacking metrics, we need to be able to map the IDs back to their
  // Contexts. This does the magic for us.
  sources::Packed::ctx_map_t cmap;
  sources::Packed::ContextTracker ctracker(cmap);
  pipelineB << ctracker;

  // The workers will come back to us with the metric data, so we have to be
  // ready to accept it.
  struct MetricReciever : public sources::Packed {
    ctx_map_t& cmap;
    int peer;
    bool done;
    MetricReciever(int p, ctx_map_t& c) : cmap(c), peer(p), done(false) {};
    DataClass provides() const noexcept override { return data::attributes + data::metrics; }
    void read(const DataClass& d) override {
      if(!d.hasMetrics() || done) return;
      auto block = Receive<std::uint8_t>(peer, 3);
      iter_t it = block.begin();
      it = unpackAttributes(it);
      it = unpackMetrics(it, cmap);
      done = true;
    }
  };
  for(int peer = 1; peer < world_size; peer++)
    pipelineB << make_unique_x<MetricReciever>(peer, cmap);

  // Finally, eventually we get to actually write stuff out.
  std::unique_ptr<SparseDB> sdb;
  switch(args.format) {
  case ProfArgs::Format::exmldb: {
    std::unique_ptr<sinks::HPCTraceDB> tdb;
    if(args.include_traces)
      tdb = make_unique_x<sinks::HPCTraceDB>(args.output, false);
    std::unique_ptr<sinks::HPCMetricDB> mdb;
    if(args.include_thread_local)
      mdb = make_unique_x<sinks::HPCMetricDB>(args.output);
    auto exml = make_unique_x<sinks::ExperimentXML>(args.output, args.include_sources,
                                                    tdb.get(), mdb.get());
    pipelineB << std::move(tdb) << std::move(mdb) << std::move(exml);

    // ExperimentXML doesn't support instruction-level metrics, so we need a
    // line-merging transformer. Since this only changes the Scope, we don't
    // need to track it.
    if(!args.instructionGrain)
      pipelineB << make_unique_x<LineMergeTransformer>();
    break;
  }
  case ProfArgs::Format::sparse: {
    std::unique_ptr<sinks::HPCTraceDB> tdb;
    if(args.include_traces)
      tdb = make_unique_x<sinks::HPCTraceDB>(args.output, false);
    if(args.include_thread_local)
      sdb = make_unique_x<SparseDB>(args.output);
    auto exml = make_unique_x<sinks::ExperimentXML>(args.output, args.include_sources,
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
  if(sdb) sdb->merge(args.threads, spacker.ctxcnt);

  return 0;
}
