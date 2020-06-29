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

#include "util/vgannotations.hpp"

#include "pipeline.hpp"

#include "util/log.hpp"
#include "source.hpp"
#include "sink.hpp"
#include "transformer.hpp"
#include "finalizer.hpp"

#include <iomanip>
#include <stdexcept>
#include <limits>

using namespace hpctoolkit;
using Settings = ProfilePipeline::Settings;
using Source = ProfilePipeline::Source;
using Sink = ProfilePipeline::Sink;

const ProfilePipeline::timeout_t ProfilePipeline::timeout_forever;

Settings& Settings::operator<<(ProfileSource& s) {
  sources.emplace_back(s);
  return *this;
}
Settings& Settings::operator<<(std::unique_ptr<ProfileSource>&& sp) {
  if(!sp) return *this;
  up_sources.emplace_back(std::move(sp));
  return operator<<(*up_sources.back());
}

Settings& Settings::operator<<(ProfileSink& s) {
  auto req = s.requires();
  requested += req;
  if((req - available).hasAny())
    util::log::fatal() << "Sink requires unavailable extended data!";
  auto wav = s.wavefronts();
  auto acc = s.accepts();
  if(acc.hasMetrics()) acc += DataClass::attributes + DataClass::threads;
  if(acc.hasContexts()) acc += DataClass::references;
  sinks.push_back({acc, acc & wav, req, std::ref(s)});
  return *this;
}
Settings& Settings::operator<<(std::unique_ptr<ProfileSink>&& sp) {
  if(!sp) return *this;
  up_sinks.emplace_back(std::move(sp));
  return operator<<(*up_sinks.back());
}

Settings& Settings::operator<<(ProfileFinalizer& f) {
  auto pro = f.provides();
  auto req = f.requires();
  if((req - available).hasAny())
    util::log::fatal() << "Finalizer requires unavailable extended data!";
  available += pro;
  finalizers.all.emplace_back(f);
  if(pro.hasClassification()) finalizers.classification.emplace_back(f);
  if(pro.hasIdentifier()) finalizers.identifier.emplace_back(f);
  if(pro.hasResolvedPath()) finalizers.resolvedPath.emplace_back(f);
  return *this;
}
Settings& Settings::operator<<(std::unique_ptr<ProfileFinalizer>&& fp) {
  if(!fp) return *this;
  up_finalizers.emplace_back(std::move(fp));
  return operator<<(*up_finalizers.back());
}

Settings& Settings::operator<<(ProfileTransformer& t) {
  transformers.emplace_back(t);
  return *this;
}
Settings& Settings::operator<<(std::unique_ptr<ProfileTransformer>&& tp) {
  if(!tp) return *this;
  up_transformers.emplace_back(std::move(tp));
  return operator<<(*up_transformers.back());
}

ProfilePipeline::ProfilePipeline(Settings&& b, std::size_t team_sz)
  : detail::ProfilePipelineBase(std::move(b)),
    team_size(team_sz), waves(sources.size()), sourceLocals(sources.size()),
    cct(nullptr) {
  using namespace literals::data;
  // Prep the Extensions first thing.
  if(requested.hasClassification()) {
    uds.classification = structs.module.add_default<Classification>(
      [this](Classification& cl, const Module& m){
        for(ProfileFinalizer& fp: finalizers.classification) fp.module(m, cl);
      });
  }
  if(requested.hasIdentifier()) {
    uds.identifier.file = structs.file.add_default<unsigned int>(
      [this](unsigned int& id, const File& f){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.file(f, id);
      });
    uds.identifier.context = structs.context.add_default<unsigned int>(
      [this](unsigned int& id, const Context& c){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.context(c, id);
      });
    uds.identifier.module = structs.module.add_default<unsigned int>(
      [this](unsigned int& id, const Module& m){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.module(m, id);
      });
    uds.identifier.metric = structs.metric.add_default<std::pair<unsigned int, unsigned int>>(
      [this](std::pair<unsigned int, unsigned int>& ids, const Metric& m){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.metric(m, ids);
      });
    uds.identifier.thread = structs.thread.add_default<unsigned int>(
      [this](unsigned int& id, const Thread& t){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.thread(t, id);
      });
  }
  if(requested.hasResolvedPath()) {
    uds.resolvedPath.file = structs.file.add_default<stdshim::filesystem::path>(
      [this](stdshim::filesystem::path& sp, const File& f){
        for(ProfileFinalizer& fp: finalizers.resolvedPath) fp.file(f, sp);
      });
    uds.resolvedPath.module = structs.module.add_default<stdshim::filesystem::path>(
      [this](stdshim::filesystem::path& sp, const Module& m){
        for(ProfileFinalizer& fp: finalizers.resolvedPath) fp.module(m, sp);
      });
  }

  // Output is prepped first, in case the input is a little early.
  DataClass all_requested;
  for(std::size_t i = 0; i < sinks.size(); i++) {
    auto& s = sinks[i];
    s().bindPipeline(Sink(*this, s.dataLimit, s.extensionLimit, i));
    all_requested |= s.dataLimit;
    scheduledWaves |= s.waveLimit;
    if(!(attributes + references + contexts).allOf(s.waveLimit))
      util::log::fatal() << "Early wavefronts for non-global data currently not supported!";
    if(s.waveLimit.hasAttributes()) sinkwaves.attributes.emplace_back(s());
    if(s.waveLimit.hasReferences()) sinkwaves.references.emplace_back(s());
    if(s.waveLimit.hasContexts()) sinkwaves.contexts.emplace_back(s());
  }
  structs.file.freeze();
  structs.context.freeze();
  structs.module.freeze();
  structs.metric.freeze();
  structs.thread.freeze();

  // Make sure the Finalizers and Transformers are ready before anything enters.
  // Unlike Sources, we can bind these without worry of anything happening.
  for(ProfileFinalizer& f: finalizers.all)
    f.bindPipeline(Source(*this, DataClass::all(), ExtensionClass::all()));
  for(std::size_t i = 0; i < transformers.size(); i++)
    transformers[i].get().bindPipeline(Source(*this, DataClass::all(), ExtensionClass::all(), i));

  // Make sure the global Context is ready before letting any data in.
  cct.reset(new Context(structs.context, metstruct, Scope(*this)));
  for(auto& s: sinks) {
    if(s.dataLimit.hasContexts()) s().notifyContext(*cct);
  }
  cct->userdata.initialize();

  // Now we can connect the input without losing any information.
  std::size_t idx = 0;
  for(ProfileSource& ms: sources) {
    auto data = ms.provides();
    ms.bindPipeline(Source(*this, data, ExtensionClass::all(), sourceLocals[idx]));
    scheduled |= data;
    idx++;
  }
  scheduled &= all_requested;
  scheduledWaves &= scheduled;
}

void ProfilePipeline::run() {
#if ENABLE_VG_ANNOTATIONS == 1
  char start_arc;
  char barrier_arc_1;
  char barrier_arc_2;
  char barrier_arc_3;
  char barrier_arc;
  char end_arc;
#endif
  ANNOTATE_HAPPENS_BEFORE(&start_arc);
  #pragma omp parallel num_threads(team_size)
  {
    ANNOTATE_HAPPENS_AFTER(&start_arc);

    // First handle the early wavefronts.
    // TODO: Make this more of a task-loop structure
    if(scheduledWaves.hasAttributes()) {
      #pragma omp for schedule(dynamic)
      for(std::size_t idx = 0; idx < sources.size(); ++idx) {
        sources[idx].get().read(DataClass::attributes);
        ANNOTATE_HAPPENS_BEFORE(&barrier_arc_1);
      }
      ANNOTATE_HAPPENS_AFTER(&barrier_arc_1);
      #pragma omp for schedule(dynamic) nowait
      for(std::size_t idx = 0; idx < sinks.size(); ++idx)
        sinks[idx]().notifyWavefront(DataClass::attributes);
    }
    if(scheduledWaves.hasReferences()) {
      #pragma omp for schedule(dynamic)
      for(std::size_t idx = 0; idx < sources.size(); ++idx) {
        sources[idx].get().read(DataClass::references);
        ANNOTATE_HAPPENS_BEFORE(&barrier_arc_2);
      }
      ANNOTATE_HAPPENS_AFTER(&barrier_arc_2);
      #pragma omp for schedule(dynamic) nowait
      for(std::size_t idx = 0; idx < sinks.size(); ++idx)
        sinks[idx]().notifyWavefront(DataClass::references);
    }
    if(scheduledWaves.hasContexts()) {
      #pragma omp for schedule(dynamic)
      for(std::size_t idx = 0; idx < sources.size(); ++idx) {
        sources[idx].get().read(DataClass::contexts);
        ANNOTATE_HAPPENS_BEFORE(&barrier_arc_3);
      }
      ANNOTATE_HAPPENS_AFTER(&barrier_arc_3);
      #pragma omp for schedule(dynamic) nowait
      for(std::size_t idx = 0; idx < sinks.size(); ++idx)
        sinks[idx]().notifyWavefront(DataClass::contexts);
    }

    // Now for the finishing wave
    #pragma omp for schedule(dynamic)
    for(std::size_t idx = 0; idx < sources.size(); ++idx) {
      sources[idx].get().read(scheduled - scheduledWaves);

      auto& sl = sourceLocals[idx];
      // Done first to set the stage for the Sinks to do things.
      for(auto& t: sl.threads)
        for(Metric& m: mets.citerate()) m.finalize(t);
      // Let the Sinks know that the Threads have finished.
      for(auto& s: sinks)
        if(s.dataLimit.hasThreads())
          for(const auto& t: sl.threads) s().notifyThreadFinal(t);
      // Clean up the Source-local data.
      sl.threads.clear();

      ANNOTATE_HAPPENS_BEFORE(&barrier_arc);
    }
    // Implicit Barrier
    ANNOTATE_HAPPENS_AFTER(&barrier_arc);

    // Let the Sinks finish up their writing
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t idx = 0; idx < sinks.size(); ++idx)
      sinks[idx]().write();
    for(std::size_t idx = 0; idx < sinks.size(); ++idx)
      sinks[idx]().help(timeout_forever);

    ANNOTATE_HAPPENS_BEFORE(&end_arc);
  }
  ANNOTATE_HAPPENS_AFTER(&end_arc);
}

bool ProfilePipeline::run(timeout_t to) {
  util::log::fatal() << "Pipeline::run(timeout) to be implemented!";
  return false;
}

Source::Source() : pipe(nullptr), tskip(std::numeric_limits<std::size_t>::max()) {};
Source::Source(ProfilePipeline& p, const DataClass& ds, const ExtensionClass& es)
  : Source(p, ds, es, std::numeric_limits<std::size_t>::max()) {};
Source::Source(ProfilePipeline& p, const DataClass& ds, const ExtensionClass& es, SourceLocal& sl)
  : pipe(&p), slocal(&sl), dataLimit(ds), extensionLimit(es),
    tskip(std::numeric_limits<std::size_t>::max()) {};
Source::Source(ProfilePipeline& p, const DataClass& ds, const ExtensionClass& es, std::size_t t)
  : pipe(&p), slocal(nullptr), dataLimit(ds), extensionLimit(es), tskip(t) {};

const decltype(ProfilePipeline::Extensions::classification)&
Source::classification() const {
  if(!extensionLimit.hasClassification())
    util::log::fatal() << "Source did not register for `classification` emission!";
  return pipe->uds.classification;
}
const decltype(ProfilePipeline::Extensions::identifier)&
Source::identifier() const {
  if(!extensionLimit.hasIdentifier())
    util::log::fatal() << "Source did not register for `identifier` emission!";
  return pipe->uds.identifier;
}
const decltype(ProfilePipeline::Extensions::resolvedPath)&
Source::resolvedPath() const {
  if(!extensionLimit.hasResolvedPath())
    util::log::fatal() << "Source did not register for `resolvedPath` emission!";
  return pipe->uds.resolvedPath;
}

void Source::attributes(const ProfileAttributes& as) {
  if(!limit().hasAttributes())
    util::log::fatal() << "Source did not register for `attributes` emission!";
  std::unique_lock<std::mutex> l(pipe->attrsLock);
  pipe->attrs.merge(as);
}

Module& Source::module(const stdshim::filesystem::path& p) {
  if(!limit().hasReferences())
    util::log::fatal() << "Source did not register for `references` emission!";
  auto x = pipe->mods.emplace(pipe->structs.module, p);
  auto r = &x.first();
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasReferences()) s().notifyModule(*r);
    }
    r->userdata.initialize();
  }
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    try {
      if(i != tskip)
        r = &pipe->transformers[i].get().module(*r);
    } catch(std::exception& e) {
      util::log::fatal() << "Exception caught while processing module " << p
                         << " through transformer " << i << "\n"
                         << "  what(): " << e.what();
    }
  return *r;
}

File& Source::file(const stdshim::filesystem::path& p) {
  if(!limit().hasReferences())
    util::log::fatal() << "Source did not register for `references` emission!";
  auto x = pipe->files.emplace(pipe->structs.file, p);
  auto r = &x.first();
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasReferences()) s().notifyFile(*r);
    }
    r->userdata.initialize();
  }
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    try {
      if(i != tskip)
        r = &pipe->transformers[i].get().file(*r);
    } catch(std::exception& e) {
      util::log::fatal() << "Exception caught while processing file " << p
                         << " through transformer " << i << "\n"
                         << "  what(): " << e.what();
    }
  return *r;
}

Metric& Source::metric(const std::string& n, const std::string& d, Metric::Type t) {
  if(!limit().hasAttributes())
    util::log::fatal() << "Source did not register for `attributes` emission!";
  auto x = pipe->mets.emplace(pipe->structs.metric, pipe->metstruct, pipe->tmetstruct, n, d, t);
  auto r = &x.first();
  if(r->type != t)
    util::log::fatal() << "Identical Metrics with different types!";
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasAttributes()) s().notifyMetric(*r);
    }
    r->userdata.initialize();
  }
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    try {
      if(i != tskip)
        r = &pipe->transformers[i].get().metric(*r);
    } catch(std::exception& e) {
      util::log::fatal() << "Exception caught while processing metric " << n
                         << " through transformer " << i << "\n"
                         << "  what(): " << e.what();
    }
  return *r;
}

Context& Source::context(const Scope& s) { return context(*pipe->cct, s); }
Context& Source::context(Context& p, const Scope& s) {
  if(!limit().hasContexts())
    util::log::fatal() << "Source did not register for `contexts` emission!";
  Context* rc = &p;
  Scope rs = s;
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    try {
      if(i != tskip)
        rc = &pipe->transformers[i].get().context(*rc, rs);
    } catch(std::exception& e) {
      util::log::fatal() << "Exception caught while processing context"
                         << " through transformer " << i << "\n"
                         << "  what(): " << e.what();
    }
  auto x = rc->ensure(rs);
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasContexts()) s().notifyContext(x.first);
    }
    x.first.userdata.initialize();
  }
  return x.first;
}

void Source::add(Context& c, Thread::Temporary& t, const Metric& m, double v) {
  if(!limit().hasMetrics())
    util::log::fatal() << "Source did not register for `metrics` emission!";
  m.add(t, c, v);
}

void Source::add(Context& c, const Metric& m, std::pair<double, double> v) {
  if(!limit().hasMetrics())
    util::log::fatal() << "Source did not register for `metrics` emission!";
  m.add(c, v);
}

void Source::add(Thread::Temporary& t, const Metric& m, double v) {
  if(!limit().hasMetrics())
    util::log::fatal() << "Source did not register for `metrics` emission!";
  m.add(t, *pipe->cct, v);
}

void Source::add(const Metric& m, std::pair<double, double> v) {
  if(!limit().hasMetrics())
    util::log::fatal() << "Source did not register for `metrics` emission!";
  m.add(*pipe->cct, v);
}

Thread::Temporary& Source::thread(const ThreadAttributes& o) {
  if(!limit().hasThreads())
    util::log::fatal() << "Source did not register for `threads` emission!";
  auto& t = *pipe->threads.emplace(new Thread(pipe->structs.thread, o)).first;
  for(auto& s: pipe->sinks) {
    if(s.dataLimit.hasThreads()) s().notifyThread(t);
  }
  t.userdata.initialize();
  slocal->threads.emplace_back(Thread::Temporary(t, pipe->tmetstruct));
  return slocal->threads.back();
}

void Source::timepoint(Thread::Temporary& tt, Context& c, std::chrono::nanoseconds tm) {
  if(!limit().hasTimepoints())
    util::log::fatal() << "Source did not register for `timepoints` emission!";
  for(auto& s: pipe->sinks) {
    if(!s.dataLimit.hasTimepoints()) continue;
    if(s.dataLimit.allOf(DataClass::threads + DataClass::contexts))
      s().notifyTimepoint(tt.thread, c, tm);
    else if(s.dataLimit.hasContexts())
      s().notifyTimepoint(c, tm);
    else if(s.dataLimit.hasThreads())
      s().notifyTimepoint(tt.thread, tm);
    else
      s().notifyTimepoint(tm);
  }
}

void Source::timepoint(Thread::Temporary& tt, std::chrono::nanoseconds tm) {
  if(!limit().hasTimepoints())
    util::log::fatal() << "Source did not register for `timepoints` emission!";
  for(auto& s: pipe->sinks) {
    if(!s.dataLimit.hasTimepoints()) continue;
    if(s.dataLimit.hasThreads())
      s().notifyTimepoint(tt.thread, tm);
    else
      s().notifyTimepoint(tm);
  }
}

void Source::timepoint(Context& c, std::chrono::nanoseconds tm) {
  if(!limit().hasTimepoints())
    util::log::fatal() << "Source did not register for `timepoints` emission!";
  for(auto& s: pipe->sinks) {
    if(!s.dataLimit.hasTimepoints()) continue;
    if(s.dataLimit.hasContexts())
      s().notifyTimepoint(c, tm);
    else
      s().notifyTimepoint(tm);
  }
}

void Source::timepoint(std::chrono::nanoseconds tm) {
  if(!limit().hasTimepoints())
    util::log::fatal() << "Source did not register for `timepoints` emission!";
  for(auto& s: pipe->sinks) {
    if(!s.dataLimit.hasTimepoints()) continue;
    s().notifyTimepoint(tm);
  }
}

Source& Source::operator=(Source&& o) {
  if(pipe != nullptr) util::log::fatal() << "Attempt to rebind a Source!";
  pipe = o.pipe;
  slocal = o.slocal;
  dataLimit = o.dataLimit;
  extensionLimit = o.extensionLimit;
  tskip = o.tskip;
  return *this;
}

Sink::Sink() : pipe(nullptr), idx(0) {};
Sink::Sink(ProfilePipeline& p, const DataClass& d, const ExtensionClass& e, std::size_t i)
  : pipe(&p), dataLimit(d), extensionLimit(e), idx(i) {};

const decltype(ProfilePipeline::Extensions::classification)&
Sink::classification() const {
  if(!extensionLimit.hasClassification())
    util::log::fatal() << "Sink did not register for `classification` absorption!";
  return pipe->uds.classification;
}
const decltype(ProfilePipeline::Extensions::identifier)&
Sink::identifier() const {
  if(!extensionLimit.hasIdentifier())
    util::log::fatal() << "Sink did not register for `identifier` absorption!";
  return pipe->uds.identifier;
}
const decltype(ProfilePipeline::Extensions::resolvedPath)&
Sink::resolvedPath() const {
  if(!extensionLimit.hasResolvedPath())
    util::log::fatal() << "Sink did not register for `resolvedPath` absorption!";
  return pipe->uds.resolvedPath;
}

const ProfileAttributes& Sink::attributes() {
  if(!dataLimit.hasAttributes())
    util::log::fatal() << "Sink did not register for `attributes` absorption!";
  return pipe->attrs;
}

const util::locked_unordered_uniqued_set<Module>& Sink::modules() {
  if(!dataLimit.hasReferences())
    util::log::fatal() << "Sink did not register for `references` absorption!";
  return pipe->mods;
}

const util::locked_unordered_uniqued_set<File>& Sink::files() {
  if(!dataLimit.hasReferences())
    util::log::fatal() << "Sink did not register for `references` absorption!";
  return pipe->files;
}

const util::locked_unordered_uniqued_set<Metric>& Sink::metrics() {
  if(!dataLimit.hasAttributes())
    util::log::fatal() << "Sink did not register for `attributes` absorption!";
  return pipe->mets;
}

const Context& Sink::contexts() {
  if(!dataLimit.hasContexts())
    util::log::fatal() << "Sink did not register for `contexts` absorption!";
  return *pipe->cct;
}

const util::locked_unordered_set<std::unique_ptr<Thread>>& Sink::threads() {
  if(!dataLimit.hasThreads())
    util::log::fatal() << "Sink did not register for `threads` absorption!";
  return pipe->threads;
}

Sink& Sink::operator=(Sink&& o) {
  if(pipe != nullptr) util::log::fatal() << "Attempt to rebind a Sink!";
  pipe = o.pipe;
  dataLimit = o.dataLimit;
  extensionLimit = o.extensionLimit;
  idx = o.idx;
  return *this;
}
