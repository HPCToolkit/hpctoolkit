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

#include "lib/profile/profile.hpp"
#include "lib/profile/metricsource.hpp"
#include "lib/profile/profilesinks/lambda.hpp"
#include "lib/profile/profilesinks/experimentxml.hpp"
#include "lib/profile/profilesinks/hpctracedb.hpp"
#include "lib/profile/profilesinks/hpcmetricdb.hpp"
#include "lib/profile/metricsources/lambda.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/profargs.hpp"

#include "mpi-strings.h"

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
  PipelineBuilder pipelineB;

  // Divvy up the inputs across the WORLD.
  {
    ScatterStrings paths(world_size);
    int rank = world_size == 1 ? 0 : 1;  // Allocate to rank 1 first.
    for(auto& sp: args.sources) {
      auto src = std::move(sp.first);
      if(rank == 0) pipelineB << std::move(src);
      else paths.add(rank, sp.second.string());
      rank++;
      if(rank >= world_size) rank = 0;
    }
    paths.scatter0();
  }

  // Up here for access.
  profilesinks::HPCTraceDB* tdb = nullptr;

  // When time, gather up the data from all our friends and emit it.
  metricsources::Lambda receiver("ra"_dat, [&](ProfilePipeline::SourceEnd& sink){
    // First the attributes: name, path, job, and environment.
    {
      auto strs = GatherStrings::gather0(world_size);
      std::vector<uint64_t> jobs(world_size, 0);
      MPI_Gather(MPI_IN_PLACE, 1, MPI_UINT64_T, jobs.data(), 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

      for(int r = 1; r < world_size; r++) {
        ProfileAttributes attr;
        if(!strs[r][0].empty()) attr.name(std::move(strs[r][0]));
        if(!strs[r][1].empty()) attr.path(std::move(strs[r][1]));
        for(std::size_t i = 2; i < strs[r].size(); i += 2)
          attr.environment(strs[r][i], strs[r][i+1]);
        if(jobs[r] != (0xFEF1F0F3ULL << 32)) attr.job(jobs[r]);
        sink.attributes(std::move(attr));
      }
    }
    // Then the Modules
    std::vector<std::vector<const Module*>> modmaps(world_size);
    {
      auto mods = GatherStrings::gather0(world_size);
      for(int r = 1; r < world_size; r++) {  // Skip ourselves, of course
        auto& ms = mods[r];
        for(std::size_t i = 0; i < ms.size(); i++)
          modmaps[r].emplace_back(&sink.module(ms[i]));
      }
    }
    // Then the Metrics
    std::vector<std::vector<const Metric*>> metmaps(world_size);
    {
      auto mets = GatherStrings::gather0(world_size);
      for(int r = 1; r < world_size; r++) {  // Skip ourselves, of course
        auto& ms = mets[r];
        for(std::size_t i = 0; i < ms.size(); i += 2)
          metmaps[r].emplace_back(&sink.metric(ms[i], ms[i+1]));
      }
    }
    // Then the metric data itself, in a compressed format.
    auto ctxs = Gather<uint64_t>::gather0(world_size);
    for(int r = 1; r < world_size; r++) {  // Skip ourselves, of course
      auto& cs = ctxs[r];
      std::stack<Context*, std::vector<Context*>> tip;
      for(std::size_t i = 0; i < cs.size(); ) {
        if(cs[i] == (0xFEF1F0F3ULL << 32)) {  // Sentinal
          tip.pop();
          i++;
        } else {
          Scope s;
          if(cs[i] == (0xF0F1F2F3ULL << 32)) {
            // Format: [magic] [ex metrics]...
            s = Scope();  // Unknown Scope
            i++;
          } else {
            // Format: [module id] [offset] [ex metrics]...
            s = Scope(*modmaps[r].at(cs[i]), cs[i+1]);  // Module Scope
            i += 2;
          }
          auto cp = tip.empty() ? &sink.context(s) : &sink.context(*tip.top(), s);
          for(const auto& mp: metmaps[r]) sink.add(*cp, *mp, cs[i++]);
          tip.push(cp);
        }
      }
      if(!tip.empty())
        throw std::logic_error("Bad compressed data from worker rank!");
    }
    // Finally the trace data-ish. TODO: Make a proper API for this.
    if(tdb) {
      auto tbs = Gather<uint64_t>::gather0(world_size);
      for(int r = 1; r < world_size; r++) {
        tdb->extra(std::chrono::nanoseconds(tbs[r][0]), std::chrono::nanoseconds(tbs[r][1]));
      }
    }
  });
  pipelineB << receiver;

  // Load in the Finalizers for Structfiles.
  for(auto& sp: args.structs) pipelineB << std::move(sp.first);

  // Insert the proper Finalizer for drawing data directly from the Modules.
  finalizers::DirectClassification dc(args);
  pipelineB << dc;

  // Since we have to keep track of the ids for just about everything, we
  // do the transformations ourselves.
  std::mutex ctxtree_m;
  Bcast<uint64_t> ctxtree;
  ctxtree.add(0);
  LambdaTransformer lmtrans;
  lmtrans.set([&](ProfilePipeline::SourceEnd& sink, Context& c, Scope& s) -> Context& {
    Context* cp = &c;
    Scope ss = s;
    // Before we lock, we do the transformations and mark down the ids.
    std::vector<unsigned int> ids;
    if(s.type() == Scope::Type::point) {
      auto mo = s.point_data();
      auto ss = mo.first.userdata[sink.extensions().classification].getScopes(mo.second);
      for(auto it = ss.crbegin(); it != ss.crend(); ++it) {
        cp = &sink.context(*cp, *it);
        ids.emplace_back(cp->userdata[sink.extensions().id_context]);
      }
      s = {mo.first, mo.first.userdata[sink.extensions().classification].getCanonicalAddr(mo.second)};
    }
    // We also need the ID of the final child Context. So we just emit it
    // and let the anti-recursion keep us from spinning out of control.
    ids.emplace_back(sink.context(*cp, s).userdata[sink.extensions().id_context]);

    // Now we can write the entry for out friends to work with.
    // Format: [parent id] (Scope) [cnt] [children ids]...
    std::unique_lock<std::mutex> l(ctxtree_m);
    ctxtree.add(c.userdata[sink.extensions().id_context]);
    if(s.type() == Scope::Type::point) {
      // Format: [module id] [offset]
      auto mo = ss.point_data();
      ctxtree.add(mo.first.userdata[sink.extensions().id_module]);
      ctxtree.add(mo.second);
    } else if(s.type() == Scope::Type::unknown) {
      // Format: [magic]
      ctxtree.add(0xF0F1F2F3ULL << 32);
    } else
      throw std::logic_error("Unhandled Scope type in prof-mpi!");
    ctxtree.add(ids.size());
    for(const auto& id: ids) ctxtree.add(id);
    return *cp;
  });
  pipelineB << lmtrans;

  // Ids for everything are pulled from the void. We call the shots here.
  finalizers::DenseIds dids;
  pipelineB << dids;

  // When all is said and done, we should let our friends know
  profilesinks::Lambda sender("rc"_dat, "i"_ext, [&](ProfilePipeline::SinkEnd& src) {
    {
      BcastStrings bs;
      for(const auto& m: src.modules()) {
        const auto& mid = m().userdata[src.extensions().id_module];
        if(bs.contents.size() <= mid)
          bs.contents.resize(mid+1, "");
        bs.contents[mid] = m().path().string();
        bs.total_size += bs.contents[mid].size();
      }
      bs.total_size += bs.contents.size();
      bs.bcast0();
    }
    ctxtree.contents[0] = src.contexts().userdata[src.extensions().id_context];
    ctxtree.bcast0();
    ctxtree.contents.clear();
  });
  pipelineB << sender;

  switch(args.format) {
  case ProfArgs::Format::exmldb: {
    std::unique_ptr<profilesinks::HPCTraceDB> tdb_up;
    if(args.include_traces)
      tdb_up = make_unique_x<profilesinks::HPCTraceDB>(args.output, false);
    tdb = tdb_up.get();
    std::unique_ptr<profilesinks::HPCMetricDB> mdb;
    if(args.include_thread_local)
      mdb = make_unique_x<profilesinks::HPCMetricDB>(args.output, true);
    auto exml = make_unique_x<profilesinks::ExperimentXML>(args, tdb, mdb.get());
    pipelineB << std::move(tdb_up) << std::move(mdb) << std::move(exml);
    break;
  }
  }

  // Create and drain the Pipeline, that's all we do.
  ProfilePipeline pipeline(std::move(pipelineB), args.threads);
  pipeline.drain(DataClass::all());

  return 0;
}
