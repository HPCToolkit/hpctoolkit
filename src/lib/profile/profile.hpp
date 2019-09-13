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

#ifndef HPCTOOLKIT_PROFILE_PROFILE_H
#define HPCTOOLKIT_PROFILE_PROFILE_H

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

class MetricSource;
class ProfileSink;
class InlineTransformer;
class Finalizer;

class PipelineBuilder;

/// Monolithic Pipeline for processing Profile data. Effectively connects
/// MetricSources to ProfileSinks, performs mediation of required data, and
/// handles the buffers when a Sink is not ready to receive data yet.
//
// I wanted to call it a Blender or Alembic or something cool like that, but
// Pipeline should work for now.
class ProfilePipeline {
public:
  ~ProfilePipeline() = default;

  /// Type to use for timeouts. Because sometimes we just don't like waiting.
  using timeout_t = std::chrono::nanoseconds;
  static constexpr timeout_t timeout_forever = timeout_t::max();

  // External Interface, for users:

  /// Constructor. Creates a Pipeline from a Builder.
  ProfilePipeline(PipelineBuilder&&, std::size_t);

  /// Bind a new Transformer to this Pipeline. These can be bound at any time,
  /// however they will not trigger on anything that has already "entered" the
  /// Pipe before binding.
  // MT: Externally Synchronized
  ProfilePipeline& operator<<(InlineTransformer&);
  ProfilePipeline& operator<<(std::unique_ptr<InlineTransformer>&&);

  /// Query whether any of a particular Class of data will ever pass through the
  /// Pipeline. Since the Pipeline is inherently lazy, if no Sink requires a
  /// particular Class of data it will never be requested from the Sources.
  /// This should be checked before pull()'ing any data into the Pipe.
  /// All Sinks should be bound previously.
  // MT: Externally Synchronized
  bool scheduled(DataClass::singleton_t) const noexcept;

  /// Pull some data from the Sources into the Pipe for modification. If for any
  /// of the elements of `dclass` schedule() returns false, behaviour is largely
  /// undefined (but probably bad). Returns true if the pull was successful in
  /// the given timeframe.
  /// Access to the stuffed data is given via stuffed().
  // MT: Externally Synchronized
  bool pull(const DataClass& dclass, timeout_t = timeout_forever);

  /// Access data that has been pull'ed into the Pipe, and has not yet been
  /// drain'ed.
  // MT: Externally Synchronized
  ProfileAttributes& stuffed(DataClass::attributes_t);
  std::pair<
    internal::locked_unordered_uniqued_set<Module>&,
    internal::locked_unordered_uniqued_set<File>&
  > stuffed(DataClass::references_t);
  internal::locked_unordered_uniqued_set<Metric>& stuffed(DataClass::metrics_t);
  Context& stuffed(DataClass::contexts_t);

  /// Prevent certain data from entering the Pipe until requested by a
  /// corrosponding pull() or drain().
  // MT: Externally Synchronized
  void cork(const DataClass&) {};

  /// Drain the Pipeline, completing processing of the data. If given, only data
  /// belonging to a particular Class will be drained, leaving the rest for a
  /// future call. Returns true if the data could be drained in the given
  /// timeframe.
  // MT: Externally Synchronized
  bool drain(timeout_t t = timeout_forever) { return drain(DataClass::all(), t); }
  bool drain(const DataClass&, timeout_t = timeout_forever);

  // Sinks are given userdata for better handling the data thrown at them.
  struct Structs {
    File::ud_t::struct_t file;
    Context::ud_t::struct_t context;
    Function::ud_t::struct_t function;
    Module::ud_t::struct_t module;
    Metric::ud_t::struct_t metric;
    Thread::ud_t::struct_t thread;
  };

  // Members for the various extension userdata available.
  struct ExtUserdata {
    File::ud_t::typed_member_t<unsigned int> id_file;
    Context::ud_t::typed_member_t<unsigned int> id_context;
    Function::ud_t::typed_member_t<unsigned int> id_function;
    Module::ud_t::typed_member_t<unsigned int> id_module;
    Metric::ud_t::typed_member_t<std::pair<unsigned int,unsigned int>> id_metric;
    Thread::ud_t::typed_member_t<unsigned int> id_thread;

    Module::ud_t::typed_member_t<Classification> classification;
  };

  /// Specialized interface for Sources. Provided while bind()'ing.
  class SourceEnd {
  public:
    SourceEnd();
    SourceEnd(SourceEnd&&);
    ~SourceEnd() = default;

    /// Access the Extension userdata members.
    // MT: Safe (const)
    const ExtUserdata& extensions() const { return pipe->uds; }

    /// Emit some new attributes into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    void attributes(const ProfileAttributes&);

    /// Emit a new Module into the Pipeline, returning the canonical copy.
    // MT: Externally Synchronized (this), Internally Synchronized
    Module& module(const stdshim::filesystem::path&);

    /// Emit a new Function into the Pipeline. Note that this function should be
    /// marked as complete (with functionComplete) at some point.
    // MT: Externally Synchronized (this), Internally Synchronized
    Function& function(const Module&);

    /// Mark a Function as complete, allowing it to flow further through the
    /// Pipe.
    // MT: Externally Synchronized (this), Internally Synchronized
    void functionComplete(const Module&, Function&);

    /// Emit a new File into the Pipeline, returning the canonical copy.
    // MT: Externally Synchronized (this), Internally Synchronized
    File& file(const stdshim::filesystem::path&);

    /// Emit a new Metric into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    Metric& metric(const std::string&, const std::string&);

    /// Emit a new Context into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    Context& context(const Scope&);
    Context& context(Context&, const Scope&);
    Context& context(Context* p, const Scope& s) {
      return p == nullptr ? context(s) : context(*p, s);
    }

    /// Emit a new Thread into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    Thread& thread(const ThreadAttributes&);

    /// Emit some Thread-local metric data into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    void add(Context&, const Thread&, const Metric&, double v);

    /// Emit some unlocalized metric data into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    void add(Context&, const Metric&, double v);

    /// Helper class for emitting trace data. It should actually be called
    /// TraceSegment, but I'll change all the names eventually.
    class Trace {
    public:
      Trace() : pipe(nullptr), td(nullptr), id(0) {};
      Trace(const Trace&) = delete;
      Trace(Trace&& o) : pipe(o.pipe), td(o.td), id(o.id) { o.td = nullptr; }
      ~Trace() { reset(); }

      Trace& operator=(const Trace&) = delete;
      Trace& operator=(Trace&& o) {
        reset();
        pipe = o.pipe;
        td = o.td;
        id = o.id;
        o.td = nullptr;
        return *this;
      }

      operator bool() const noexcept { return td != nullptr; }
      void reset();

      /// Emit a timepoint for a particular trace segment.
      // MT: Externally Synchronized (this), Internally Synchronized
      void trace(std::chrono::nanoseconds, const Context&);

    private:
      friend class SourceEnd;
      Trace(ProfilePipeline& p, Thread& t, unsigned long i)
        : pipe(&p), td(&t), id(i) {};
      ProfilePipeline* pipe;
      Thread* td;
      unsigned long id;
    };

    /// Emit a new trace segment into the Pipeline.
    // MT: Externally Synchronized (this), Internally Synchronized
    Trace traceStart(Thread&, std::chrono::nanoseconds epoch);

    // Disable copy-assignment, and allow move assignment.
    SourceEnd& operator=(const SourceEnd&) = delete;
    SourceEnd& operator=(SourceEnd&&);

  private:
    friend class ProfilePipeline;
    SourceEnd(ProfilePipeline&);
    SourceEnd(ProfilePipeline&, std::size_t);
    ProfilePipeline* pipe;
    std::size_t tskip;
  };

  /// Get a SourceEnd for use outside of a MetricSource. Usually for inserting
  /// bits into the Pipe without a pull().
  // MT: Internally Synchronized
  SourceEnd source() { return SourceEnd(*this); }

  /// Specialized interface for Sinks. Provided while bind()'ing.
  class SinkEnd {
  public:
    SinkEnd();
    ~SinkEnd() = default;

    // NOTE: Interface highly WIP. At some point these will probably use a
    // callback-style of transferring the data.

    const ProfileAttributes& attributes() {
      return pipe->attrs;
    }

    const internal::locked_unordered_uniqued_set<Module>& modules() {
      return pipe->mods;
    }

    const internal::locked_unordered_uniqued_set<File>& files() {
      return pipe->files;
    }

    const internal::locked_unordered_uniqued_set<Metric>& metrics() {
      return pipe->mets;
    }

    const Context& contexts() {
      return *pipe->cct;
    }

    const internal::locked_unordered_set<std::unique_ptr<Thread>>& threads() {
      return pipe->threads;
    }

    const ExtUserdata& extensions() const { return pipe->uds; }
    Structs& structs() { return pipe->structs; }

    // Disable copy-assignment, and allow move assignment.
    SinkEnd& operator=(const SinkEnd&) = delete;
    SinkEnd& operator=(SinkEnd&&);

  private:
    friend class ProfilePipeline;
    SinkEnd(ProfilePipeline& p, std::size_t);
    ProfilePipeline* pipe;
    std::size_t idx;
  };

private:
  friend class SourceEnd;
  friend class SinkEnd;

  Function& newFunction(const Module& mod) {
    return *mod.m_funcs.emplace(new Function(structs.function, mod)).first;
  }

  void ctxAdd(Context& c, const Metric& m, double v) {
    m.add(c, v);
  }

  unsigned long threadId(Thread& t) {
    return t.next_segment.fetch_add(1, std::memory_order_relaxed);
  }

  // Userdata structures for the various bits. Up here for extra lifetime.
  Structs structs;
  ExtUserdata uds;
  Context::met_t::struct_t metstruct;

  // Pointers to all the Sources and Sinks.
  std::vector<MetricSource*> sources;
  std::vector<ProfileSink*> sinks;
  std::vector<InlineTransformer*> transformers;
  std::vector<Finalizer*> finalizers;

  // Storage for Sources and Sinks that we have been given the unique_ptrs for.
  std::vector<std::unique_ptr<MetricSource>> up_sources;
  std::vector<std::unique_ptr<ProfileSink>> up_sinks;
  std::vector<std::unique_ptr<InlineTransformer>> up_transformers;
  std::vector<std::unique_ptr<Finalizer>> up_finalizers;

  // Scheduled bin of Classes that will be passing through.
  DataClass all_scheduled;

  // Stashes for all the stuffed copies.
  ProfileAttributes attrs;
  internal::locked_unordered_set<std::unique_ptr<Thread>> threads;
  internal::locked_unordered_uniqued_set<Module> mods;
  internal::locked_unordered_uniqued_set<File> files;
  internal::locked_unordered_uniqued_set<Metric> mets;
  std::unique_ptr<Context> cct;

  // Size of the worker thread teams for doing things.
  std::size_t team_size;
};

/// Helper object to build ProfilePipelines. Because having all the possible
/// arguments ready and at hand is rather difficult.
class PipelineBuilder {
public:
  /// Constructor for an empty Builder.
  PipelineBuilder() = default;
  ~PipelineBuilder() = default;

  /// Add a new Source to the future Pipeline.
  PipelineBuilder& operator<<(MetricSource& s);
  PipelineBuilder& operator<<(std::unique_ptr<MetricSource>&& sp);

  /// Add a new Sink to the future Pipeline.
  PipelineBuilder& operator<<(ProfileSink& s);
  PipelineBuilder& operator<<(std::unique_ptr<ProfileSink>&& sp);

  /// Add a new Finalizer to the future Pipeline.
  PipelineBuilder& operator<<(Finalizer&);
  PipelineBuilder& operator<<(std::unique_ptr<Finalizer>&&);

  /// Add a new Transformer to the future Pipeline.
  PipelineBuilder& operator<<(InlineTransformer&);
  PipelineBuilder& operator<<(std::unique_ptr<InlineTransformer>&&);

private:
  friend class ProfilePipeline;

  ExtensionClass available;

  // Pointers to all the contained bits.
  std::vector<MetricSource*> sources;
  std::vector<ProfileSink*> sinks;
  std::vector<Finalizer*> finalizers;
  std::vector<InlineTransformer*> transformers;

  // Storage for bits that we have ownership of.
  std::vector<std::unique_ptr<MetricSource>> up_sources;
  std::vector<std::unique_ptr<ProfileSink>> up_sinks;
  std::vector<std::unique_ptr<Finalizer>> up_finalizers;
  std::vector<std::unique_ptr<InlineTransformer>> up_transformers;
};

}

#endif // HPCTOOLKIT_PROFILE_PROFILE_H
