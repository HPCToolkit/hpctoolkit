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

#include "profile.hpp"

#include "metricsource.hpp"
#include "profilesink.hpp"
#include "transformer.hpp"
#include "finalizer.hpp"

#include <iomanip>
#include <stdexcept>
#include <limits>
#include <iostream>

using namespace hpctoolkit;
using SourceEnd = ProfilePipeline::SourceEnd;
using SinkEnd = ProfilePipeline::SinkEnd;

const ProfilePipeline::timeout_t ProfilePipeline::timeout_forever;

PipelineBuilder& PipelineBuilder::operator<<(MetricSource& s) {
  sources.emplace_back(&s);
  return *this;
}
PipelineBuilder& PipelineBuilder::operator<<(std::unique_ptr<MetricSource>&& sp) {
  if(!sp) return *this;
  up_sources.emplace_back(std::move(sp));
  return operator<<(*up_sources.back());
}

PipelineBuilder& PipelineBuilder::operator<<(ProfileSink& s) {
  if((s.requires() - available).has_any())
    throw std::logic_error("Sink requires unavailable extended data!");
  sinks.emplace_back(&s);
  return *this;
}
PipelineBuilder& PipelineBuilder::operator<<(std::unique_ptr<ProfileSink>&& sp) {
  if(!sp) return *this;
  up_sinks.emplace_back(std::move(sp));
  return operator<<(*up_sinks.back());
}

PipelineBuilder& PipelineBuilder::operator<<(Finalizer& f) {
  available += f.provides();
  finalizers.emplace_back(&f);
  return *this;
}
PipelineBuilder& PipelineBuilder::operator<<(std::unique_ptr<Finalizer>&& fp) {
  if(!fp) return *this;
  up_finalizers.emplace_back(std::move(fp));
  return operator<<(*up_finalizers.back());
}

PipelineBuilder& PipelineBuilder::operator<<(InlineTransformer& t) {
  transformers.emplace_back(&t);
  return *this;
}
PipelineBuilder& PipelineBuilder::operator<<(std::unique_ptr<InlineTransformer>&& tp) {
  if(!tp) return *this;
  up_transformers.emplace_back(std::move(tp));
  return operator<<(*up_transformers.back());
}

ProfilePipeline::ProfilePipeline(PipelineBuilder&& b, std::size_t team_sz)
  : sources(std::move(b.sources)), sinks(std::move(b.sinks)),
    transformers(std::move(b.transformers)), finalizers(std::move(b.finalizers)),
    up_sources(std::move(b.up_sources)), up_sinks(std::move(b.up_sinks)),
    up_transformers(std::move(b.up_transformers)), up_finalizers(std::move(b.up_finalizers)),
    cct(nullptr), team_size(team_sz) {
  // Prep the Classification and ID members first thing
  if(b.available.has_identifier()) {
    uds.id_file = structs.file.add_default<unsigned int>(
      [this](unsigned int& id, const File& f){
        for(auto& fp: finalizers) fp->file(f, id);
      });
    uds.id_context = structs.context.add_default<unsigned int>(
      [this](unsigned int& id, const Context& c){
        for(auto& fp: finalizers) fp->context(c, id);
      });
    uds.id_function = structs.function.add_default<unsigned int>(
      [this](unsigned int& id, const Module& m, const Function& f){
        for(auto& fp: finalizers) fp->function(m, f, id);
      });
    uds.id_module = structs.module.add_default<unsigned int>(
      [this](unsigned int& id, const Module& m){
        for(auto& fp: finalizers) fp->module(m, id);
      });
    uds.id_metric = structs.metric.add_default<std::pair<unsigned int, unsigned int>>(
      [this](std::pair<unsigned int, unsigned int>& ids, const Metric& m){
        for(auto& fp: finalizers) fp->metric(m, ids);
      });
    uds.id_thread = structs.thread.add_default<unsigned int>(
      [this](unsigned int& id, const Thread& t){
        for(auto& fp: finalizers) fp->thread(t, id);
      });
    uds.classification = structs.module.add_default<Classification>(
      [this](Classification& cl, const Module& m){
        for(const auto& fp: finalizers) {
          auto filter = fp->filterModule();
          if(filter.empty() || m.path() == filter) fp->module(m, cl);
        }
      });
  }

  // Output is prepped first, in case the input is a little early.
  DataClass all_requested;
  for(std::size_t i = 0; i < sinks.size(); i++) {
    sinks[i]->bindPipeline(SinkEnd(*this, i));
    all_requested |= sinks[i]->accepts();
  }
  structs.file.freeze();
  structs.context.freeze();
  structs.function.freeze();
  structs.module.freeze();
  structs.metric.freeze();
  structs.thread.freeze();

  // Make sure the Finalizers are ready before anything else happens.
  for(auto& f: finalizers) f->bindPipeline(SourceEnd(*this));

  // Now we can connect the input without losing any information.
  for(auto& ms: sources) {
    ms->bindPipeline(SourceEnd(*this));
    all_scheduled |= ms->provides();
  }
  all_scheduled &= all_requested;
  for(std::size_t i = 0; i < transformers.size(); i++)
    transformers[i]->bindPipeline(SourceEnd(*this, i));

  // Last, set up the global Context.
  cct.reset(new Context(structs.context, metstruct, Scope(*this)));
  for(auto& s: sinks) s->notifyContext(*cct);
  cct->userdata.initialize();
}

ProfilePipeline& ProfilePipeline::operator<<(std::unique_ptr<InlineTransformer>&& up) {
  if(!up) return *this;
  up_transformers.emplace_back(std::move(up));
  return operator<<(*up_transformers.back());
}
ProfilePipeline& ProfilePipeline::operator<<(InlineTransformer& trans) {
  transformers.emplace_back(&trans);
  trans.bindPipeline(SourceEnd(*this, transformers.size()-1));
  return *this;
}

bool ProfilePipeline::scheduled(DataClass::singleton_t c) const noexcept {
  return (all_scheduled & c) == c;
}

bool ProfilePipeline::pull(const DataClass& d, timeout_t to) {
  #pragma omp parallel for schedule(dynamic) num_threads(team_size)
  for(std::size_t idx = 0; idx < sources.size(); ++idx)
    sources[idx]->read(d & all_scheduled);
  return true;
}

ProfileAttributes& ProfilePipeline::stuffed(DataClass::attributes_t) {
  return attrs;
}
std::pair<
  internal::locked_unordered_uniqued_set<Module>&,
  internal::locked_unordered_uniqued_set<File>&
> ProfilePipeline::stuffed(DataClass::references_t) { return {mods, files}; }
internal::locked_unordered_uniqued_set<Metric>& ProfilePipeline::stuffed(DataClass::metrics_t) {
  return mets;
}
Context& ProfilePipeline::stuffed(DataClass::contexts_t) { return *cct; }

bool ProfilePipeline::drain(const DataClass&, timeout_t) {
  pull(DataClass::all());
  #pragma omp parallel num_threads(team_size)
  {
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t idx = 0; idx < sinks.size(); ++idx)
      sinks[idx]->write();
    for(std::size_t idx = 0; idx < sinks.size(); ++idx)
      sinks[idx]->help();
  }
  return true;
}

SourceEnd::SourceEnd() : pipe(nullptr), tskip(std::numeric_limits<std::size_t>::max()) {};
SourceEnd::SourceEnd(SourceEnd&& o) : pipe(o.pipe), tskip(o.tskip) {};
SourceEnd::SourceEnd(ProfilePipeline& p) : pipe(&p), tskip(std::numeric_limits<std::size_t>::max()) {};
SourceEnd::SourceEnd(ProfilePipeline& p, std::size_t t) : pipe(&p), tskip(t) {};

void SourceEnd::attributes(const ProfileAttributes& as) {
  pipe->attrs.merge(as, [](const std::string& msg){
    std::cerr << "WARNING: " << msg << "\n";
  });
}

Module& SourceEnd::module(const stdshim::filesystem::path& p) {
  auto x = pipe->mods.emplace(pipe->structs.module, p);
  auto r = &x.first();
  if(x.second) {
    for(const auto& s: pipe->sinks) s->notifyModule(*r);
    r->userdata.initialize();
  }
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    if(i != tskip)
      r = &pipe->transformers[i]->module(*r);
  return *r;
}

Function& SourceEnd::function(const Module& m) {
  return pipe->newFunction(m);
}

void SourceEnd::functionComplete(const Module& m, Function& f) {
  for(const auto& s: pipe->sinks) s->notifyFunction(m, f);
  f.userdata.initialize();
}

File& SourceEnd::file(const stdshim::filesystem::path& p) {
  auto x = pipe->files.emplace(pipe->structs.file, p);
  auto r = &x.first();
  if(x.second) {
    for(const auto& s: pipe->sinks) s->notifyFile(*r);
    r->userdata.initialize();
  }
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    if(i != tskip)
      r = &pipe->transformers[i]->file(*r);
  return *r;
}

Metric& SourceEnd::metric(const std::string& n, const std::string& d) {
  auto x = pipe->mets.emplace(pipe->structs.metric, pipe->metstruct, n, d);
  auto r = &x.first();
  if(x.second) {
    for(const auto& s: pipe->sinks) s->notifyMetric(*r);
    r->userdata.initialize();
  }
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    if(i != tskip)
      r = &pipe->transformers[i]->metric(*r);
  return *r;
}

Context& SourceEnd::context(const Scope& s) { return context(*pipe->cct, s); }
Context& SourceEnd::context(Context& p, const Scope& s) {
  Context* rc = &p;
  Scope rs = s;
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    if(i != tskip)
      rc = &pipe->transformers[i]->context(*rc, rs);
  auto x = rc->ensure(rs);
  if(x.second) {
    for(const auto& s: pipe->sinks) s->notifyContext(x.first);
    x.first.userdata.initialize();
  }
  return x.first;
}

void SourceEnd::add(Context& c, const Thread& t, const Metric& m, double v) {
  for(const auto& s: pipe->sinks)
    s->notifyContextData(t, c, m, v);
  pipe->ctxAdd(c, m, v);
}

void SourceEnd::add(Context& c, const Metric& m, double v) {
  pipe->ctxAdd(c, m, v);
}

Thread& SourceEnd::thread(const ThreadAttributes& o) {
  std::pair<unsigned long, unsigned long> key(o.threadid(), o.mpirank());
  auto x = pipe->threads.emplace(new Thread(pipe->structs.thread, o));
  if(x.second) {
    for(const auto& s: pipe->sinks) s->notifyThread(*x.first);
    x.first->userdata.initialize();
  } else {
    std::cerr << "WARNING: Merging indistiguishable threads: ("
              << o.threadid() << "," << o.mpirank() << ")!\n";
    x.first->attributes.merge(o, [](const std::string& msg){
      std::cerr << "WARNING: " << msg << "\n";
    });
  }
  return *x.first;
}

SourceEnd::Trace SourceEnd::traceStart(Thread& t, std::chrono::nanoseconds epoch) {
  auto id = pipe->threadId(t);
  for(const auto& s: pipe->sinks)
    s->traceStart(t, id, epoch);
  return Trace(*pipe, t, id);
}

void SourceEnd::Trace::trace(std::chrono::nanoseconds tm, const Context& c) {
  if(!*this) throw std::logic_error("Attempt to emit traces into an empty Trace!");
  for(const auto& s: pipe->sinks)
    s->trace(*td, id, tm, c);
}

void SourceEnd::Trace::reset() {
  if(!*this) return;
  for(const auto& s: pipe->sinks)
    s->traceEnd(*td, id);
}

SourceEnd& SourceEnd::operator=(SourceEnd&& o) {
  if(pipe != nullptr) throw std::logic_error("Attempt to rebind a SourceEnd!");
  pipe = o.pipe;
  tskip = o.tskip;
  return *this;
}

SinkEnd::SinkEnd() : pipe(nullptr), idx(0) {};
SinkEnd::SinkEnd(ProfilePipeline& p, std::size_t i) : pipe(&p), idx(i) {};

SinkEnd& SinkEnd::operator=(SinkEnd&& o) {
  if(pipe != nullptr) throw std::logic_error("Attempt to rebind a SinkEnd!");
  pipe = o.pipe;
  idx = o.idx;
  return *this;
}
