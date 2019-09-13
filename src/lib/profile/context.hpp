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

#ifndef HPCTOOLKIT_PROFILE_CONTEXT_H
#define HPCTOOLKIT_PROFILE_CONTEXT_H

#include "util/locked_unordered.hpp"
#include "util/atomic_unordered.hpp"
#include "scope.hpp"
#include "util/ragged_vector.hpp"

namespace hpctoolkit {

class Metric;

// An element of Metric data that can be located at a particular Context. Also
// known as a Datum.
// At the moment always includes both inclusive and exclusive.
class MetricDatum {
public:
  MetricDatum() : v_int_ex(0), v_int_in(0) {};
  ~MetricDatum() = default;

  /// Addition of exclusive data. Adds to both inclusive and exclusive.
  // MT: Internally Synchronized
  MetricDatum& operator+=(const double& v) noexcept;

  /// Propagation of inclusive data. Adds to just inclusive.
  // MT: Internally Synchronized
  MetricDatum& operator<<=(const double& v) noexcept;

  /// Addition of another datum. Adds component-wise.
  // MT: Internally Synchronized
  MetricDatum& operator+=(const MetricDatum&) noexcept;

  /// Check whether a value can always be dropped silently.
  static constexpr bool empty(double v) noexcept { return v == 0; }

  // Get the internal values, for now.
  std::pair<double, double> get() const noexcept {
    return {v_int_ex.load(std::memory_order_relaxed),
            v_int_in.load(std::memory_order_relaxed)};
  }

private:
  std::atomic<double> v_int_ex;
  std::atomic<double> v_int_in;
};

// A single calling Context.
class Context {
public:
  using ud_t = internal::ragged_vector<const Context&>;
  using met_t = internal::ragged_map<>;

  Context(ud_t::struct_t& rs, met_t::struct_t& ms) : userdata(rs, std::ref(*this)), data(ms) {};
  Context(ud_t::struct_t& rs, met_t::struct_t& ms, const Scope& l) : Context(rs, ms, nullptr, l) {};
  Context(ud_t::struct_t& rs, met_t::struct_t& ms, Scope&& l) : Context(rs, ms, nullptr, l) {};
  ~Context() = default;

private:
  using children_t = internal::locked_unordered_uniqued_set<Context>;

public:
  enum class iteration { pre, post, leaf };

  /// Iterate over all the decendant Contexts.
  // MT: Externally Synchronized
  void iterate(const std::function<void(const Context&, iteration)>& f) const;

  /// Access to the children of this Context.
  // MT: Externally Synchronized
  children_t& children() const noexcept { return *children_p; }

  /// The direct parent of this Context.
  Context* direct_parent() const { return u_parent; }

  /// The Scope that this Context represents in the tree.
  const Scope& scope() const { return u_scope; }

  mutable ud_t userdata;

  /// Metric data for this particular Context.
  met_t data;

private:
  std::unique_ptr<children_t> children_p;

  Context(ud_t::struct_t& rs, met_t::struct_t& ms, Context* p, const Scope& l)
    : Context(rs, ms, p, Scope(l)) {};
  Context(ud_t::struct_t&m, met_t::struct_t&, Context*, Scope&&);
  Context(Context&& c);

  friend class ProfilePipeline;

  /// Get the child Context for a given Scope, creating one if none exists.
  // MT: Internally Synchronized
  std::pair<Context&,bool> ensure(Scope&&);
  template<class... Args> std::pair<Context&,bool> ensure(Args&&... args) {
    return ensure(Scope(std::forward<Args>(args)...));
  }

  internal::uniqable_key<Context*> u_parent;
  internal::uniqable_key<Scope> u_scope;

  friend class internal::uniqued<Context>;
  internal::uniqable_key<Scope>& uniqable_key() { return u_scope; }
};
}

#endif // HPCTOOLKIT_PROFILE_CONTEXT_H
