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
#include "lib/profile/metricsources/lambda.hpp"
#include "lib/profile/profilesinks/lambda.hpp"
#include "lib/profile/profilesinks/hpctracedb.hpp"
#include "lib/profile/profilesinks/hpcmetricdb.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/finalizers/lambda.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/profargs.hpp"

#include "mpi-strings.h"

#include <mpi.h>
#include <iostream>

using namespace hpctoolkit;
using namespace hpctoolkit::literals;
namespace fs = stdshim::filesystem;

struct TraceHelper : public ProfileSink {
  TraceHelper() : min(std::chrono::nanoseconds::max()),
    max(std::chrono::nanoseconds::min()) {};

  DataClass accepts() const noexcept { return "t"_dat; }
  ExtensionClass requires() const noexcept { return ""_ext; }

  bool write(std::chrono::nanoseconds) { return true; }

  void trace(const Thread&, unsigned long, std::chrono::nanoseconds t, const Context&) {
    if(t < min) min = t;
    if(max < t) max = t;
  }

  std::chrono::nanoseconds min;
  std::chrono::nanoseconds max;
};

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
  auto srcs = ScatterStrings::scatterN();

  PipelineBuilder pipelineB1;
  PipelineBuilder pipelineB2;

  // We use (mostly) the same Sources for both Pipelines.
  for(const auto& p: srcs) {
    pipelineB1 << MetricSource::create_for(p);
    pipelineB2 << MetricSource::create_for(p);
  }

  // We need to keep some data between Pipelines to sort everything out.
  std::vector<fs::path> modules;
  std::vector<Metric::ident> metrics;

  std::unique_ptr<profilesinks::HPCTraceDB> tdb;
  std::unique_ptr<profilesinks::HPCMetricDB> mdb;
  switch(args.format) {
  case ProfArgs::Format::exmldb:
    if(args.include_traces)
      tdb = make_unique_x<profilesinks::HPCTraceDB>(args.output, false);
    if(args.include_thread_local)
      mdb = make_unique_x<profilesinks::HPCMetricDB>(args.output, true);
    break;
  }

  // Fire off the first Pipeline, let rank 0 know all about our data.
  {
    profilesinks::Lambda sender("rc"_dat, ""_ext, [&](ProfilePipeline::SinkEnd& src){
      // Gather up all the attributes and send them up to rank 0
      {
        GatherStrings gs;
        gs.add(src.attributes().name());
        gs.add(src.attributes().path().string());
        for(const auto& kv: src.attributes().environment()) {
          gs.add(kv.first);
          gs.add(kv.second);
        }
        gs.gatherN();
        uint64_t job = 0xFEF1F0F3ULL << 32;
        if(src.attributes().has_job())
          job = src.attributes().job();
        MPI_Gather(&job, 1, MPI_UINT64_T, nullptr, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
      }
      // Gather up all the modules and send them up to rank 0
      std::unordered_map<const Module*, std::size_t> modmap;
      {
        GatherStrings gs;
        for(const auto& m: src.modules()) {
          modmap[&m()] = modules.size();
          modules.push_back(m().path());
          gs.add(m().path().string());
        }
        gs.gatherN();
      }
      // Gather up all the metrics and send them up to rank 0
      std::vector<const Metric*> metorder;
      {
        GatherStrings gs;
        for(const auto& m: src.metrics()) {
          metorder.emplace_back(&m());
          metrics.push_back({m().name(), m().description()});
          gs.add(m().name());
          gs.add(m().description());
        }
        gs.gatherN();
      }
      // Walk through the context tree and ship it up to rank 0
      Gather<uint64_t> g;
      std::function<void(const Context&)> handle = [&](const Context& c){
        if(c.scope().type() == Scope::Type::point) {
          // Format: [module id] [offset] [ex metrics]... children... [sentinal]
          auto mo = c.scope().point_data();
          g.add(modmap.at(&mo.first));
          g.add(mo.second);
        } else if(c.scope().type() == Scope::Type::unknown) {
          // Format: [magic] [ex metrics]... children... [sentinal]
          g.add(0xF0F1F2F3ULL << 32);
        } else throw std::logic_error("Unhandled Scope in prof-mpi!");
        for(const auto& m: metorder) {
          auto v = m->getFor(c);
          g.add(v.first);
        }
        for(const auto& cc: c.children()) handle(cc());
        g.add(0xFEF1F0F3ULL << 32);
      };
      for(const auto& c: src.contexts().children()) handle(c);
      g.gatherN();
    });
    pipelineB1 << sender;

    TraceHelper th;
    if(tdb) pipelineB1 << th;

    ProfilePipeline pipeline(std::move(pipelineB1), args.threads);
    pipeline.drain();

    if(tdb) {
      Gather<uint64_t> g;
      g.add(th.min.count());
      g.add(th.max.count());
      g.gatherN();
    }
  }

  // Fetch the final bits of data from rank 0. (As per MPI, we have to
  // participate in everything even if we don't actually need the data.)
  auto modpaths = BcastStrings::bcastN();
  auto ctxtree = Bcast<uint64_t>::bcastN();

  // Fire off the second pipeline, integrating the new data from rank 0
  {
    // Most of the IDs can be pulled from the void, only the Context IDs
    // need to be adjusted.
    finalizers::DenseIds dids;
    pipelineB2 << dids;

    // Set up some maps for easy ID assignment and such.
    using tup_t = std::tuple<unsigned int, const Module*, uint64_t>;
    struct tuphash {
      std::hash<unsigned int> h_id;
      std::hash<const Module*> h_mod;
      std::hash<uint64_t> h_off;
      std::size_t operator()(const tup_t& v) const noexcept {
        std::size_t sponge = rotl(h_id(std::get<0>(v)), 5);
        sponge = rotl(sponge ^ h_mod(std::get<1>(v)), 7);
        sponge = rotl(sponge ^ h_off(std::get<2>(v)), 11);
        return sponge;
      }
    };
    unsigned int globalid = ctxtree[0];
    std::unordered_map<tup_t, std::vector<unsigned int>, tuphash> cmap_module;
    std::unordered_map<unsigned int, std::vector<unsigned int>> cmap_unknown;

    // Do all the things only a Transformer can do.
    std::once_flag once;
    const Module* exmod = nullptr;
    std::vector<const Module*> modmap;
    LambdaTransformer lmtrans;
    lmtrans.set([&](ProfilePipeline::SourceEnd& sink, Context& c, Scope& s) -> Context& {
      internal::call_once(once, [&](){
        exmod = &sink.module("/nonexistent/prof-mpi/exmod");
        for(const auto& s: modpaths) modmap.emplace_back(&sink.module(s));

        for(std::size_t i = 1; i < ctxtree.size(); ) {
          std::size_t cnt;
          std::vector<unsigned int>* cids;
          if(ctxtree[i+1] == (0xF0F1F2F3ULL << 32)) {
            // Format: [parent id] [magic] [cnt] [children ids]...
            cids = &cmap_unknown[ctxtree[i]];
            cnt = ctxtree[i+2];
            i += 3;
          } else {
            // Format: [parent id] [module id] [offset] [cnt] [children ids]...
            cids = &cmap_module[{ctxtree[i], modmap.at(ctxtree[i+1]), ctxtree[i+2]}];
            cnt = ctxtree[i+3];
            i += 4;
          }
          for(std::size_t x = 0; x < cnt; x++)
            cids->emplace_back(ctxtree[i + x]);
          i += cnt;
        }
        ctxtree.clear();
      });

      std::vector<unsigned int>* ids;
      if(s.type() == Scope::Type::point) {
        auto mo = s.point_data();
        ids = &cmap_module.at({c.userdata[sink.extensions().id_context], &mo.first, mo.second});
      } else if(s.type() == Scope::Type::unknown) {
        ids = &cmap_unknown.at(c.userdata[sink.extensions().id_context]);
      } else throw std::logic_error("Unhandled Scope in prof-mpi!");

      Context* cp = &c;
      bool first = true;
      for(const auto& id: *ids) {
        if(!first) cp = &sink.context(*cp, s);
        s = Scope(*exmod, id);
        first = false;
      }
      return *cp;
    });
    pipelineB2 << lmtrans;

    // Finish up by translating the exmod Scopes into IDs
    finalizers::Lambda ids([&](ProfilePipeline::SourceEnd&, const Context& c, unsigned int& id) {
      if(c.scope().type() == Scope::Type::global) {
        id = globalid;
        return;
      }
      if(c.scope().type() != Scope::Type::point)
        throw std::logic_error("Unhandled Scope in prof-mpi!");
      auto mo = c.scope().point_data();
      if(&mo.first != exmod) {
        std::cerr << mo.first.path() << "\n";
        throw std::logic_error("Scope missed conversion in prof-mpi!");
      }
      id = mo.second;
    });
    pipelineB2 << ids;

    // We only emit our part of the MetricDB and TraceDB.
    pipelineB2 << std::move(mdb) << std::move(tdb);

    ProfilePipeline pipeline(std::move(pipelineB2), args.threads);
    pipeline.drain();
  }

  return 0;
}
