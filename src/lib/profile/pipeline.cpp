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

detail::ProfilePipelineBase::SourceEntry::SourceEntry(ProfileSource& s)
  : source(s), up_source(nullptr) {};
detail::ProfilePipelineBase::SourceEntry::SourceEntry(std::unique_ptr<ProfileSource>&& up)
  : source(*up), up_source(std::move(up)) {};
detail::ProfilePipelineBase::SourceEntry::SourceEntry(SourceEntry&& o)
  : source(std::move(o.source)), up_source(std::move(o.up_source)) {};

Settings& Settings::operator<<(ProfileSource& s) {
  sources.emplace_back(s);
  return *this;
}
Settings& Settings::operator<<(std::unique_ptr<ProfileSource>&& sp) {
  if(!sp) return *this;
  sources.emplace_back(std::move(sp));
  return *this;
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
  sinks.emplace_back(acc, acc & wav, req, s);
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
  if(pro.hasMScopeIdentifiers()) finalizers.mscopeIdentifiers.emplace_back(f);
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
  : detail::ProfilePipelineBase(std::move(b)), team_size(team_sz),
    waves(sources.size()),
    sourcePrewaveRegionDepChain(std::numeric_limits<std::size_t>::max()),
    sinkWavefrontDepChain(std::numeric_limits<std::size_t>::max()),
    sourcePostwaveRegionDepChain(std::numeric_limits<std::size_t>::max()),
    sinkWriteDepChain(std::numeric_limits<std::size_t>::max()),
    depChainComplete(false), sourceLocals(sources.size()), cct(nullptr) {
  using namespace literals::data;
  // Prep the Extensions first thing.
  if(requested.hasClassification()) {
    uds.classification = structs.module.add_initializer<Classification>(
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
    uds.identifier.metric = structs.metric.add_default<unsigned int>(
      [this](unsigned int& id, const Metric& m){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.metric(m, id);
      });
    uds.identifier.thread = structs.thread.add_default<unsigned int>(
      [this](unsigned int& id, const Thread& t){
        for(ProfileFinalizer& fp: finalizers.identifier) fp.thread(t, id);
      });
  }
  if(requested.hasMScopeIdentifiers()) {
    uds.mscopeIdentifiers.metric = structs.metric.add_default<Metric::ScopedIdentifiers>(
      [this](Metric::ScopedIdentifiers& ids, const Metric& m){
        for(ProfileFinalizer& fp: finalizers.mscopeIdentifiers) fp.metric(m, ids);
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
    if(!(attributes + references + contexts + DataClass::threads).allOf(s.waveLimit))
      util::log::fatal() << "Early wavefronts for non-global data currently not supported!";
  }
  structs.file.freeze();
  structs.context.freeze();
  structs.module.freeze();
  structs.metric.freeze();
  structs.thread.freeze();
  depChainComplete = true;

  // Make sure the Finalizers and Transformers are ready before anything enters.
  // Unlike Sources, we can bind these without worry of anything happening.
  for(ProfileFinalizer& f: finalizers.all)
    f.bindPipeline(Source(*this, DataClass::all(), ExtensionClass::all()));
  for(std::size_t i = 0; i < transformers.size(); i++)
    transformers[i].get().bindPipeline(Source(*this, DataClass::all(), ExtensionClass::all(), i));

  // Make sure the global Context is ready before letting any data in.
  cct.reset(new Context(structs.context, Scope(*this)));
  for(auto& s: sinks) {
    if(s.dataLimit.hasContexts()) s().notifyContext(*cct);
  }
  cct->userdata.initialize();

  // Now we can connect the input without losing any information.
  std::size_t idx = 0;
  for(auto& ms: sources) {
    ms.dataLimit = ms().provides();
    sourceLocals[idx].orderedRegions = ms().requiresOrderedRegions();
    if(sourceLocals[idx].orderedRegions.first) {
      sourceLocals[idx].priorPrewaveRegionDep = sourcePrewaveRegionDepChain;
      sourcePrewaveRegionDepChain = idx;
    }
    if(sourceLocals[idx].orderedRegions.second) {
      sourceLocals[idx].priorPostwaveRegionDep = sourcePostwaveRegionDepChain;
      sourcePostwaveRegionDepChain = idx;
    }
    ms().bindPipeline(Source(*this, ms.dataLimit, ExtensionClass::all(), sourceLocals[idx]));
    scheduled |= ms.dataLimit;
    idx++;
  }
  scheduled &= all_requested;
  unscheduledWaves = scheduledWaves - scheduled;
  scheduledWaves &= scheduled;
}

void ProfilePipeline::run() {
#if ENABLE_VG_ANNOTATIONS == 1
  char start_arc;
  char barrier_arc;
  char single_arc;
  char for_arc;
  char end_arc;
#endif

  std::array<std::atomic<std::size_t>, 4> countdowns;
  for(auto& c: countdowns) c.store(sources.size(), std::memory_order_relaxed);

  std::mutex delayedThreads_lock;
  std::vector<Thread::Temporary> delayedThreads;

  ANNOTATE_HAPPENS_BEFORE(&start_arc);
  #pragma omp parallel num_threads(team_size)
  {
    ANNOTATE_HAPPENS_AFTER(&start_arc);

    // Function to notify a Sink for this wavefront, potentially recursing if needed.
    auto notify = [&](SinkEntry& e, DataClass newwaves) {
      // Update this Sink's view of the current wave status, check if we care.
      DataClass allwaves;
      {
        std::unique_lock<std::mutex> l(e.wavefrontStatusLock);
        e.wavefrontState |= newwaves;

        // Skip if we already delivered the current waveset
        if(e.wavefrontDelivered.allOf(e.wavefrontState & e.waveLimit)) return;

        // If we haven't hit the dependency delay yet, skip until later
        if(!e.wavefrontState.allOf(e.wavefrontPriorDelay & scheduledWaves))
          return;

        // We intend to deliver all the waves that have passed so far.
        allwaves = e.wavefrontDelivered |= e.wavefrontState & e.waveLimit;
      }

      // Deliver a notification, potentially out of order
      e().notifyWavefront(allwaves);
    };

    // First issue a wavefront with just the unscheduled waves
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < sinks.size(); ++i)
      notify(sinks[i], unscheduledWaves);

    // Unblock the finishing wave for any Sources that don't have waves.
    // Also handle cases that require an initial pre-wavefront ordering
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < sources.size(); ++i) {
      if(!(scheduledWaves & sources[i].dataLimit).hasAny()) {
        sources[i].wavesComplete.signal();
      }
      auto& sl = sourceLocals[i];
      if(sl.orderedRegions.first) {
        std::unique_lock<std::mutex> l(sources[i].lock);
        sl.orderedPrewaveRegionUnlocked = true;
        sources[i]().read({});
        sl.orderedPrewaveRegionUnlocked = false;
      }
    }

    // The rest of the waves have the same general format
    auto wave = [&](DataClass d, std::size_t idx) {
      if(!(d & scheduledWaves).hasAny()) return;
      #pragma omp for schedule(dynamic) nowait
      for(std::size_t i = 0; i < sources.size(); ++i) {
        {
          std::unique_lock<std::mutex> l(sources[i].lock);
          DataClass req = (sources[i]().finalizeRequest(d) - sources[i].read)
                          & sources[i].dataLimit;
          sources[i].read |= req;
          if(req.hasAny()) {
            sources[i]().read(req);
            // If there are (as of now) no more available waves for this source,
            // emit a signal to unblock the finishing wave
            if(sources[i].read.allOf(scheduledWaves & sources[i].dataLimit))
              sources[i].wavesComplete.signal();
          }
        }
        if(countdowns[idx].fetch_sub(1, std::memory_order_acq_rel)-1 == 0) {
          for(SinkEntry& e: sinks) notify(e, d);
        }
      }
    };
    wave(DataClass::attributes, 0);
    wave(DataClass::references, 1);
    wave(DataClass::threads, 2);
    wave(DataClass::contexts, 3);

    // Now for the finishing wave
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < sources.size(); ++i) {
      auto& sl = sourceLocals[i];
      {
        sources[i].wavesComplete.wait();
        std::unique_lock<std::mutex> l(sources[i].lock);
        sl.orderedPostwaveRegionUnlocked = true;
        DataClass req = (sources[i]().finalizeRequest(scheduled - scheduledWaves)
                         - sources[i].read) & sources[i].dataLimit;
        sources[i].read |= req;
        if(req.hasAny()) sources[i]().read(req);
      }

      // Finalize all the Threads we can.
      for(auto& t: sl.threads) {
        Metric::prefinalize(t);
        if(Metric::needsCrossfinalize(t)) {
          std::unique_lock<std::mutex> l(delayedThreads_lock);
          delayedThreads.emplace_back(std::move(t));
          continue;
        }
        Metric::finalize(t);
        for(auto& s: sinks)
          if(s.dataLimit.hasThreads()) s().notifyThreadFinal(t);
      }

      // Clean up the Source-local data.
      sl.threads.clear();
      if(!sl.thawedMetrics.empty())
        util::log::fatal{} << "Source exited without freezing all its Metrics!";
      sl.thawedMetrics.clear();
    }

    // Make sure everything has been read before we cross-distribute
    ANNOTATE_HAPPENS_BEFORE(&barrier_arc);
    #pragma omp barrier
    ANNOTATE_HAPPENS_AFTER(&barrier_arc);

    // One thread works hard to do the cross-Thread distributions
    #pragma omp single
    {
      Metric::crossfinalize(delayedThreads);
      ANNOTATE_HAPPENS_BEFORE(&single_arc);
    }
    ANNOTATE_HAPPENS_AFTER(&single_arc);

    // Once that's done, everybody works together to finish the job
    #pragma omp for schedule(dynamic)
    for(std::size_t i = 0; i < delayedThreads.size(); ++i) {
      auto t = std::move(delayedThreads[i]);
      Metric::finalize(t);
      for(auto& s: sinks)
        if(s.dataLimit.hasThreads()) s().notifyThreadFinal(t);
      ANNOTATE_HAPPENS_BEFORE(&for_arc);
    }
    ANNOTATE_HAPPENS_AFTER(&for_arc);

    // Clean up the Sources early, to save some serialized time later
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < sources.size(); ++i)
      sources[i].up_source.reset();

    // Let the Sinks finish up their writing
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t idx = 0; idx < sinks.size(); ++idx)
      sinks[idx]().write();

    // We don't have any work to do, so attempt to assist the others.
    std::forward_list<std::reference_wrapper<SinkEntry>> workingSinks(sinks.begin(), sinks.end());
    bool didwork = true;
    do {
      // If we didn't do any work in the last iteration, we may contend for
      // resources if we try to poll again. So we yield this thread to whoever.
      if(!didwork) std::this_thread::yield();
      didwork = false;

      auto before_it = workingSinks.before_begin();
      auto it = workingSinks.begin();
      while(it != workingSinks.end()) {
        auto result = (*it)().help();
        didwork = didwork || result.contributed;
        if(result.completed) {
          it = workingSinks.erase_after(before_it);
        } else {
          ++before_it;
          ++it;
        }
      }
    } while(!workingSinks.empty());

    ANNOTATE_HAPPENS_BEFORE(&end_arc);
  }
  ANNOTATE_HAPPENS_AFTER(&end_arc);
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
const decltype(ProfilePipeline::Extensions::mscopeIdentifiers)&
Source::mscopeIdentifiers() const {
  if(!extensionLimit.hasMScopeIdentifiers())
    util::log::fatal() << "Source did not register for `mscopeIdentifiers` emission!";
  return pipe->uds.mscopeIdentifiers;
}
const decltype(ProfilePipeline::Extensions::resolvedPath)&
Source::resolvedPath() const {
  if(!extensionLimit.hasResolvedPath())
    util::log::fatal() << "Source did not register for `resolvedPath` emission!";
  return pipe->uds.resolvedPath;
}

util::Once::Caller Source::enterOrderedPrewaveRegion() {
  if(!slocal->orderedRegions.first)
    util::log::fatal{} << "Source attempted to enter a prewave ordered region without registration!";
  if(!slocal->orderedPrewaveRegionUnlocked)
    util::log::fatal{} << "Source attempted to enter a prewave ordered region prior to wavefront completion!";
  if(slocal->priorPrewaveRegionDep != std::numeric_limits<std::size_t>::max())
    pipe->sourceLocals[slocal->priorPrewaveRegionDep].orderedPrewaveRegionDepOnce.wait();
  return slocal->orderedPrewaveRegionDepOnce.signal();
}
util::Once::Caller Source::enterOrderedPostwaveRegion() {
  if(!slocal->orderedRegions.second)
    util::log::fatal{} << "Source attempted to enter a postwave ordered region without registration!";
  if(!slocal->orderedPostwaveRegionUnlocked)
    util::log::fatal{} << "Source attempted to enter a postwave ordered region prior to wavefront completion!";
  if(slocal->priorPostwaveRegionDep != std::numeric_limits<std::size_t>::max())
    pipe->sourceLocals[slocal->priorPostwaveRegionDep].orderedPostwaveRegionDepOnce.wait();
  else if(pipe->sinkWavefrontDepChain != std::numeric_limits<std::size_t>::max())
    pipe->sinks[pipe->sinkWavefrontDepChain].wavefrontDepOnce.wait();
  else if(pipe->sourcePrewaveRegionDepChain != std::numeric_limits<std::size_t>::max())
    pipe->sourceLocals[pipe->sourcePrewaveRegionDepChain].orderedPrewaveRegionDepOnce.wait();
  return slocal->orderedPostwaveRegionDepOnce.signal();
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
  return *r;
}

Metric& Source::metric(Metric::Settings s) {
  if(!limit().hasAttributes())
    util::log::fatal() << "Source did not register for `attributes` emission!";
  auto x = pipe->mets.emplace(pipe->structs.metric, std::move(s));
  slocal->thawedMetrics.insert(&x.first());
  for(ProfileTransformer& t: pipe->transformers)
    t.metric(x.first(), x.first().statsAccess());
  return x.first();
}

ExtraStatistic& Source::extraStatistic(ExtraStatistic::Settings s) {
  if(!limit().hasAttributes())
    util::log::fatal() << "Source did not register for `attributes` emission!";
  auto x = pipe->estats.emplace(std::move(s));
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasAttributes()) s().notifyExtraStatistic(x.first());
    }
  }
  return x.first();
}

void Source::metricFreeze(Metric& m) {
  if(m.freeze()) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasAttributes()) s().notifyMetric(m);
    }
    m.userdata.initialize();
  }
  slocal->thawedMetrics.erase(&m);
}

Context& Source::global() { return *pipe->cct; }
void Source::notifyContext(Context& c) {
  for(auto& s: pipe->sinks) {
    if(s.dataLimit.hasContexts()) s().notifyContext(c);
  }
  c.userdata.initialize();
}
ContextRef Source::context(ContextRef p, const Scope& s, bool recurse) {
  if(!limit().hasContexts())
    util::log::fatal() << "Source did not register for `contexts` emission!";
  ContextRef res = p;
  Scope rs = s;
  for(std::size_t i = 0; i < pipe->transformers.size(); i++)
    if(recurse || i != tskip)
      res = pipe->transformers[i].get().context(res, rs);

  auto newCtx = [&](ContextRef p, const Scope& s) -> ContextRef {
    if(auto pc = std::get_if<Context>(p)) {
      if(p.collaboration()) {
        auto& collab = *p.collaboration();
        return {collab, collab.ensure(*pc, rs, [this](Context& c) {
          notifyContext(c);
        })};
      }
      auto x = pc->ensure(rs);
      if(x.second) notifyContext(x.first);
      return x.first;
    }
    util::log::fatal{} << "Attempt to create a Context from an improper Context!";
    abort();  // unreachable
  };

  if(auto rc = std::get_if<Context>(res)) {
    res = newCtx(res, rs);
  } else if(auto rc = std::get_if<SuperpositionedContext>(res)) {
    for(auto& t: rc->m_targets) t.target = newCtx(t.target, rs);
  } else abort();  // unreachable

  if(tskip >= pipe->transformers.size())
    for(auto& ss: pipe->sinks)
      ss().notifyContextExpansion(p, s, res);
  return res;
}

ContextRef Source::superposContext(ContextRef root, std::vector<SuperpositionedContext::Target> targets) {
  if(!limit().hasContexts())
    util::log::fatal() << "Source did not register for `contexts` emission!";
  if(!std::holds_alternative<Context>(root))
    util::log::fatal{} << "Attempt to root a Superposition on an improper Context!";
  if(root.collaboration())
    util::log::fatal{} << "TODO: Support superpositions off of collaborations";
  return std::get<Context>(root).superposition(std::move(targets));
}

CollaborativeContext& Source::collabContext(std::uint64_t id) {
  if(!limit().hasContexts())
    util::log::fatal() << "Source did not register for `contexts` emission!";
  return pipe->collabs[id].ctx;
}
ContextRef Source::collaborate(ContextRef target, CollaborativeContext& collab) {
  if(!limit().hasContexts())
    util::log::fatal() << "Source did not register for `contexts` emission!";
  collab.addCollaboratorRoot(target, [this](Context& c){ notifyContext(c); });
  return {collab, target};
}

Source::AccumulatorsRef Source::accumulateTo(ContextRef c, Thread::Temporary& t) {
  if(!limit().hasMetrics())
    util::log::fatal() << "Source did not register for `metrics` emission!";
  if(auto collab = c.collaboration()) {
    auto& data = t.cb_data[*collab];
    if(auto pc = std::get_if<Context>(c))
      return data[*pc];
    else abort();  // unreachable
  } else if(auto pc = std::get_if<Context>(c))
    return t.data[*pc];
  else if(auto pc = std::get_if<SuperpositionedContext>(c))
    return t.sp_data[*pc];
  else abort();  // unreachable
}

void Source::AccumulatorsRef::add(Metric& m, double v) {
  map[m].add(v);
}

Source::StatisticsRef Source::accumulateTo(ContextRef c) {
  if(!limit().hasMetrics())
    util::log::fatal() << "Source did not register for `metrics` emission!";
  if(!std::holds_alternative<Context>(c))
    util::log::fatal{} << "Statistics are only present on proper Contexts!";
  return {c};
}

template<class T>
void Source::StatisticsRef::add(T& ctx, Metric& m, const StatisticPartial& sp,
                                MetricScope ms, double v) {
  auto& a = ctx.data.emplace(std::piecewise_construct,
                             std::forward_as_tuple(m),
                             std::forward_as_tuple(m)).first;
  a.get(sp).add(ms, v);
}
void Source::StatisticsRef::add(Metric& m, const StatisticPartial& sp,
                                MetricScope ms, double v) {
  if(auto pc = std::get_if<Context>(c)) add(*pc, m, sp, ms, v);
  else abort();  // unreachable
}

Thread::Temporary& Source::thread(const ThreadAttributes& o) {
  if(!limit().hasThreads())
    util::log::fatal() << "Source did not register for `threads` emission!";
  auto& t = *pipe->threads.emplace(new Thread(pipe->structs.thread, o)).first;
  for(auto& s: pipe->sinks) {
    if(s.dataLimit.hasThreads()) s().notifyThread(t);
  }
  t.userdata.initialize();
  slocal->threads.emplace_back(Thread::Temporary(t));
  return slocal->threads.back();
}

void Source::timepoint(Thread::Temporary& tt, ContextRef c, std::chrono::nanoseconds tm) {
  if(!limit().hasTimepoints())
    util::log::fatal() << "Source did not register for `timepoints` emission!";
  for(auto& s: pipe->sinks) {
    if(!s.dataLimit.hasTimepoints()) continue;
    if(s.dataLimit.allOf(DataClass::threads + DataClass::contexts))
      s().notifyTimepoint(tt.thread(), c, tm);
    else if(s.dataLimit.hasContexts())
      s().notifyTimepoint(c, tm);
    else if(s.dataLimit.hasThreads())
      s().notifyTimepoint(tt.thread(), tm);
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
      s().notifyTimepoint(tt.thread(), tm);
    else
      s().notifyTimepoint(tm);
  }
}

void Source::timepoint(ContextRef c, std::chrono::nanoseconds tm) {
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
  : pipe(&p), dataLimit(d), extensionLimit(e), idx(i), orderedWavefront(false),
    priorWavefrontDepOnce(std::numeric_limits<std::size_t>::max()),
    orderedWrite(false),
    priorWriteDepOnce(std::numeric_limits<std::size_t>::max()) {};

void Sink::registerOrderedWavefront() {
  if(pipe->depChainComplete)
    util::log::fatal{} << "Attempt to register a Sink for an ordered wavefront chain after notifyPipeline!";
  if(!orderedWavefront) {
    orderedWavefront = true;
    priorWavefrontDepOnce = pipe->sinkWavefrontDepChain;
    pipe->sinks[idx].wavefrontPriorDelay = pipe->sinkWavefrontDepClasses;
    pipe->sinkWavefrontDepClasses |= pipe->sinks[idx].waveLimit;
    pipe->sinkWavefrontDepChain = idx;
  }
}
void Sink::registerOrderedWrite() {
  if(pipe->depChainComplete)
    util::log::fatal{} << "Attempt to register a Sink for an ordered write chain after notifyPipeline!";
  if(!orderedWrite) {
    orderedWrite = true;
    priorWriteDepOnce = pipe->sinkWriteDepChain;
    pipe->sinkWriteDepChain = idx;
  }
}

util::Once::Caller Sink::enterOrderedWavefront() {
  if(!orderedWavefront)
    util::log::fatal{} << "Attempt to enter an ordered wavefront region without registering!";
  if(priorWavefrontDepOnce != std::numeric_limits<std::size_t>::max())
    pipe->sinks[priorWavefrontDepOnce].wavefrontDepOnce.wait();
  else if(pipe->sourcePrewaveRegionDepChain != std::numeric_limits<std::size_t>::max())
    pipe->sourceLocals[pipe->sourcePrewaveRegionDepChain].orderedPrewaveRegionDepOnce.wait();
  return pipe->sinks[idx].wavefrontDepOnce.signal();
}
util::Once::Caller Sink::enterOrderedWrite() {
  if(!orderedWrite)
    util::log::fatal{} << "Attempt to enter an ordered write region without registering!";
  if(priorWriteDepOnce != std::numeric_limits<std::size_t>::max())
    pipe->sinks[priorWriteDepOnce].writeDepOnce.wait();
  return pipe->sinks[idx].writeDepOnce.signal();
}

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
const decltype(ProfilePipeline::Extensions::mscopeIdentifiers)&
Sink::mscopeIdentifiers() const {
  if(!extensionLimit.hasMScopeIdentifiers())
    util::log::fatal() << "Sink did not register for `mscopeIdentifiers` absorption!";
  return pipe->uds.mscopeIdentifiers;
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

const util::locked_unordered_uniqued_set<ExtraStatistic>& Sink::extraStatistics() {
  if(!dataLimit.hasAttributes())
    util::log::fatal() << "Sink did not register for `attributes` absorption!";
  return pipe->estats;
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
  orderedWavefront = o.orderedWavefront;
  priorWavefrontDepOnce = o.priorWavefrontDepOnce;
  orderedWrite = o.orderedWrite;
  priorWriteDepOnce = o.priorWriteDepOnce;
  return *this;
}
