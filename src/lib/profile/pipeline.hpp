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
// Copyright ((c)) 2019-2022, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_PIPELINE_H
#define HPCTOOLKIT_PROFILE_PIPELINE_H

#include "attributes.hpp"
#include "context.hpp"
#include "dataclass.hpp"
#include "lexical.hpp"
#include "metric.hpp"
#include "module.hpp"

#include "util/locked_unordered.hpp"
#include "util/once.hpp"

#include <map>
#include <bitset>
#include <optional>
#include <memory>
#include <chrono>
#include <stdexcept>

namespace hpctoolkit {

class ProfileSource;
class ProfileSink;
class ProfileFinalizer;

namespace detail {

// Hidden move-only class for helping with Profile* management. Effectively this
// is what can be carried between the Pipeline::Settings and the Pipeline.
class ProfilePipelineBase {
protected:
  // This class (and subclasses) are move-only and highly controlled.
  ProfilePipelineBase() = default;
  ProfilePipelineBase(const ProfilePipelineBase&) = delete;
  ProfilePipelineBase(ProfilePipelineBase&&) = default;
  ~ProfilePipelineBase() = default;

  // All the Sources. These don't have callbacks, so they can be together.
  struct SourceEntry {
    SourceEntry(ProfileSource& s);
    SourceEntry(std::unique_ptr<ProfileSource>&& up);
    SourceEntry(SourceEntry&& o);

    DataClass dataLimit;

    util::Once wavesComplete;
    std::mutex lock;
    std::reference_wrapper<ProfileSource> source;
    std::unique_ptr<ProfileSource> up_source;
    DataClass read;

    operator ProfileSource&() { return source; }
    ProfileSource& operator()() { return source; }
  };
  std::vector<SourceEntry> sources;

  // All the Sinks, with notes on which callbacks they should be triggered for.
  struct SinkEntry {
    SinkEntry(DataClass d, DataClass w, ExtensionClass e, ProfileSink& s)
      : dataLimit(d), waveLimit(w), extensionLimit(e), sink(s) {};
    SinkEntry(SinkEntry&& o)
      : dataLimit(o.dataLimit), waveLimit(o.waveLimit),
        extensionLimit(o.extensionLimit), sink(o.sink) {};

    DataClass dataLimit;
    DataClass waveLimit;
    ExtensionClass extensionLimit;
    std::reference_wrapper<ProfileSink> sink;

    std::mutex wavefrontStatusLock;
    DataClass wavefrontState;
    DataClass wavefrontPriorDelay;
    DataClass wavefrontDelivered;

    util::Once wavefrontDepOnce;
    util::Once writeDepOnce;

    operator ProfileSink&() { return sink; }
    ProfileSink& operator()() { return sink; }
  };
  std::vector<SinkEntry> sinks;

  // All the Finalizers, split up by their extensions.
  struct {
    std::vector<std::reference_wrapper<ProfileFinalizer>> classification;
    std::vector<std::reference_wrapper<ProfileFinalizer>> identifier;
    std::vector<std::reference_wrapper<ProfileFinalizer>> resolvedPath;
    std::vector<std::reference_wrapper<ProfileFinalizer>> statistics;
    std::vector<std::reference_wrapper<ProfileFinalizer>> all;
  } finalizers;

  ExtensionClass available;  // Maximum available Extension set.
  ExtensionClass requested;  // Minimal requested Extension set.

  // Storage for the unique_ptrs
  std::vector<std::unique_ptr<ProfileSink>> up_sinks;
  std::vector<std::unique_ptr<ProfileFinalizer>> up_finalizers;
};

}  // namespace detail

/// Monolithic Pipeline for processing Profile data. Effectively connects
/// ProfileSources to ProfileSinks, performs mediation of required data, and
/// handles the buffers when a Sink is not ready to receive data yet.
class ProfilePipeline final : public detail::ProfilePipelineBase {
public:
  class Settings;

  /// Setup structure for Pipelines. Roughly speaking, if the normal Pipeline
  /// is a compiled fast-as-lightning object, this is the source form.
  class Settings final : public detail::ProfilePipelineBase {
  public:
    Settings() = default;
    ~Settings() = default;

    /// Append a new Source to the future Pipeline, with optional ownership.
    // MT: Externally Synchronized
    Settings& operator<<(ProfileSource&);
    Settings& operator<<(std::unique_ptr<ProfileSource>&&);

    /// Append a new Sink to the future Pipeline, with optional ownership.
    // MT: Externally Synchronized
    Settings& operator<<(ProfileSink&);
    Settings& operator<<(std::unique_ptr<ProfileSink>&&);

    /// Append a new Finalizer to the future Pipeline, with optional ownership.
    // MT: Externally Synchronized
    Settings& operator<<(ProfileFinalizer&);
    Settings& operator<<(std::unique_ptr<ProfileFinalizer>&&);
  };

  /// Compile the given Settings into a usable Pipeline. The teamSize determines
  /// how many threads to use for processing.
  ProfilePipeline(Settings&&, std::size_t teamSize);

  // Move construction is disabled; after bind'ing the Sources and Sinks we have
  // to have a stable address.
  ProfilePipeline(ProfilePipeline&&) = delete;

  /// Destructor.
  ~ProfilePipeline() = default;

  /// Process everything until there's nothing left.
  // MT: Externally Synchronized
  void run();

  /// Storage structure for the various Userdata slots available.
  struct Structs {
    File::ud_t::struct_t file;
    Context::ud_t::struct_t context;
    Module::ud_t::struct_t module;
    Metric::ud_t::struct_t metric;
    Thread::ud_t::struct_t thread;
  };

  /// Userdata registrations for the Extended data.
  struct Extensions {
    struct {
      File::ud_t::typed_member_t<unsigned int> file;
      const auto& operator()(File::ud_t&) const noexcept { return file; }
      Context::ud_t::typed_member_t<unsigned int> context;
      const auto& operator()(Context::ud_t&) const noexcept { return context; }
      Module::ud_t::typed_member_t<unsigned int> module;
      const auto& operator()(Module::ud_t&) const noexcept { return module; }
      Metric::ud_t::typed_member_t<Metric::Identifier> metric;
      const auto& operator()(Metric::ud_t&) const noexcept { return metric; }
      Thread::ud_t::typed_member_t<unsigned int> thread;
      const auto& operator()(Thread::ud_t&) const noexcept { return thread; }
    } identifier;

    struct {
      File::ud_t::typed_member_t<stdshim::filesystem::path> file;
      const auto& operator()(File::ud_t&) const noexcept { return file; }
      Module::ud_t::typed_member_t<stdshim::filesystem::path> module;
      const auto& operator()(Module::ud_t&) const noexcept { return module; }
    } resolvedPath;
  };

private:
  // Internal Source-local storage structure. Externally Synchronized.
  struct SourceLocal {
    std::forward_list<PerThreadTemporary> threads;
    std::unordered_set<Metric*> thawedMetrics;
    bool lastWave = false;
#ifndef NDEBUG
    DataClass disabled;
#endif
  };

public:
  /// Registration structure for a PipelineSource. All access to a Pipeline from
  /// a Source must come through this structure.
  class Source final {
  public:
    Source();
    Source(Source&&) = default;
    ~Source() = default;

    /// Allow registration of custom userdata for Sources.
    Structs& structs() { return pipe->structs; }

    /// Access the Extension userdata members.
    // MT: Safe (const)
    const decltype(Extensions::identifier)& identifier() const;
    const decltype(Extensions::resolvedPath)& resolvedPath() const;

    /// Get the limits on this Source's emissions.
    DataClass limit() const noexcept { return dataLimit & pipe->scheduled; }

    /// Emit some new attributes into the Pipeline.
    /// DataClass: `attributes`
    // MT: Externally Synchronized (this), Internally Synchronized
    void attributes(const ProfileAttributes&);

    /// Emit some Thread-less timepoint bounds into the Pipeline.
    /// DataClass: `ctxTimepoints` or `metricTimepoints`
    // MT: Externally Synchronized (this), Internally Synchronized
    void timepointBounds(std::chrono::nanoseconds min, std::chrono::nanoseconds max);

    /// Emit a new Module into the Pipeline, returning the canonical copy.
    /// DataClass: `references`
    // MT: Externally Synchronized (this), Internally Synchronized
    Module& module(const stdshim::filesystem::path&);

    /// Emit a new File into the Pipeline, returning the canonical copy.
    /// DataClass: `references`
    // MT: Externally Synchronized (this), Internally Synchronized
    File& file(const stdshim::filesystem::path&);

    /// Emit a new Metric into the Pipeline. The returned Metric may not be
    /// used until after a call to metricFreeze.
    /// DataClass: `attributes`
    // MT: Externally Synchronized (this), Internally Synchronized
    Metric& metric(Metric::Settings);

    /// Emit a new ExtraStatistic into the Pipeline.
    /// DataClass: `attributes`
    // MT: Externally Synchronized (this), Internally Synchronized
    ExtraStatistic& extraStatistic(ExtraStatistic::Settings);

    /// "Freeze" the given previously emitted Metric. All requests through
    /// Metric::StatsAccess must complete prior to this call.
    /// DataClass: `attributes`
    // MT: Externally Synchronized (this), Internally Synchronized
    void metricFreeze(Metric&);

    /// Reference the global Context.
    // MT: Externally Synchronized (this), Internally Synchronized
    Context& global();

  private:
    void notifyContext(Context&);

  public:
    /// Emit a new Context into the Pipeline as the child of another.
    /// ProfileFinalizers may inject additional Contexts between the parent
    /// resulting child Context to provide additional (usually lexical) context.
    ///
    /// Two Contexts are returned, the first refers to the relation while the
    /// second refers to the flat Scope in context. For example, a call to
    /// `context((point){A}, (nominal call to):(point){B})` that expands to
    ///     (point){A}
    ///       (nominal call to):(func){funcB@/b.c:10}  <-- .first
    ///         (enclosed sub-Scope):(line){b.c:12}
    ///           (enclosed sub-Scope):(point){B}      <-- .second
    /// will return the two marked Contexts.
    ///
    /// This method presupposes the single sequence of Contexts between the
    /// parent and child can be derived statically from the Scope.
    /// See contextReconstruction for a case when this is not possible.
    /// DataClass: `contexts`
    // MT: Externally Synchronized (this), Internally Synchronized
    std::pair<Context&, Context&> context(Context&, const NestedScope&);

    /// Emit a new ContextReconstruction for the given ContextFlowGraph,
    /// rooted at a particular Context.
    ///
    /// In summary, this handles the case where `graph.scope()` was executed
    /// while `root` was calling an "entry" Scope, but the exact sequence of
    /// zero or more calls used to get from `root` to `graph.scope()` are
    /// unknown. In this case we do our best to reconstruct the missing calling
    /// Contexts, and if we end up with multiple valid candidates we distribute
    /// metric values based on the probability distribution of candidates.
    /// See ContextReconstruction for more details on this process.
    ///
    /// Note that this is fundamentally different from
    ///     `root` -> unknown -> `graph.scope()`
    /// which represents the case where what `root` called is unknown. Here
    /// we know what `root` called and need to reconstruct how that ended up
    /// in `graph.scope()`.
    ///
    /// DataClass: `contexts`
    // MT: Externally Synchronized (this), Internally Synchronized
    ContextReconstruction& contextReconstruction(ContextFlowGraph& graph,
                                                 Context& root);

    /// Emit a ContextFlowGraph, generated statically from the given Scope.
    ///
    /// FlowGraphs are a generalization of ContextReconstructions where the
    /// root Context is no longer known. Instead FlowGraphs are added to
    /// "Reconstruction groups" along with root Contexts, metric values
    /// attributed to the FlowGraph (and group) are redistributed to
    /// reconstructed Contexts below any and/or all of the roots in the group.
    /// When added root Contexts indicate which entry Scopes they call, the root
    /// becomes a target for redistribution if the FlowGraph contains matching
    /// entry Scopes.
    /// See ContextFlowGraph for more details on this process.
    ///
    /// If there is no suitable binary analysis results for the given
    /// Scope('s Module), std::nullopt is returned instead. The caller is
    /// responsible for adapting accordingly.
    ///
    /// DataClass: `contexts`
    // MT: Externally Synchronized (this), Internally Synchronized
    util::optional_ref<ContextFlowGraph> contextFlowGraph(const Scope&);

    /// Include a ContextFlowGraph in the given Reconstruction group.
    /// See contextFlowGraph for more details on what this means in practice.
    ///
    /// This function must be called before metric values can be attributed to
    /// the FlowGraph in the given group.
    ///
    /// DataClass: `contexts`
    // MT: Externally Synchronized (this), Internally Synchronized
    void addToReconstructionGroup(ContextFlowGraph&, PerThreadTemporary&, uint64_t);

    /// Include a Context as a root in the given Reconstruction group, which
    /// may plausibly call the given entry Scope.
    /// See contextFlowGraph for more details on what this means in practice.
    /// DataClass: `contexts`
    // MT: Externally Synchronized (this), Internally Synchronized
    void addToReconstructionGroup(Context&, const Scope&, PerThreadTemporary&, uint64_t);

  private:
    // Helpers functions for creating and setting up new Threads
    Thread& newThread(ThreadAttributes);
    PerThreadTemporary& setup(PerThreadTemporary&);

  public:
    /// Emit a new Thread into the Pipeline.
    /// DataClass: `threads`
    // MT: Externally Synchronized (this), Internally Synchronized
    PerThreadTemporary& thread(ThreadAttributes);

    /// Emit a Thread into the Pipeline, with merging based on the idTuple.
    /// Normally Threads are never merged, since doing so would require
    /// PerThreadTemporary data to have a lifetime over the whole process. This
    /// method allows that restriction to be redacted. Use sparingly.
    ///
    /// TODO: Document the thread-safety properties of PerThreadTemporary.
    /// DataClass: `threads`
    // MT: Externally Synchronized (this), Internally Synchronized
    PerThreadTemporary& mergedThread(ThreadAttributes);

    /// Return codes for timepoint-related functions
    enum class TimepointStatus {
      /// The next timepoint should be a new not-yet-emitted timepoint.
      next,
      /// The next timepoint should be the first timepoint. Reemit all timepoints.
      rewindStart,
    };

  private:
    // Helper template to merge common code for all timepoint types
    template<class Tp, class Rw, class Nt, class Sg>
    [[nodiscard]] TimepointStatus timepoint(PerThreadTemporary&,
        PerThreadTemporary::TimepointsData<Tp>&, Tp, Sg, const Rw&, const Nt&);

  public:
    /// Emit a Context-type timepoint into the Pipeline.
    /// Returns the expected next timepoint the caller should inject.
    /// DataClass: `ctxTimepoints`
    // MT: Externally Synchronized (this), Internally Synchronized
    [[nodiscard]] TimepointStatus timepoint(PerThreadTemporary&, Context&, std::chrono::nanoseconds);

    /// Emit a Metric-value timepoint into the Pipeline.
    /// Returns the expected next timepoint the caller should inject.
    /// DataClass: `metricTimepoints`
    // MT: Externally Synchronized (this), Internally Synchronized
    [[nodiscard]] TimepointStatus timepoint(PerThreadTemporary&, Metric&, double, std::chrono::nanoseconds);

    /// Reference to the Thread-local metric data for a particular Context.
    /// Allows for efficient emmission of multiple Metrics' data to one location.
    class AccumulatorsRef final {
    public:
      AccumulatorsRef() = delete;

      AccumulatorsRef(const AccumulatorsRef&) = default;
      AccumulatorsRef(AccumulatorsRef&&) = default;
      AccumulatorsRef& operator=(const AccumulatorsRef&) = default;
      AccumulatorsRef& operator=(AccumulatorsRef&&) = default;

      /// Emit some Thread-local metric data into the Pipeline.
      // MT: Externally Synchronized (this, Source), Internally Synchronized
      void add(Metric&, double);

    private:
      friend class ProfilePipeline::Source;
      std::reference_wrapper<decltype(PerThreadTemporary::c_data)::mapped_type> map;
      explicit AccumulatorsRef(decltype(map)::type& m) : map(m) {};
    };

    /// Attribute metric values to the given Thread and Context, by proxy
    /// of the returned AccumulatorsRef.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    AccumulatorsRef accumulateTo(PerThreadTemporary&, Context&);

    /// Attribute metric values to a ContextReconstruction. These values will be
    /// redistributed back to the base Contexts, see ContextReconstruction.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    AccumulatorsRef accumulateTo(PerThreadTemporary&, ContextReconstruction&);

    /// Attribute metric values to a Context, as part of the given
    /// Reconstruction group. These will be summed with the non-group values
    /// but are more importantly used for FlowGraph redistribution.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    AccumulatorsRef accumulateTo(PerThreadTemporary&, uint64_t group, Context&);

    /// Attribute metric values to a ContextFlowGraph. These values will be
    /// redistributed among the root Contexts included in the given
    /// Reconstruction group.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    AccumulatorsRef accumulateTo(PerThreadTemporary&, uint64_t group,
                                 ContextFlowGraph&);

    // Disable copy-assignment, and allow move assignment.
    Source& operator=(const Source&) = delete;
    Source& operator=(Source&&);

  private:
    friend class ProfilePipeline;
    Source(ProfilePipeline&, const DataClass&, const ExtensionClass&);
    Source(ProfilePipeline&, const DataClass&, const ExtensionClass&, SourceLocal&);
    ProfilePipeline* pipe;
    SourceLocal* slocal;
    DataClass dataLimit;
    ExtensionClass extensionLimit;
    bool finalizeContexts;
  };

  /// Registration structure for a PipelineSink. All access to a Pipeline from
  /// a Sink must come through this structure.
  class Sink final {
  public:
    Sink();
    ~Sink() = default;

    /// Register this Sink with the dependency ordering chains.
    void registerOrderedWavefront();
    void registerOrderedWrite();

    /// Wait for the the previous element of the dependency ordering chain to
    /// complete, and return a Once::Caller to signal when complete.
    util::Once::Caller enterOrderedWavefront();
    util::Once::Caller enterOrderedWrite();

    /// Access the Extensions available within the Pipeline.
    const decltype(Extensions::identifier)& identifier() const;
    const decltype(Extensions::resolvedPath)& resolvedPath() const;

    /// Allow registration of Userdata for Sinks.
    Structs& structs() { return pipe->structs; }

    /// Access to the canonical copies of the data within the Pipeline.
    /// Can only be used after the appropriate wavefront or write() barrier.
    const ProfileAttributes& attributes();
    std::optional<std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>>
      timepointBounds();
    const util::locked_unordered_uniqued_set<Module, stdshim::shared_mutex,
        util::uniqued_hash<stdshim::hash_path>>& modules();
    const util::locked_unordered_uniqued_set<File, stdshim::shared_mutex,
        util::uniqued_hash<stdshim::hash_path>>& files();
    const util::locked_unordered_uniqued_set<Metric>& metrics();
    const util::locked_unordered_uniqued_set<ExtraStatistic>& extraStatistics();
    const Context& contexts();
    const util::locked_unordered_uniqued_set<ContextFlowGraph>& contextFlowGraphs();
    const util::locked_unordered_set<std::unique_ptr<Thread>>& threads();

    /// Get the size of the worker team in use by the connected Pipeline. Useful
    /// for tuning sizes of things based on the number of available threads.
    std::size_t teamSize() const noexcept;

    // Disable copy-assignment, and allow move assignment.
    Sink& operator=(const Sink&) = delete;
    Sink& operator=(Sink&&);

  private:
    friend class ProfilePipeline;
    Sink(ProfilePipeline& p, const DataClass&, const ExtensionClass&, std::size_t);
    ProfilePipeline* pipe;
    DataClass dataLimit;
    ExtensionClass extensionLimit;
    std::size_t idx;

    bool orderedWavefront;
    std::size_t priorWavefrontDepOnce;
    bool orderedWrite;
    std::size_t priorWriteDepOnce;
  };

private:
  // Finalize the data in a PerThreadTemporary, and commit it to the Sinks
  // MT: Externally Synchronized (tt, localTimepointBounds), Internally Synchronized (this)
  void complete(PerThreadTemporary&& tt, std::optional<std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>>& localTimepointBounds);

  // Scheduled data transfer. Minimal requested and available set.
  DataClass scheduled;
  // Scheduled early wavefronts. Minimal requested and available set.
  DataClass scheduledWaves;
  // Requested but not scheduled early wavefronts.
  DataClass unscheduledWaves;
  // Size of the worker thread teams for doing things.
  std::size_t team_size;

  // Atomic counters for the early wavefronts.
  struct Waves {
    Waves() = delete;
    Waves(std::size_t i) : attributes(i), references(i), contexts(i) {};
    std::atomic<std::size_t> attributes;
    std::atomic<std::size_t> references;
    std::atomic<std::size_t> contexts;
  } waves;

  // Chain pointers marking dependency chains
  std::size_t sourcePrewaveRegionDepChain;
  std::size_t sinkWavefrontDepChain;
  DataClass sinkWavefrontDepClasses;
  std::size_t sourcePostwaveRegionDepChain;
  std::size_t sinkWriteDepChain;
  bool depChainComplete;

  // Storage for the pointers to the SourceLocals.
  std::vector<SourceLocal> sourceLocals;

  // Bits needed for ThreadAttributes to finalize
  ThreadAttributes::FinalizeState threadAttrFinalizeState;

  // Userdata structures for the various bits. Must be above the data itself.
  Structs structs;
  Extensions uds;

  // Storage for the canonical copies of everything. Because keeping track of
  // reference counts is a downright pain.
  std::mutex attrsLock;
  ProfileAttributes attrs;
  std::optional<std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>>
    timepointBounds;
  util::locked_unordered_set<std::unique_ptr<Thread>> threads;
  util::locked_unordered_uniqued_set<Module, stdshim::shared_mutex,
      util::uniqued_hash<stdshim::hash_path>> mods;
  util::locked_unordered_uniqued_set<File, stdshim::shared_mutex,
      util::uniqued_hash<stdshim::hash_path>> files;
  util::locked_unordered_uniqued_set<Metric> mets;
  util::locked_unordered_uniqued_set<ExtraStatistic> estats;
  std::unique_ptr<Context> cct;
  util::locked_unordered_uniqued_set<ContextFlowGraph> cgraphs;

  struct TupleHash {
    std::hash<uint16_t> h_u16;
    std::hash<uint64_t> h_u64;
    size_t operator()(const std::vector<pms_id_t>&) const noexcept;
  };
  struct TupleEqual {
    bool operator()(const std::vector<pms_id_t>&, const std::vector<pms_id_t>&) const noexcept;
  };
  std::shared_mutex mergedThreadsLock;
  std::unordered_map<std::vector<pms_id_t>, PerThreadTemporary,
                     TupleHash, TupleEqual> mergedThreads;
};

}

#endif  // HPCTOOLKIT_PROFILE_PIPELINE_H
