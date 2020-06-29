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

#ifndef HPCTOOLKIT_PROFILE_PIPELINE_H
#define HPCTOOLKIT_PROFILE_PIPELINE_H

#include "context.hpp"
#include "metric.hpp"
#include "util/locked_unordered.hpp"
#include "module.hpp"
#include "attributes.hpp"
#include "dataclass.hpp"

#include <map>
#include <bitset>
#include <memory>
#include <chrono>
#include <stdexcept>

namespace hpctoolkit {

class ProfileSource;
class ProfileSink;
class ProfileTransformer;
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
  std::vector<std::reference_wrapper<ProfileSource>> sources;

  // All the transformers. These don't register limits, so they can be together.
  std::vector<std::reference_wrapper<ProfileTransformer>> transformers;

  // All the Sinks, with notes on which callbacks they should be triggered for.
  struct SinkEntry {
    DataClass dataLimit;
    DataClass waveLimit;
    ExtensionClass extensionLimit;
    std::reference_wrapper<ProfileSink> sink;

    operator ProfileSink&() { return sink; }
    ProfileSink& operator()() { return sink; }
  };
  std::vector<SinkEntry> sinks;

  // All the Finalizers, split up by their extensions.
  struct {
    std::vector<std::reference_wrapper<ProfileFinalizer>> classification;
    std::vector<std::reference_wrapper<ProfileFinalizer>> identifier;
    std::vector<std::reference_wrapper<ProfileFinalizer>> resolvedPath;
    std::vector<std::reference_wrapper<ProfileFinalizer>> all;
  } finalizers;

  ExtensionClass available;  // Maximum available Extension set.
  ExtensionClass requested;  // Minimal requested Extension set.

  // Storage for the unique_ptrs
  std::vector<std::unique_ptr<ProfileSource>> up_sources;
  std::vector<std::unique_ptr<ProfileSink>> up_sinks;
  std::vector<std::unique_ptr<ProfileTransformer>> up_transformers;
  std::vector<std::unique_ptr<ProfileFinalizer>> up_finalizers;
};

}  // namespace detail

/// Monolithic Pipeline for processing Profile data. Effectively connects
/// ProfileSources to ProfileSinks, performs mediation of required data, and
/// handles the buffers when a Sink is not ready to receive data yet.
class ProfilePipeline final : public detail::ProfilePipelineBase {
public:
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

    /// Append a new Transformer to the future Pipeline, with optional ownership.
    // MT: Externally Synchronized
    Settings& operator<<(ProfileTransformer&);
    Settings& operator<<(std::unique_ptr<ProfileTransformer>&&);

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

  /// Type to use for timeouts. Because sometimes we just don't like waiting.
  using timeout_t = std::chrono::nanoseconds;
  static constexpr timeout_t timeout_forever = timeout_t::max();

  /// Process all the data scheduled for the given Class, optionally with
  /// a timeout. A false return value indicates that the operation has not
  /// completed and a later call is required.
  // MT: Externally Synchronized
  void run();
  bool run(timeout_t);

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
    Module::ud_t::typed_member_t<Classification> classification;

    struct {
      File::ud_t::typed_member_t<unsigned int> file;
      const auto& operator()(File::ud_t&) const noexcept { return file; }
      Context::ud_t::typed_member_t<unsigned int> context;
      const auto& operator()(Context::ud_t&) const noexcept { return context; }
      Module::ud_t::typed_member_t<unsigned int> module;
      const auto& operator()(Module::ud_t&) const noexcept { return module; }
      Metric::ud_t::typed_member_t<std::pair<unsigned int,unsigned int>> metric;
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
    std::vector<Thread::Temporary> threads;
  };

public:
  /// Registration structure for a PipelineSource. All access to a Pipeline from
  /// a Source must come through this structure.
  class Source final {
  public:
    Source();
    Source(Source&&) = default;
    ~Source() = default;

    /// Access the Extension userdata members.
    // MT: Safe (const)
    const decltype(Extensions::classification)& classification() const;
    const decltype(Extensions::identifier)& identifier() const;
    const decltype(Extensions::resolvedPath)& resolvedPath() const;

    /// Get the limits on this Source's emissions.
    DataClass limit() const noexcept { return dataLimit & pipe->scheduled; }

    /// Emit some new attributes into the Pipeline.
    /// DataClass: `attributes`
    // MT: Externally Synchronized (this), Internally Synchronized
    void attributes(const ProfileAttributes&);

    /// Emit a new Module into the Pipeline, returning the canonical copy.
    /// DataClass: `references`
    // MT: Externally Synchronized (this), Internally Synchronized
    Module& module(const stdshim::filesystem::path&);

    /// Emit a new File into the Pipeline, returning the canonical copy.
    /// DataClass: `references`
    // MT: Externally Synchronized (this), Internally Synchronized
    File& file(const stdshim::filesystem::path&);

    /// Emit a new Metric into the Pipeline.
    /// DataClass: `attributes`
    // MT: Externally Synchronized (this), Internally Synchronized
    Metric& metric(const std::string&, const std::string&, Metric::Type);

    /// Emit a new Context into the Pipeline.
    /// DataClass: `contexts`
    // MT: Externally Synchronized (this), Internally Synchronized
    Context& context(const Scope&);
    Context& context(Context&, const Scope&);

    /// Emit a new Thread into the Pipeline.
    /// DataClass: `threads`
    // MT: Externally Synchronized (this), Internally Synchronized
    Thread::Temporary& thread(const ThreadAttributes&);

    /// Emit a timepoint into the Pipeline. Overloads allow for less data.
    /// DataClass: `timepoints`
    // MT: Externally Synchronized (this), Internally Synchronized
    void timepoint(Thread::Temporary&, Context&, std::chrono::nanoseconds);
    void timepoint(Thread::Temporary&, std::chrono::nanoseconds);
    void timepoint(Context&, std::chrono::nanoseconds);
    void timepoint(std::chrono::nanoseconds);

    /// Emit some Thread-local metric data into the Pipeline.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    void add(Context&, Thread::Temporary&, const Metric&, double v);

    /// Emit some unlocalized accumulator data into the Pipeline.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    void add(Context&, const Metric&, std::pair<double, double>);

    /// Emit some uncontextualized metric data into the Pipeline.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    void add(Thread::Temporary&, const Metric&, double v);

    /// Emit some fully ambiguous metric data into the Pipeline.
    /// DataClass: `metrics`
    // MT: Externally Synchronized (this), Internally Synchronized
    void add(const Metric&, std::pair<double, double>);

    // Disable copy-assignment, and allow move assignment.
    Source& operator=(const Source&) = delete;
    Source& operator=(Source&&);

  private:
    friend class ProfilePipeline;
    Source(ProfilePipeline&, const DataClass&, const ExtensionClass&);
    Source(ProfilePipeline&, const DataClass&, const ExtensionClass&, SourceLocal&);
    Source(ProfilePipeline&, const DataClass&, const ExtensionClass&, std::size_t);
    ProfilePipeline* pipe;
    SourceLocal* slocal;
    DataClass dataLimit;
    ExtensionClass extensionLimit;
    std::size_t tskip;
  };

  /// Registration structure for a PipelineSink. All access to a Pipeline from
  /// a Sink must come through this structure.
  class Sink final {
  public:
    Sink();
    ~Sink() = default;

    /// Access the Extensions available within the Pipeline.
    const decltype(Extensions::classification)& classification() const;
    const decltype(Extensions::identifier)& identifier() const;
    const decltype(Extensions::resolvedPath)& resolvedPath() const;

    /// Allow registration of Userdata for Sinks.
    Structs& structs() { return pipe->structs; }

    /// Access to the canonical copies of the data within the Pipeline. Can only
    /// be used after the write() barrier.
    const ProfileAttributes& attributes();
    const util::locked_unordered_uniqued_set<Module>& modules();
    const util::locked_unordered_uniqued_set<File>& files();
    const util::locked_unordered_uniqued_set<Metric>& metrics();
    const Context& contexts();
    const util::locked_unordered_set<std::unique_ptr<Thread>>& threads();

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
  };

private:
  // Scheduled data transfer. Minimal requested and available set.
  DataClass scheduled;
  // Scheduled early wavefronts. Minimal requested and available set.
  DataClass scheduledWaves;
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

  // Sinks sorted by which early waves they want
  struct {
    std::vector<std::reference_wrapper<ProfileSink>> attributes;
    std::vector<std::reference_wrapper<ProfileSink>> references;
    std::vector<std::reference_wrapper<ProfileSink>> contexts;
  } sinkwaves;

  // Storage for the pointers to the SourceLocals.
  std::vector<SourceLocal> sourceLocals;

  // Userdata structures for the various bits. Must be above the data itself.
  Structs structs;
  Extensions uds;
  Context::met_t::struct_t metstruct;
  Thread::met_t::struct_t tmetstruct;

  // Storage for the canonical copies of everything. Because keeping track of
  // reference counts is a downright pain.
  std::mutex attrsLock;
  ProfileAttributes attrs;
  util::locked_unordered_set<std::unique_ptr<Thread>> threads;
  util::locked_unordered_uniqued_set<Module> mods;
  util::locked_unordered_uniqued_set<File> files;
  util::locked_unordered_uniqued_set<Metric> mets;
  std::unique_ptr<Context> cct;
};

}

#endif  // HPCTOOLKIT_PROFILE_PIPELINE_H
