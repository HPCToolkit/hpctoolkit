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

#ifndef HPCTOOLKIT_PROFILE_TRANSFORMER_H
#define HPCTOOLKIT_PROFILE_TRANSFORMER_H

#include "pipeline.hpp"

namespace hpctoolkit {

/// Inline Transformer of Profile data, triggered on-demand. Effectively allows
/// for translations or additions to outputs from ProfileSources during the
/// piping process, without requiring a full synchronization to do so.
class ProfileTransformer {
public:
  virtual ~ProfileTransformer() = default;

  /// Bind this Transformer to a Pipeline. By nature can only be bound once.
  // MT: Externally Synchronized
  void bindPipeline(ProfilePipeline::Source&& se) noexcept {
    sink = std::move(se);
  }

  /// Callback issued when a new Context is emitted into the Pipe.
  // MT: Internally Synchronized
  virtual ContextRef context(ContextRef c, Scope&) noexcept { return c; }

  /// Append additional Statistics to the given Metric.
  // MT: Internally Synchronized
  virtual void metric(const Metric&, Metric::StatsAccess) noexcept {};

protected:
  // Use a subclass to implement the bits.
  ProfileTransformer() = default;

  // Source of the Pipeline to use for inserting data: our sink.
  ProfilePipeline::Source sink;
};

/// Transformer for expanding routes for `point` Contexts.
struct RouteExpansionTransformer : public ProfileTransformer {
  RouteExpansionTransformer() = default;
  ~RouteExpansionTransformer() = default;

  ContextRef context(ContextRef cr, Scope& s) noexcept override {
    if(!std::holds_alternative<SuperpositionedContext>(cr)) {
      if(s.type() == Scope::Type::point) {
        auto mo = s.point_data();
        const auto& c = mo.first.userdata[sink.classification()];
        auto routes = c.getRoutes(mo.second);
        if(routes.empty()) return cr;
        if(routes.size() == 1) {
          for(const auto& s: routes.front()) cr = sink.context(cr, s);
          return cr;
        }

        std::vector<SuperpositionedContext::Target> paths;
        paths.reserve(routes.size());
        for(const auto& r: routes) {
          paths.push_back({{}, cr});
          for(const auto& s: r) {
            paths.back().target = sink.context(paths.back().target, s);
            paths.back().route.emplace_back(sink.context(cr, s, true));
          }
        }
        return sink.superposContext(cr, std::move(paths));
      }
    }
    return cr;
  }
};

/// Transformer for expanding `point` Contexts with Classification data.
struct ClassificationTransformer : public ProfileTransformer {
  ClassificationTransformer() = default;
  ~ClassificationTransformer() = default;

  ContextRef context(ContextRef c, Scope& s) noexcept override {
    if(s.type() == Scope::Type::point) {
      auto mo = s.point_data();
      const auto& cl = mo.first.userdata[sink.classification()];
      for(const auto& s: cl.getScopes(mo.second)) c = sink.context(c, s);
    }
    return c;
  }
};

}

#endif  // HPCTOOLKIT_PROFILE_TRANSFORMER_H
