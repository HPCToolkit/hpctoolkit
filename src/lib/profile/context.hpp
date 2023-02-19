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
// Copyright ((c)) 2002-2023, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_CONTEXT_H
#define HPCTOOLKIT_PROFILE_CONTEXT_H

#include "accumulators.hpp"
#include "attributes.hpp"

#include "util/locked_unordered.hpp"
#include "scope.hpp"
#include "util/ragged_vector.hpp"
#include "util/ref_wrappers.hpp"
#include "util/uniqable.hpp"

#include <deque>
#include <functional>
#include <unordered_set>

namespace hpctoolkit {

/// Calling Context representing a unique nested Scope. See Scope::Type for more
/// details on how nested Scopes are interpreted. The root of the Context tree
/// is always a (global) Scope.
///
/// Note that calling Context is more than just the caller, it also includes
/// lexical Contexts such as functions, lines and loop constructs.
///
/// There is always at most one Context for every sequence of nested Scopes
/// from the (global) root to any individual Context.
class Context {
public:
  using ud_t = util::ragged_vector<const Context&>;

  // Contexts can only be created by the ProfilePipeline
  Context() = delete;
  ~Context() noexcept;

private:
  using children_t = util::locked_unordered_uniqued_set<Context>;
  using reconsts_t = util::locked_unordered_uniqued_set<ContextReconstruction>;

public:
  /// List the Context children of this Context.
  // MT: Safe (const)
  const children_t& children() const noexcept { return *children_p; }

  /// Parent Context, or std::nullopt if this Context does not have one.
  // MT: Safe (const)
  util::optional_ref<Context> direct_parent() noexcept { return m_parent; }
  util::optional_ref<const Context> direct_parent() const noexcept { return m_parent; }

  /// The full NestedScope that this Context represents.
  // MT: Safe (const)
  const NestedScope& scope() const noexcept { return u_scope(); }

  /// Userdata storage and access.
  // MT: See ragged_vector.
  mutable ud_t userdata;

  /// Access this Context's per-Context data.
  // MT: Internally Synchronized, Unstable (before `metrics` wavefront)
  const auto& data() const noexcept { return m_data; }
  auto& data() noexcept { return m_data; }

  /// Iterate over the Context sub-tree rooted at this Context. The given
  /// functions are called before and after every Context.
  // MT: Safe (const), Unstable (before `contexts` wavefront)
  void iterate(const std::function<void(Context&)>& pre,
               const std::function<void(Context&)>& post);
  void citerate(const std::function<void(const Context&)>& pre,
                const std::function<void(const Context&)>& post) const;

private:
  std::unique_ptr<children_t> children_p;
  std::unique_ptr<reconsts_t> reconsts_p;

  Context(ud_t::struct_t&, util::optional_ref<Context>, NestedScope);
  Context(Context&& c);

  friend class PerThreadTemporary;
  PerContextAccumulators m_data;

  friend class ProfilePipeline;
  /// Get the child Context for a given Scope, creating one if none exists.
  /// Returns true if the Context was created by this call.
  // MT: Internally Synchronized
  std::pair<Context&, bool> ensure(NestedScope);

  const util::optional_ref<Context> m_parent;
  util::uniqable_key<NestedScope> u_scope;

  friend class util::uniqued<Context>;
  util::uniqable_key<NestedScope>& uniqable_key() { return u_scope; }
};

/// Reconstruction of a potentially missing sequence of calling Contexts.
///
/// In the most basic case the full calling Context is available for a sample.
/// So if instruction A calls function F and in there instruction X calls to
/// instruction B, the resulting chain of Contexts is:
///     ... -> A -> F - X -> B
/// And similarly, with more in-between callers:
///     ... -> A -> F - X -> G - Y -> B
///
/// Sometimes though we don't know X or Y, since B is a sample that can't be
/// unwound (currently all samples in GPU code). If we can predict when this
/// happens we can still record data on A, thus we have:
///     ... -> A -> F - ...*something*... -> B
/// The purpose of a ContextReconstruction is to fill in that "something" using
/// static binary analysis, in short the static control flow (or call) graph.
/// A shared ContextFlowGraph is referenced to store the bits needed.
///
/// The complication is that there might be multiple paths through the Graph
/// which are a valid "something." The measurements aren't able to distinguish
/// between them, so instead we "redistribute" the metric values attributed to
/// the Reconstruction among the possibilities. Something like:
///     ... -> A -> F - X -> B  += 0.556 * metric values
///     ... -> A -> F - Y -> B  += 0.377 * metric values
///     ... -> A -> G - Z -> B  += 0.067 * metric values
/// Since `... -> A` is a prefix to this Context sub-tree, it is called the
/// "root" of the Reconstruction. The generation of these factors is complex and
/// given in more detail in ContextFlowGraph::Template, but in this case they
/// are generated entirely only from the metric values of the "entry point(s)"
/// `... -> A -> (F|G)` and of sibling Reconstructions for X, Y and Z.
///
/// Note that this is fundamentally different from:
///     ... -> A -> (unknown) -> B
/// where we don't know F or aren't confident enough to fill in the "something"
/// (either because of missing data or the potential of cross-Module calls).
/// In this latter case there is little more that can be said past (unknown).
class ContextReconstruction final {
public:
  // ContextReconstructions must be created by a Context
  ContextReconstruction() = delete;
  ~ContextReconstruction() noexcept;

  ContextReconstruction(const ContextReconstruction&) = delete;
  ContextReconstruction(ContextReconstruction&&);
  ContextReconstruction& operator=(const ContextReconstruction&) = delete;
  ContextReconstruction& operator=(ContextReconstruction&&) = delete;

  /// ContextFlowGraph containing the relevant binary analysis results for this
  /// Reconstruction.
  // MT: Safe (const)
  ContextFlowGraph& graph() noexcept { return u_graph().get(); }
  const ContextFlowGraph& graph() const noexcept { return u_graph().get(); }

  /// Root of the reconstructed Context sub-tree. All Contexts generated as part
  /// of this Reconstruction are children of this Context.
  Context& root() noexcept { return m_root; }
  const Context& root() const noexcept { return m_root; }

private:
  friend class ContextFlowGraph;
  util::uniqable_key<util::reference_index<ContextFlowGraph>> u_graph;
  Context& m_root;
  util::Once m_instantiated;

  /// Contexts for the entry Scopes. Metric values attributed to these are used
  /// to calculate the exterior factors.
  std::unordered_map<Scope, Context&> m_entries;

  /// Sibling Reconstructions for the interior Scopes. Metric values attributed
  /// to these are used to calculate the interior factors.
  std::unordered_map<Scope, ContextReconstruction&> m_siblings;

  /// Final Contexts metric values get re-attributed to. Indexed by Template.
  std::vector<std::reference_wrapper<Context>> m_finals;

  friend class PerThreadTemporary;
  /// From the given data, calculate the rescaling factors for each Template
  /// within the main FlowGraph, as instantiated here.
  ///
  /// Also determine which Templates have entry calls at all, for interiorFactors.
  // MT: Safe (const)
  std::pair<std::vector<double>, std::vector<bool>> rescalingFactors(
    const util::locked_unordered_map<util::reference_index<const Context>,
      util::locked_unordered_map<util::reference_index<const Metric>,
        MetricAccumulator>>&) const;

  /// Variant that allows for STL maps instead of the locked wrappers.
  // MT: Safe (const)
  std::vector<double> rescalingFactors(
    const std::unordered_map<util::reference_index<const Context>,
      std::unordered_map<util::reference_index<const Metric>,
        double>>&) const;

  /// Internal implementation template for rescalingFactors.
  // MT: Safe (const)
  template<class T, class F1, class F2, class F3>
  std::pair<std::vector<double>, std::vector<bool>> rescalingFactors_impl(
      const F1&, const F2&, const F3&) const;

  /// From the given data, calculate the interior factors for each Template
  /// within the main FlowGraph, as instantiated here.
  // MT: Safe (const)
  std::vector<double> interiorFactors(
    const util::locked_unordered_map<util::reference_index<const ContextReconstruction>,
      util::locked_unordered_map<util::reference_index<const Metric>,
        MetricAccumulator>>&, const std::vector<bool>&) const;

  friend class ProfilePipeline;
  ContextReconstruction(Context&, ContextFlowGraph&);
  void instantiate(const std::function<Context&(Context&, const Scope&)>&,
                   const std::function<ContextReconstruction&(const Scope&)>&);

  friend class util::uniqued<ContextReconstruction>;
  auto& uniqable_key() { return u_graph; }
};

/// Binary analysis results for reconstructing missing calling Contexts.
///
/// In the most basic case the full calling Context is available for a sample.
/// So if instruction A calls function F and in there instruction X calls to
/// instruction B, the resulting chain of Contexts is:
///     ... -> A -> F - X -> B
/// And similarly, with more in-between callers:
///     ... -> A -> F - X -> G - Y -> B
///
/// Sometimes we don't know X or Y, this is the case handled by
/// ContextReconstruction. ContextFlowGraph handles the slightly more general
/// case where all we know is:
///     ... -> A -> F
/// and:
///     ...*something*... -> B
/// In short, we have samples for B with no calling Context, but we do know that
/// A called F which may have been a caller for B. We then reconstruct the
/// missing Context sub-tree under A and redistribute metric values attributed
/// to the FlowGraph itself.
///
/// If there are multiple such A's we need to redistribute based on the number
/// of calls to F from each A. In practice simple weighted distribution isn't
/// quite enough, the arguments to F may differ between roots (A's) and cause
/// significant performance differences that we want to preserve. The solution
/// is to add "reconstruction groups," FlowGraphs in the group only distribute
/// to roots also in their reconstruction groups. These groups are identifed by
/// 64-bit integers and can be thought of as a way to associate events grouped
/// in "time," rather than by calling context.
///
/// Note that the temporal association represented by reconstruction groups is
/// asymptotically more costly than normal profile data, Prof handles it as best
/// it can but it is up to hpcrun to not blow up the number of groups.
class ContextFlowGraph final {
public:
  // ContextFlowGraphs must be created by the ProfilePipeline
  ContextFlowGraph() = delete;
  ~ContextFlowGraph() noexcept;

  ContextFlowGraph(const ContextFlowGraph&) = delete;
  ContextFlowGraph(ContextFlowGraph&&);
  ContextFlowGraph& operator=(const ContextFlowGraph&) = delete;
  ContextFlowGraph& operator=(ContextFlowGraph&&) = delete;\

  /// Scope used to generate this FlowGraph. Metric values originate from
  /// samples taken at this Scope, and then are redistributed.
  // MT: Safe (const)
  Scope scope() const noexcept { return u_scope(); }

  /// One (of possibly many) valid "somethings" for the caller of a sample.
  /// From a Template (+ the Reconstruction root) a full calling Context can be
  /// generated, as follows:
  ///     root -> entry() - path()... -> scope()
  /// One way to visualize this is as a call Graph, where entry() is a function
  /// node with no incoming calls and path() is the series of call instructions
  /// on a path through the Graph until exiting at scope().
  ///
  /// Metric values attributed to a Reconstruction or FlowGraph are
  /// redistributed among the Templates, the exact proportion of the value
  /// re-attributed to any one Template's full calling Context is the product
  /// of the exterior, interior and rescaling factors. See the method
  /// documentations for how the first two are calculated, see
  /// MetricHandling::exteriorLogical for the rescaling factors.
  class Template final {
  public:
    Template(Scope entry, std::vector<Scope> path)
      : m_entry(entry), m_path(std::move(path)) {};

    Template(const Template&) = default;
    Template(Template&&) = default;
    Template& operator=(const Template&) = default;
    Template& operator=(Template&&) = default;

    /// Function used to "enter" the FlowGraph. Metric values attributed to
    /// `root -> entry()` are used to calculate the exterior factors, as:
    ///     calls(root -> entry()) / sum(calls(root' -> entry()) for root' in roots)
    /// where calls(c) is the number of (sampled) calls attributed to Context c.
    ///
    /// Note that this is simply 1 if there is a single root (e.g. in the case
    /// of ContextReconstruction). Note also that exterior factors are
    /// calculated per-reconstruction group.
    ///
    /// The Metric for calls has MetricHandling::exterior set.
    ///
    /// This Scope is never intentionally added to the full calling Context, so
    /// it can actually be anything the call Metric is attributed to.
    const Scope& entry() const noexcept { return m_entry; }

    /// Series of calls leading through the FlowGraph. Metric values attributed
    /// to `root -> path()[0...n]` are used to calculate the interior factors:
    ///     factor = 1
    ///     valid = set(t for t in templates() if pcalls(t.entry()) > 0)
    ///     for idx = n...0:
    ///       factor *= calls(path()[idx]) / sum(calls(q) for q in set(t.path()[idx] for t in siblings))
    ///       valid = set(t for t in valid if t.path()[idx] == p)
    /// where calls(x) is the number of calls attributed to the sibling
    /// Reconstruction or FlowGraph x, and pcalls(c) is the number of (sampled)
    /// "entry" calls attributed to Context c.
    ///
    /// Note that interior factors are calculated per-reconstruction group.
    /// Note also that no path() can be a suffix of another path() in the same
    /// FlowGraph.
    ///
    /// The Metric for calls has MetricHandling::interior set.
    ///
    /// These Scopes also serve as the callers in the full calling Context.
    const auto& path() const noexcept { return m_path; }

  private:
    Scope m_entry;
    std::vector<Scope> m_path;
  };

  /// Add a new Template to this FlowGraph.
  // MT: Externally Synchronized
  void add(Template);

  /// Access the sequence of all Templates added to this FlowGraph. As a side
  /// effect, waits for freeze() to be called (handled by the Pipeline).
  // MT: Internally Synchronized, Safe (const)
  const std::deque<Template>& templates() const noexcept;

  /// Handling instructions for the values associated with a particular Metric.
  struct MetricHandling {
    MetricHandling() = default;
    ~MetricHandling() = default;

    MetricHandling(const MetricHandling&) = default;
    MetricHandling(MetricHandling&&) = default;
    MetricHandling& operator=(const MetricHandling&) = default;
    MetricHandling& operator=(MetricHandling&&) = default;

    /// If true, this Metric's value should be used to determine exterior
    /// factors, see Template::entry() for details.
    ///
    /// If true, this Metric (may) represent the number of "physical" or
    /// "sampled" calls to an entry function, if there is not another metric
    /// with this set and exteriorLogical not set. Up to two Metrics may have
    /// this set if exteriorLogical differs.
    bool exterior = false;

    /// If true, this Metric's value should be used to determine rescaling
    /// factors.
    ///
    /// If true, this Metric represents the number of "logical" or "all" calls
    /// to an entry function. This can only be set on one Metric.
    ///
    /// The concept here is that not all calls to an entry function need be
    /// sampled ("physical"), we can presume all calls have roughly the same
    /// performance characteristics and just rescale the values to make up for
    /// the skipped calls. The rescaling factor for this is calculated as:
    ///     lcalls(root -> entry) / pcalls(root -> entry)
    /// where lcalls(c) is the number of "logical" calls to Context c and
    /// pcalls(c) is the number of "physical" calls, both summed across all
    /// reconstruction groups.
    ///
    /// Note that this calculation considers *all* calls root -> entry to have
    /// the same performance characteristics, rather than all calls within a
    /// reconstruction group.
    bool exteriorLogical = false;

    /// If true, this Metric's value should be used to determine interior
    /// factors, see Template::path() for details.
    ///
    /// If true, this Metric represents the (approximate) number of calls a
    /// call instruction makes. This can only be set on one Metric.
    bool interior = false;
  };

  /// Set a handler to generate MetricHandlings for Metrics. Must be set if
  /// any Templates were added.
  // MT: Externally Synchronized
  void handler(std::function<MetricHandling(const Metric&)>);

  /// Get the handler used to generate MetricHandlings. As a side effect, waits
  /// for freeze() to be called (handled by the Pipeline).
  // MT: Internally Synchronized, Safe (const)
  const std::function<MetricHandling(const Metric&)>& handler() const noexcept;

private:
  friend class ContextReconstruction;
  util::uniqable_key<Scope> u_scope;
  std::deque<Template> m_templates;
  Scope m_fallback;
  std::function<MetricHandling(const Metric&)> m_handler;
  util::Once m_frozen_once;

  /// Set of entry Scopes, for easy lookups.
  std::unordered_set<Scope> m_entries;

  /// Sibling FlowGraphs for the interior Scopes. Metric value attributed to
  /// these are used to calculate the interior factors.
  std::unordered_map<Scope, ContextFlowGraph&> m_siblings;

  friend class PerThreadTemporary;
  /// From the given data, calculate the exterior factors for each Template
  /// as instantiated within each Reconstruction. Also determine the Templates
  /// that have entry calls, for interiorFactors calculations.
  ///
  /// The metric values given are local to a reconstruction group. For
  /// efficiency, only the given set of Reconstructions will be considered.
  // MT: Safe (const)
  std::pair<
    std::unordered_map<util::reference_index<const ContextReconstruction>,
                       std::vector<double>>,
    std::vector<bool>
  > exteriorFactors(
    const std::unordered_set<
      util::reference_index<const ContextReconstruction>>& reconsts,
    const util::locked_unordered_map<util::reference_index<const Context>,
      util::locked_unordered_map<util::reference_index<const Metric>,
        MetricAccumulator>>&) const;

  /// From the given data, calculate the interior factors for each Template
  /// within the FlowGraph, using values attributed to top-level Scopes.
  ///
  /// The metric values given are local to a reconstruction group.
  // MT: Safe (const)
  std::vector<double> interiorFactors(
    const util::locked_unordered_map<util::reference_index<const ContextFlowGraph>,
      util::locked_unordered_map<util::reference_index<const Metric>,
        MetricAccumulator>>&, const std::vector<bool>&) const;

  /// Internal implementation template for interiorFactors.
  template<class T, class F1, class F2, class F3>
  std::vector<double> interiorFactors_impl(const F1&, const F2&, const F3&,
      const std::vector<bool>&) const;

  friend class ProfilePipeline;
  ContextFlowGraph(Scope);

  /// "Freeze" this FlowGraph and make it suitable for creating Reconstructions.
  /// After this none of the setters may be used.
  // MT: Externally Synchronized
  void freeze(const std::function<util::optional_ref<ContextFlowGraph>(const Scope&)>&);

  /// Wait for the "freeze" to complete and returns whether this FlowGraph is
  /// empty and thus not suitable for creating Reconstructions.
  // MT: Internally Synchronized
  bool empty() const;

  /// List all the entry points for this FlowGraph. Only available after the
  /// FlowGraph has been frozen.
  // MT: Safe (const)
  const std::unordered_set<Scope>& entries() const;

  friend class util::uniqued<ContextFlowGraph>;
  auto& uniqable_key() { return u_scope; }
};

}

#endif // HPCTOOLKIT_PROFILE_CONTEXT_H
