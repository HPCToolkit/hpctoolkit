// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FINALIZER_H
#define HPCTOOLKIT_PROFILE_FINALIZER_H

#include "pipeline.hpp"
#include "dataclass.hpp"

#include "util/ref_wrappers.hpp"

#include "stdshim/filesystem.hpp"
#include <atomic>
#include <optional>

namespace hpctoolkit {

/// Finalizers expand on the data structures after the merge done by the
/// Pipeline. Since they represent a sort of "middling" step of the Pipeline,
/// their actual operations are highly limited.
class ProfileFinalizer {
public:
  virtual ~ProfileFinalizer() = default;

  /// Bind this Finalizer to a Pipeline to actually get stuff done.
  // MT: Externally Synchronized
  void bindPipeline(ProfilePipeline::Source&&) noexcept;

  /// Notify the Finalizer that a Pipeline has been bound, to register any userdata.
  // MT: Externally Synchronized
  virtual void notifyPipeline() noexcept;

  /// Query for the ExtensionClass this Finalizer provides to the Pipeline.
  // MT: Safe (const)
  virtual ExtensionClass provides() const noexcept = 0;

  /// Query for the ExtensionClass this Finalizer will depend on.
  /// Cannot overlap with the result from provides().
  // MT: Safe (const)
  virtual ExtensionClass requirements() const noexcept = 0;

  /// Generate an ID to the given object. Must be unique among all objects of the
  /// same type, and preferably dense towards 0.
  /// ExtensionClass: `identifier`
  // MT: Internally Synchronized
  virtual std::optional<unsigned int> identify(const Module&) noexcept;
  virtual std::optional<unsigned int> identify(const File&) noexcept;
  virtual std::optional<Metric::Identifier> identify(const Metric&) noexcept;
  virtual std::optional<unsigned int> identify(const Context&) noexcept;
  virtual std::optional<unsigned int> identify(const Thread&) noexcept;

  /// Resolve the path for the given object.
  /// ExtensionClass: `resolvedPath`
  // MT: Internally Synchronized
  virtual std::optional<stdshim::filesystem::path> resolvePath(const File&) noexcept;
  virtual std::optional<stdshim::filesystem::path> resolvePath(const Module&) noexcept;

  /// Generate the parent Context for a particular NestedScope, if possible by
  /// this Finalizer.
  ///
  /// Two Contexts are returned, the first refers to the relation while the
  /// second refers to the parent Context of the flat Scope. See
  /// ProfilePipeline::Source::context for more details, note that `.second`
  /// here is the *parent* of `.second` over there.
  ///
  /// If the first Context is omitted, the newly created child Context will be
  /// used to refer to the relation.
  ///
  /// ExtensionClass: `classification`
  // MT: Internally Synchronized
  virtual std::optional<std::pair<util::optional_ref<Context>, Context&>>
  classify(Context& ancestor, NestedScope&) noexcept;

  /// Fill a ContextFlowGraph with the appropriate data. Returns true if this
  /// Finalizer provided this data.
  /// ExtensionClass: `classification`
  // MT: Internally Synchronized
  virtual bool resolve(ContextFlowGraph&) noexcept;

  /// Fill a Metric will the requested Statistics. Unlike other methods, this
  /// one is called for all Finalizers.
  /// ExtensionClass: `statistics`
  // MT: Internally Synchronized
  virtual void appendStatistics(const Metric&, Metric::StatsAccess) noexcept;

protected:
  // This is a base class, don't construct it directly.
  ProfileFinalizer() = default;

  ProfilePipeline::Source sink;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZER_H
