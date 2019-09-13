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

#include "profile.hpp"

namespace hpctoolkit {

class LambdaTransformer;

/// Inline Transformer of Profile data, triggered on-demand. Effectively allows
/// for translations or additions to outputs from MetricSources during the
/// piping process, without requiring a full synchronization to do so.
class InlineTransformer {
public:
  virtual ~InlineTransformer() = default;

  /// Bind this Transformer to a Pipeline. By nature can only be bound once.
  // MT: Externally Synchronized
  void bindPipeline(ProfilePipeline::SourceEnd&& se) { sink = std::move(se); }

  /// Callback issued when a new Module is emitted into the Pipe.
  // MT: Internally Synchronized
  virtual Module& module(Module& m) { return m; }

  /// Callback issued when a new File is emitted into the Pipe.
  // MT: Internally Synchronized
  virtual File& file(File& f) { return f; }

  /// Callback issued when a new Metric is emitted into the Pipe.
  // MT: Internally Synchronized
  virtual Metric& metric(Metric& m) { return m; }

  /// Callback issued when a new Context is emitted into the Pipe.
  // MT: Internally Synchronized
  virtual Context& context(Context& c, Scope&) { return c; }

  /// Convenience typedef for the Lambda implementation.
  using Lambda = LambdaTransformer;

protected:
  // Use a subclass to implement the bits.
  InlineTransformer() = default;

  // SourceEnd of the Pipeline to use for inserting data: our sink.
  ProfilePipeline::SourceEnd sink;
};

/// Convenience subclass for using lambdas as the methods.
struct LambdaTransformer : public InlineTransformer {
  LambdaTransformer() = default;
  ~LambdaTransformer() = default;

  std::function<Module&(ProfilePipeline::SourceEnd&, Module&)> f_module;
  Module& module(Module& m) { return f_module ? f_module(sink, m) : m; }
  void set(std::function<Module&(ProfilePipeline::SourceEnd&, Module&)>&& f) {
    f_module = std::move(f);
  }

  std::function<File&(ProfilePipeline::SourceEnd&, File&)> f_file;
  File& file(File& f) { return f_file ? f_file(sink, f) : f; }
  void set(std::function<File&(ProfilePipeline::SourceEnd&, File&)>&& f) {
    f_file = std::move(f);
  }

  std::function<Metric&(ProfilePipeline::SourceEnd&, Metric&)> f_metric;
  Metric& metric(Metric& m) { return f_metric ? f_metric(sink, m) : m; }
  void set(std::function<Metric&(ProfilePipeline::SourceEnd&, Metric&)>&& f) {
    f_metric = std::move(f);
  }

  std::function<Context&(ProfilePipeline::SourceEnd&, Context&, Scope&)> f_context;
  Context& context(Context& c, Scope& s) {
    return f_context ? f_context(sink, c, s) : c;
  }
  void set(std::function<Context&(ProfilePipeline::SourceEnd&, Context&, Scope&)>&& f) {
    f_context = std::move(f);
  }
};

/// Transformer for expanding `module` Contexts with Classification data.
struct ClassificationTransformer : public InlineTransformer {
  ClassificationTransformer() = default;
  ~ClassificationTransformer() = default;

  Context& context(Context& c, Scope& s) {
    const auto& cl = sink.extensions().classification;
    Context* p = &c;
    if(s.type() == Scope::Type::point) {
      auto mo = s.point_data();
      auto ss = mo.first.userdata[cl].getScopes(mo.second);
      for(auto it = ss.crbegin(); it != ss.crend(); ++it)
        p = &sink.context(*p, *it);
    }
    return *p;
  }
};

/// Transformer for merging `module` Contexts based on their line information.
struct LineMergeTransformer : public InlineTransformer {
  LineMergeTransformer() = default;
  ~LineMergeTransformer() = default;

  Context& context(Context& c, Scope& s) {
    const auto& cl = sink.extensions().classification;
    if(s.type() == Scope::Type::point) {
      auto mo = s.point_data();
      s = {mo.first, mo.first.userdata[cl].getCanonicalAddr(mo.second)};
    }
    return c;
  }
};

}

#endif  // HPCTOOLKIT_PROFILE_TRANSFORMER_H
