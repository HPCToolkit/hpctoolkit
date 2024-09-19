// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "util/vgannotations.hpp"

#include "pipeline.hpp"

#include "util/log.hpp"
#include "source.hpp"
#include "sink.hpp"
#include "finalizer.hpp"

#include <iomanip>
#include <stdexcept>
#include <limits>

using namespace hpctoolkit;
using Settings = ProfilePipeline::Settings;
using Source = ProfilePipeline::Source;
using Sink = ProfilePipeline::Sink;

size_t ProfilePipeline::TupleHash::operator()(const std::vector<pms_id_t>& tuple) const noexcept {
  size_t sponge = 0x15;
  for(const auto& e: tuple) {
    sponge ^= h_u16(IDTUPLE_GET_KIND(e.kind)) ^ h_u64(e.physical_index);
    sponge <<= 1;
  }
  return sponge;
}
bool ProfilePipeline::TupleEqual::operator()(const std::vector<pms_id_t>& a, const std::vector<pms_id_t>& b) const noexcept {
  if(a.size() != b.size()) return false;
  for(size_t i = 0; i < a.size(); i++) {
    if(IDTUPLE_GET_KIND(a[i].kind) != IDTUPLE_GET_KIND(b[i].kind)
       || a[i].physical_index != b[i].physical_index)
      return false;
  }
  return true;
}

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
  auto req = s.requirements();
  requested += req;
  assert(req - available == ExtensionClass() && "Sink requires unavailable extended data!");
  auto wav = s.wavefronts();
  auto acc = s.accepts();
  if(acc.hasMetrics()) acc += DataClass::attributes + DataClass::threads;
  if(acc.hasContexts()) acc += DataClass::references;
  if(acc.hasCtxTimepoints()) acc += DataClass::contexts + DataClass::threads;
  if(acc.hasMetricTimepoints()) acc += DataClass::attributes + DataClass::threads;
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
  auto req = f.requirements();
  assert(req - available == ExtensionClass() && "Finalizer requires unavailable extended data!");
  requested += req;
  available += pro;
  finalizers.all.emplace_back(f);
  if(pro.hasIdentifier()) finalizers.identifier.emplace_back(f);
  if(pro.hasResolvedPath()) finalizers.resolvedPath.emplace_back(f);
  if(pro.hasClassification()) finalizers.classification.emplace_back(f);
  if(pro.hasStatistics()) finalizers.statistics.emplace_back(f);
  return *this;
}
Settings& Settings::operator<<(std::unique_ptr<ProfileFinalizer>&& fp) {
  if(!fp) return *this;
  up_finalizers.emplace_back(std::move(fp));
  return operator<<(*up_finalizers.back());
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
  if(requested.hasIdentifier()) {
    uds.identifier.file = structs.file.add_default<unsigned int>(
      [this](unsigned int& id, const File& f){
        id = std::numeric_limits<unsigned int>::max();
        for(ProfileFinalizer& fp: finalizers.identifier) {
          if(auto v = fp.identify(f)) {
            id = *v;
            break;
          }
        }
      });
    uds.identifier.context = structs.context.add_default<unsigned int>(
      [this](unsigned int& id, const Context& c){
        id = std::numeric_limits<unsigned int>::max();
        for(ProfileFinalizer& fp: finalizers.identifier) {
          if(auto v = fp.identify(c)) {
            id = *v;
            break;
          }
        }
      });
    uds.identifier.module = structs.module.add_default<unsigned int>(
      [this](unsigned int& id, const Module& m){
        id = std::numeric_limits<unsigned int>::max();
        for(ProfileFinalizer& fp: finalizers.identifier) {
          if(auto v = fp.identify(m)) {
            id = *v;
            break;
          }
        }
      });
    uds.identifier.metric = structs.metric.add_initializer<Metric::Identifier>(
      [this](Metric::Identifier& id, const Metric& m){
        for(ProfileFinalizer& fp: finalizers.identifier) {
          if(auto v = fp.identify(m)) {
            assert(&v->getMetric() == &m);
            id = std::move(*v);
            break;
          }
        }
      });
    uds.identifier.thread = structs.thread.add_default<unsigned int>(
      [this](unsigned int& id, const Thread& t){
        id = std::numeric_limits<unsigned int>::max();
        for(ProfileFinalizer& fp: finalizers.identifier) {
          if(auto v = fp.identify(t)) {
            id = *v;
            break;
          }
        }
      });
  }
  if(requested.hasResolvedPath()) {
    uds.resolvedPath.file = structs.file.add_default<stdshim::filesystem::path>(
      [this](stdshim::filesystem::path& sp, const File& f){
        for(ProfileFinalizer& fp: finalizers.resolvedPath) {
          if(auto v = fp.resolvePath(f)) {
            assert(v->empty() || v->is_absolute());
            sp = *v;
            return;
          }
        }
        std::error_code ec;
        if(!f.path().empty() && f.path().is_absolute()
           && stdshim::filesystem::exists(f.path(), ec))
          sp = f.path();
      });
    uds.resolvedPath.module = structs.module.add_default<stdshim::filesystem::path>(
      [this](stdshim::filesystem::path& sp, const Module& m){
        for(ProfileFinalizer& fp: finalizers.resolvedPath) {
          if(auto v = fp.resolvePath(m)) {
            assert(v->empty() || v->is_absolute());
            sp = *v;
            return;
          }
        }
        std::error_code ec;
        if(!m.path().empty() && m.path().is_absolute()
           && stdshim::filesystem::exists(m.path(), ec))
          sp = m.path();
      });
  }

  // Output is prepped first, in case the input is a little early.
  DataClass all_requested;
  for(std::size_t i = 0; i < sinks.size(); i++) {
    auto& s = sinks[i];
    s().bindPipeline(Sink(*this, s.dataLimit, s.extensionLimit, i));
    all_requested |= s.dataLimit;
    scheduledWaves |= s.waveLimit;
    assert((attributes + references + contexts + DataClass::threads).allOf(s.waveLimit)
           && "Early wavefronts requested for invalid dataclasses!");
  }
  depChainComplete = true;

  // Make sure the Finalizers are ready before anything gets emitted.
  for(ProfileFinalizer& f: finalizers.all)
    f.bindPipeline(Source(*this, DataClass::all(), ExtensionClass::all()));

  structs.file.freeze();
  structs.context.freeze();
  structs.module.freeze();
  structs.metric.freeze();
  structs.thread.freeze();

  // Make sure the global Context is ready before letting any data in.
  cct.reset(new Context(structs.context, std::nullopt, {Relation::global, Scope(*this)}));
  for(auto& s: sinks) {
    if(s.dataLimit.hasContexts()) s().notifyContext(*cct);
  }

  // Now we can connect the input without losing any information.
  std::size_t idx = 0;
  for(auto& ms: sources) {
    ms.dataLimit = ms().provides();
    ms().bindPipeline(Source(*this, ms.dataLimit, ExtensionClass::all(), sourceLocals[idx]));
    scheduled |= ms.dataLimit;
    idx++;
  }
  scheduled &= all_requested;
  unscheduledWaves = scheduledWaves - scheduled;
  scheduledWaves &= scheduled;
}

void ProfilePipeline::complete(PerThreadTemporary&& tt, std::optional<std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>>& localTimepointBounds) {
  auto drain = [&](auto& tpd, auto type, auto notify) {
    // Drain the remaining timepoints from the staging buffer first
    if(!tpd.staging.empty()) {
      if(tpd.unboundedDisorder) {
        std::sort(tpd.staging.begin(), tpd.staging.end(),
            util::compare_only_first<typename decltype(tpd.staging)::value_type>());
      }
      for(auto& s: sinks) {
        if(!s.dataLimit.has(type)) continue;
        notify(s(), tpd.staging);
      }
      tpd.staging.clear();
    }
    // Then drain the timepoints from the sorting buffer
    if(!tpd.sortBuf.empty()) {
      auto tps = std::move(tpd.sortBuf).sorted();
      for(auto& s: sinks) {
        if(!s.dataLimit.has(type)) continue;
        notify(s(), tps);
      }
    }

    // Update our thread-local timepoint min-max bounds
    if(tt.minTime > std::chrono::nanoseconds::min()) {
      if(localTimepointBounds) {
        localTimepointBounds->first = std::min(localTimepointBounds->first, tt.minTime);
        localTimepointBounds->second = std::max(localTimepointBounds->second, tt.maxTime);
      } else
        localTimepointBounds = {tt.minTime, tt.maxTime};
    }
  };
  drain(tt.ctxTpData, DataClass::ctxTimepoints, [&](ProfileSink& s, const auto& tps){
    s.notifyTimepoints(tt.thread(), tps);
  });
  for(auto& [m, tpd]: tt.metricTpData.iterate()) {
    const Metric& mm = m;
    drain(tpd, DataClass::metricTimepoints, [&](ProfileSink& s, const auto& tps){
      s.notifyTimepoints(tt.thread(), mm, tps);
    });
  }

  // Finish off the Thread's metrics and let the Sinks know
  tt.finalize();
  std::shared_ptr<PerThreadTemporary> ttptr = std::make_shared<PerThreadTemporary>(std::move(tt));
  for(auto& s: sinks)
    if(s.dataLimit.hasThreads()) s().notifyThreadFinal(ttptr);
}

void ProfilePipeline::run() {
#ifndef NVALGRIND
  char start_arc;
  char barrier_arc;
  char single_arc;
  char barrier2_arc;
  char end_arc;
#endif  // !NVALGRIND

  std::array<std::atomic<std::size_t>, 4> countdowns;
  for(auto& c: countdowns) c.store(sources.size(), std::memory_order_relaxed);

  std::deque<std::reference_wrapper<PerThreadTemporary>> allMergedThreads;

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
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < sources.size(); ++i) {
      if(!(scheduledWaves & sources[i].dataLimit).hasAny()) {
        sources[i].wavesComplete.signal();
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
#ifndef NDEBUG
          // Now that the Source has read all of what we requested, we disallow
          // any further output of what we just read.
          sourceLocals[i].disabled |= req;
#endif
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

    std::optional<std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>>
      localTimepointBounds;

    // Now for the finishing wave
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < sources.size(); ++i) {
      auto& sl = sourceLocals[i];
      {
        sources[i].wavesComplete.wait();
        std::unique_lock<std::mutex> l(sources[i].lock);
        sl.lastWave = true;
        DataClass req = (sources[i]().finalizeRequest(scheduled - scheduledWaves)
                         - sources[i].read) & sources[i].dataLimit;
        sources[i].read |= req;
        if(req.hasAny()) sources[i]().read(req);
#ifndef NDEBUG
        sl.disabled |= req;
#endif
      }

      // Complete the threads unit to this Source in particular
      for(auto& tt: sl.threads) complete(std::move(tt), localTimepointBounds);

      // Clean up the Source-local data.
      sl.threads.clear();
      assert(sl.thawedMetrics.empty() && "Source exited before freezing all of its referenced Metrics!");
      sl.thawedMetrics.clear();
    }

    // Make sure everything has been read before we handle the merged threads
    ANNOTATE_HAPPENS_BEFORE(&barrier_arc);
    #pragma omp barrier
    ANNOTATE_HAPPENS_AFTER(&barrier_arc);

    // One thread fills allMergedThreads from the mergedThreads map, all others
    // wait for that to complete.
    #pragma omp single
    {
      for(auto& mt: mergedThreads)
        allMergedThreads.emplace_back(mt.second);

      ANNOTATE_HAPPENS_BEFORE(&single_arc);
    }
    ANNOTATE_HAPPENS_AFTER(&single_arc);

    // Handle all the Threads that were merged, same as for any other Thread
    #pragma omp for schedule(dynamic) nowait
    for(std::size_t i = 0; i < allMergedThreads.size(); ++i) {
      complete(std::move(allMergedThreads[i].get()), localTimepointBounds);
    }

    // Update the main timepoint bounds with our thread-local data
    if(localTimepointBounds) {
      std::unique_lock<std::mutex> l(attrsLock);
      if(timepointBounds) {
        timepointBounds->first = std::min(timepointBounds->first, localTimepointBounds->first);
        timepointBounds->second = std::max(timepointBounds->second, localTimepointBounds->second);
      } else
        timepointBounds = localTimepointBounds;
    }

    // Make sure all the merged threads have been handled before continuing
    ANNOTATE_HAPPENS_BEFORE(&barrier2_arc);
    #pragma omp barrier
    ANNOTATE_HAPPENS_AFTER(&barrier2_arc);

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

Source::Source() : pipe(nullptr), finalizeContexts(false) {};

// This signature is used for Finalizers, which don't have a SourceLocal.
Source::Source(ProfilePipeline& p, const DataClass& ds, const ExtensionClass& es)
  : pipe(&p), slocal(nullptr), dataLimit(ds), extensionLimit(es),
    finalizeContexts(false) {};
// This signature is for Sources, which have a SourceLocal.
Source::Source(ProfilePipeline& p, const DataClass& ds, const ExtensionClass& es, SourceLocal& sl)
  : pipe(&p), slocal(&sl), dataLimit(ds), extensionLimit(es),
    finalizeContexts(true) {};

const decltype(ProfilePipeline::Extensions::identifier)&
Source::identifier() const {
  assert(extensionLimit.hasIdentifier() && "Source did not register for `identifier` emission!");
  return pipe->uds.identifier;
}
const decltype(ProfilePipeline::Extensions::resolvedPath)&
Source::resolvedPath() const {
  assert(extensionLimit.hasResolvedPath() && "Source did not register for `resolvedPath` emission!");
  return pipe->uds.resolvedPath;
}

#define SRC_ASSERT_LIMITS(DS) do { \
  assert(dataLimit.has(DataClass:: DS) && "Source did not register for `" #DS "` emission!"); \
  assert(pipe->scheduled.has(DataClass:: DS) && "`" #DS "` are not scheduled for this Pipeline!"); \
  if(slocal != nullptr) \
    assert(!slocal->disabled.has(DataClass:: DS) && "Attempt to emit `" #DS "` after Source has already read(`" #DS "`)!"); \
} while(0)

#define SRC_ASSERT_LIMITS_MULTI(DS) do { \
  assert(dataLimit.anyOf(DS) && "Source did not register for `" #DS "` emission!"); \
  assert(pipe->scheduled.anyOf(DS) && "`" #DS "` are not scheduled for this Pipeline!"); \
  if(slocal != nullptr) \
    assert(!slocal->disabled.allOf(DS) && "Attempt to emit `" #DS "` after Source has already read(`" #DS "`)!"); \
} while(0)

void Source::attributes(const ProfileAttributes& as) {
  SRC_ASSERT_LIMITS(attributes);
  std::unique_lock<std::mutex> l(pipe->attrsLock);
  pipe->attrs.merge(as);
}

void Source::timepointBounds(std::chrono::nanoseconds min, std::chrono::nanoseconds max) {
  SRC_ASSERT_LIMITS_MULTI(DataClass::ctxTimepoints | DataClass::metricTimepoints);
  std::unique_lock<std::mutex> l(pipe->attrsLock);
  if(pipe->timepointBounds) {
    pipe->timepointBounds->first = std::min(min, pipe->timepointBounds->first);
    pipe->timepointBounds->second = std::max(max, pipe->timepointBounds->second);
  } else
    pipe->timepointBounds = std::make_pair(min, max);
}

Module& Source::module(const stdshim::filesystem::path& p) {
  SRC_ASSERT_LIMITS(references);
  auto x = pipe->mods.emplace(pipe->structs.module, p);
  auto r = &x.first();
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasReferences()) s().notifyModule(*r);
    }
  }
  return *r;
}

Module& Source::module(const stdshim::filesystem::path& p, const stdshim::filesystem::path& rp) {
  SRC_ASSERT_LIMITS(references);
  auto x = pipe->mods.emplace(pipe->structs.module, p, rp);
  auto r = &x.first();
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasReferences()) s().notifyModule(*r);
    }
  }
  return *r;
}

File& Source::file(const stdshim::filesystem::path& p) {
  SRC_ASSERT_LIMITS(references);
  auto x = pipe->files.emplace(pipe->structs.file, p);
  auto r = &x.first();
  if(x.second) {
    for(auto& s: pipe->sinks) {
      if(s.dataLimit.hasReferences()) s().notifyFile(*r);
    }
  }
  return *r;
}

Metric& Source::metric(Metric::Settings s) {
  SRC_ASSERT_LIMITS(attributes);
  auto x = pipe->mets.emplace(pipe->structs.metric, std::move(s));
  slocal->thawedMetrics.insert(&x.first());
  for(ProfileFinalizer& f: pipe->finalizers.statistics)
    f.appendStatistics(x.first(), x.first().statsAccess());
  return x.first();
}

ExtraStatistic& Source::extraStatistic(ExtraStatistic::Settings s) {
  SRC_ASSERT_LIMITS(attributes);
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
  }
  slocal->thawedMetrics.erase(&m);
}

Context& Source::global() { return *pipe->cct; }
void Source::notifyContext(Context& c) {
  for(auto& s: pipe->sinks) {
    if(s.dataLimit.hasContexts()) s().notifyContext(c);
  }
}
std::pair<Context&, Context&> Source::context(Context& p, const NestedScope& ns) {
  SRC_ASSERT_LIMITS(contexts);
  util::optional_ref<Context> res_rel;
  std::reference_wrapper<Context> res_flat = p;
  NestedScope res_ns = ns;
  if(finalizeContexts) {
    for(ProfileFinalizer& f: pipe->finalizers.classification) {
      NestedScope this_ns = ns;
      auto r = f.classify(p, this_ns);
      if(r) {
        res_ns = this_ns;
        res_rel = r->first;
        assert(!res_rel || res_rel->direct_parent() == &p);
        res_flat = r->second;
        break;
      }
    }
  }

  bool first;
  std::tie(res_flat, first) = res_flat.get().ensure(res_ns);
  if(first) notifyContext(res_flat);

  return {res_rel ? *res_rel : res_flat.get(), res_flat};
}

util::optional_ref<ContextFlowGraph> Source::contextFlowGraph(const Scope& s) {
  SRC_ASSERT_LIMITS(contexts);
  std::pair<const util::uniqued<ContextFlowGraph>&, bool> x = pipe->cgraphs.emplace(s);
  ContextFlowGraph& fg = x.first();
  if(x.second) {
    for(ProfileFinalizer& f: pipe->finalizers.classification) {
      if(f.resolve(fg)) break;
    }
    fg.freeze([&](const Scope& ss){
      assert(ss != s);
      return contextFlowGraph(ss);
    });
  }
  if(fg.empty()) return std::nullopt;
  return fg;
}

ContextReconstruction& Source::contextReconstruction(ContextFlowGraph& g, Context& r) {
  SRC_ASSERT_LIMITS(contexts);
  assert(!g.empty() && "FlowGraph obtained when it shouldn't have been?");
  auto x = r.reconsts_p->emplace(r, g);
  ContextReconstruction& rc = x.first;
  if(x.second) {
    rc.instantiate(
      [&](Context& c, const Scope& s) -> Context& {
        return context(c, {Relation::call, s}).second;
      }, [&](const Scope& s) -> ContextReconstruction& {
        assert(s != g.scope());
        auto fg = contextFlowGraph(s);
        assert(fg);  // Just assert here, tested during ContextFlowGraph::freeze
        return contextReconstruction(*fg, r);
      });
  }
  return rc;
}

void Source::addToReconstructionGroup(ContextFlowGraph& g, PerThreadTemporary& t, uint64_t gid) {
  auto& group = t.r_groups[gid];
  std::unique_lock<std::mutex> l(group.lock);
  auto [reconsts_it, first] = group.fg_reconsts.try_emplace(g);
  if(!first) return;
  auto& reconsts = reconsts_it->second;

  // Instantitate Reconstructions at every root in the group that calls any of
  // the Graph's entries.
  for(const Scope& entry: g.entries()) {
    auto roots_it = group.c_entries.find(entry);
    if(roots_it != group.c_entries.end()) {
      for(Context& r: roots_it->second)
        reconsts.insert(contextReconstruction(g, r));
    }
  }
}

void Source::addToReconstructionGroup(Context& r, const Scope& entry, PerThreadTemporary& t, uint64_t gid) {
  SRC_ASSERT_LIMITS(contexts);
  auto& group = t.r_groups[gid];
  std::unique_lock<std::mutex> l(group.lock);
  auto [it, first] = group.c_entries[entry].insert(r);
  if(!first) return;

  // Instantiate Reconstructions below this new root for every FlowGraph that
  // supports this entry point.
  for(auto& [g, reconsts]: group.fg_reconsts) {
    if(g->entries().count(entry) > 0)
      reconsts.insert(contextReconstruction(g, r));
  }
}

Source::AccumulatorsRef Source::accumulateTo(PerThreadTemporary& t, Context& c) {
  SRC_ASSERT_LIMITS(metrics);
  assert(slocal->lastWave && "Attempt to emit metrics before requested!");
  return AccumulatorsRef(t.c_data[c]);
}

Source::AccumulatorsRef Source::accumulateTo(PerThreadTemporary& t, ContextReconstruction& cr) {
  SRC_ASSERT_LIMITS(metrics);
  assert(slocal->lastWave && "Attempt to emit metrics before requested!");
  return AccumulatorsRef(t.r_data[cr]);
}

Source::AccumulatorsRef Source::accumulateTo(PerThreadTemporary& t, uint64_t g, Context& c) {
  SRC_ASSERT_LIMITS(metrics);
  assert(slocal->lastWave && "Attempt to emit metrics before requested!");
  return AccumulatorsRef(t.r_groups[g].c_data[c]);
}

Source::AccumulatorsRef Source::accumulateTo(PerThreadTemporary& t, uint64_t g, ContextFlowGraph& cfg) {
  SRC_ASSERT_LIMITS(metrics);
  assert(slocal->lastWave && "Attempt to emit metrics before requested!");
  auto& group = t.r_groups[g];
#ifndef NDEBUG
  {
    std::unique_lock<std::mutex> l(group.lock);
    assert(group.fg_reconsts.find(cfg) != group.fg_reconsts.end()
           && "addToReconstructionGroup must be called before accumulateTo!");
  }
#endif
  return AccumulatorsRef(group.fg_data[cfg]);
}

void Source::AccumulatorsRef::add(Metric& m, double v) {
  map.get()[m].add(v);
}

Thread& Source::newThread(ThreadAttributes o) {
  o.finalize(pipe->threadAttrFinalizeState);
  auto& t = *pipe->threads.emplace(new Thread(pipe->structs.thread, o)).first;
  for(auto& s: pipe->sinks) {
    if(s.dataLimit.hasThreads()) s().notifyThread(t);
  }
  return t;
}

PerThreadTemporary& Source::setup(PerThreadTemporary& tt) {
  tt.ctxTpData.staging.reserve(4096);
  if(tt.thread().attributes.ctxTimepointDisorder() > 0) {
    // We need K+1 to detect the case when it was >K-disordered
    // Then another +1 to so disorder is treated properly by the algorithm
    tt.ctxTpData.sortBuf = decltype(tt.ctxTpData.sortBuf)(
        tt.thread().attributes.ctxTimepointDisorder() + 2);
  }
  return tt;
}

PerThreadTemporary& Source::thread(ThreadAttributes o) {
  SRC_ASSERT_LIMITS(threads);
  assert(o.ok() && "Source did not fill out enough of the ThreadAttributes!");
  auto& t = newThread(std::move(o));
  slocal->threads.emplace_front(PerThreadTemporary(t));
  return setup(slocal->threads.front());
}

PerThreadTemporary& Source::mergedThread(ThreadAttributes o) {
  SRC_ASSERT_LIMITS(threads);
  assert(o.ok() && "Source did not fill out enough of the ThreadAttributes!");
  {
    std::shared_lock<std::shared_mutex> l(pipe->mergedThreadsLock);
    auto it = pipe->mergedThreads.find(o.idTuple());
    if(it != pipe->mergedThreads.end()) return it->second;
  }
  std::unique_lock<std::shared_mutex> l(pipe->mergedThreadsLock);
  auto it = pipe->mergedThreads.find(o.idTuple());
  if(it != pipe->mergedThreads.end()) return it->second;

  // Now we are confident this is a new Thread, so create it
  auto& t = newThread(std::move(o));
  auto [x_tt, first] = pipe->mergedThreads.emplace(t.attributes.idTuple(), PerThreadTemporary(t));
  assert(first);
  return setup(x_tt->second);
}

template<class Tp, class Rewind, class Notify, class Singleton>
Source::TimepointStatus Source::timepoint(PerThreadTemporary& tt, PerThreadTemporary::TimepointsData<Tp>& tpd,
    Tp tp, Singleton type, const Rewind& rewind, const Notify& notify) {
  tt.minTime = std::min(tt.minTime, std::get<0>(tp));
  tt.maxTime = std::max(tt.maxTime, std::get<0>(tp));

  if(tpd.unboundedDisorder) {
    // Stash in the staging buffer until we have 'em all to sort together
    tpd.staging.push_back(std::move(tp));
    return TimepointStatus::next;
  }

  if(tpd.sortBuf.bound() > 0) {
    if(!tpd.sortBuf.full()) {
      // The buffer hasn't filled yet. Add the new point and continue.
      tpd.sortBuf.push(std::move(tp));
      return TimepointStatus::next;
    }

    // Replace an element and see if we're over the bound
    auto [elem, over] = tpd.sortBuf.replace(std::move(tp));
    if(over) {
      // We hit the disorder bound. Reset in preparation for fallbacks.
      tpd.sortBuf.clear();
      tpd.staging.clear();

      if(tpd.sortBuf.bound() < 800) {
        // Our fallback is 1023, but we only try it if the previous attempt was
        // with a significantly smaller bound. Rewinds are expensive.
        tpd.sortBuf = decltype(tpd.sortBuf)(1023);
      } else {
        // Fall back to loading the whole thing in memory
        util::log::warning{} << "Trace for a thread is unexpectedly extremely"
             " unordered, falling back to an in-memory sort.\n"
             "  This may indicate an issue during measurement, and WILL"
             " significantly increase memory usage!\n"
             "  Affected thread: " << tt.thread().attributes;
        tpd.unboundedDisorder = true;
      }

      for(auto& s: pipe->sinks) {
        if(!s.dataLimit.has(type)) continue;
        rewind(s());
      }
      return TimepointStatus::rewindStart;
    }

    tp = elem;
  }

  // Tack it onto the end of the staging vector. If its full enough let the
  // Sinks at it for a bit.
  tpd.staging.push_back(std::move(tp));
  if(tpd.staging.size() >= 4096) {
    for(auto& s: pipe->sinks) {
      if(!s.dataLimit.has(type)) continue;
      notify(s(), tpd.staging);
    }
    tpd.staging.clear();
    tpd.staging.reserve(4096);
  }
  return TimepointStatus::next;
}

Source::TimepointStatus Source::timepoint(PerThreadTemporary& tt, Context& c, std::chrono::nanoseconds tm) {
  SRC_ASSERT_LIMITS(ctxTimepoints);
  assert(slocal->lastWave && "Attempt to emit timepoints before requested!");
  return timepoint(tt, tt.ctxTpData, {tm, c}, DataClass::ctxTimepoints,
    [&](ProfileSink& s) { s.notifyCtxTimepointRewindStart(tt.thread()); },
    [&](ProfileSink& s, const auto& tps) { s.notifyTimepoints(tt.thread(), tps); });
}

Source::TimepointStatus Source::timepoint(PerThreadTemporary& tt, Metric& m, double v, std::chrono::nanoseconds tm) {
  SRC_ASSERT_LIMITS(ctxTimepoints);
  assert(slocal->lastWave && "Attempt to emit timepoints before requested!");
  auto x = tt.metricTpData.try_emplace(m);
  auto& tpd = x.first;
  if(x.second) {
    tpd.staging.reserve(4096);
    auto dis = tt.thread().attributes.metricTimepointDisorder(m);
    if(dis > 0) {
      // We need K+1 to detect the case when it was >K-disordered
      // Then another +1 to so disorder is treated properly by the algorithm
      tpd.sortBuf = decltype(tpd.sortBuf)(dis + 2);
    }
  }
  return timepoint(tt, tpd, {tm, v}, DataClass::metricTimepoints,
    [&](ProfileSink& s) { s.notifyMetricTimepointRewindStart(tt.thread(), m); },
    [&](ProfileSink& s, const auto& tps) { s.notifyTimepoints(tt.thread(), m, tps); });
}

Source& Source::operator=(Source&& o) {
  assert(pipe == nullptr && "Attempt to rebind a Source!");
  pipe = o.pipe;
  slocal = o.slocal;
  dataLimit = o.dataLimit;
  extensionLimit = o.extensionLimit;
  finalizeContexts = o.finalizeContexts;
  return *this;
}

Sink::Sink() : pipe(nullptr), idx(0) {};
Sink::Sink(ProfilePipeline& p, const DataClass& d, const ExtensionClass& e, std::size_t i)
  : pipe(&p), dataLimit(d), extensionLimit(e), idx(i), orderedWavefront(false),
    priorWavefrontDepOnce(std::numeric_limits<std::size_t>::max()),
    orderedWrite(false),
    priorWriteDepOnce(std::numeric_limits<std::size_t>::max()) {};

void Sink::registerOrderedWavefront() {
  assert(!pipe->depChainComplete && "Attempt to register a Sink for an ordered wavefront chain after notifyPipeline!");
  if(!orderedWavefront) {
    orderedWavefront = true;
    priorWavefrontDepOnce = pipe->sinkWavefrontDepChain;
    pipe->sinks[idx].wavefrontPriorDelay = pipe->sinkWavefrontDepClasses;
    pipe->sinkWavefrontDepClasses |= pipe->sinks[idx].waveLimit;
    pipe->sinkWavefrontDepChain = idx;
  }
}
void Sink::registerOrderedWrite() {
  assert(!pipe->depChainComplete && "Attempt to register a Sink for an ordered write chain after notifyPipeline!");
  if(!orderedWrite) {
    orderedWrite = true;
    pipe->sinkWriteDepChain = idx;
  }
}

util::Once::Caller Sink::enterOrderedWavefront() {
  assert(orderedWavefront && "Attempt to enter an ordered wavefront region without registering!");
  if(priorWavefrontDepOnce != std::numeric_limits<std::size_t>::max())
    pipe->sinks[priorWavefrontDepOnce].wavefrontDepOnce.wait();
  return pipe->sinks[idx].wavefrontDepOnce.signal();
}
util::Once::Caller Sink::enterOrderedWrite() {
  assert(orderedWrite && "Attempt to enter an ordered write region without registering!");
  if(priorWriteDepOnce != std::numeric_limits<std::size_t>::max())
    pipe->sinks[priorWriteDepOnce].writeDepOnce.wait();
  return pipe->sinks[idx].writeDepOnce.signal();
}

const decltype(ProfilePipeline::Extensions::identifier)&
Sink::identifier() const {
  assert(extensionLimit.hasIdentifier() && "Sink did not register for `identifier` absorption!");
  return pipe->uds.identifier;
}
const decltype(ProfilePipeline::Extensions::resolvedPath)&
Sink::resolvedPath() const {
  assert(extensionLimit.hasResolvedPath() && "Sink did not register for `resolvedPath` absorption!");
  return pipe->uds.resolvedPath;
}

const ProfileAttributes& Sink::attributes() {
  assert(dataLimit.hasAttributes() && "Sink did not register for `attributes` absorption!");
  return pipe->attrs;
}

std::optional<std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>>
Sink::timepointBounds() {
  assert(dataLimit.anyOf(DataClass::ctxTimepoints | DataClass::metricTimepoints)
         && "Sink did not register for a `*Timepoints` absorption!");
  return pipe->timepointBounds;
}

const util::locked_unordered_uniqued_set<Module, stdshim::shared_mutex,
    util::uniqued_hash<stdshim::hash_path>>& Sink::modules() {
  assert(dataLimit.hasReferences() && "Sink did not register for `references` absorption!");
  return pipe->mods;
}

const util::locked_unordered_uniqued_set<File, stdshim::shared_mutex,
    util::uniqued_hash<stdshim::hash_path>>& Sink::files() {
  assert(dataLimit.hasReferences() && "Sink did not register for `references` absorption!");
  return pipe->files;
}

const util::locked_unordered_uniqued_set<Metric>& Sink::metrics() {
  assert(dataLimit.hasAttributes() && "Sink did not register for `attributes` absorption!");
  return pipe->mets;
}

const util::locked_unordered_uniqued_set<ExtraStatistic>& Sink::extraStatistics() {
  assert(dataLimit.hasAttributes() && "Sink did not register for `attributes` absorption!");
  return pipe->estats;
}

const Context& Sink::contexts() {
  assert(dataLimit.hasContexts() && "Sink did not register for `contexts` absorption!");
  return *pipe->cct;
}

const util::locked_unordered_uniqued_set<ContextFlowGraph>& Sink::contextFlowGraphs() {
  assert(dataLimit.hasContexts() && "Sink did not register for `contexts` absorption!");
  return pipe->cgraphs;
}

const util::locked_unordered_set<std::unique_ptr<Thread>>& Sink::threads() {
  assert(dataLimit.hasThreads() && "Sink did not register for `threads` absorption!");
  return pipe->threads;
}

std::size_t Sink::teamSize() const noexcept {
  return pipe->team_size;
}

Sink& Sink::operator=(Sink&& o) {
  assert(pipe == nullptr && "Attempt to rebind a Sink!");
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
