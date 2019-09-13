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
// Copyright ((c)) 2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_LAMBDA_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_LAMBDA_H

#include "../finalizer.hpp"

namespace hpctoolkit::finalizers {

class Lambda final : public Finalizer {
public:
  using moda_t = std::function<void(ProfilePipeline::SourceEnd&, const Module&, Classification&)>;
  using modb_t = std::function<void(ProfilePipeline::SourceEnd&, const Module&, unsigned int&)>;
  using file_t = std::function<void(ProfilePipeline::SourceEnd&, const File&, unsigned int&)>;
  using func_t = std::function<void(ProfilePipeline::SourceEnd&, const Module&, const Function&, unsigned int&)>;
  using met_t = std::function<void(ProfilePipeline::SourceEnd&, const Metric&, std::pair<unsigned int, unsigned int>&)>;
  using ctx_t = std::function<void(ProfilePipeline::SourceEnd&, const Context&, unsigned int&)>;
  using thread_t = std::function<void(ProfilePipeline::SourceEnd&, const Thread&, unsigned int&)>;

  Lambda(const moda_t& f) : pro(ExtensionClass::classification), f_moda(f) {};
  Lambda(moda_t&& f) : pro(ExtensionClass::classification), f_moda(std::move(f)) {};
  Lambda(const modb_t& f) : pro(ExtensionClass::identifier), f_modb(f) {};
  Lambda(modb_t&& f) : pro(ExtensionClass::identifier), f_modb(std::move(f)) {};
  Lambda(const file_t& f) : pro(ExtensionClass::identifier), f_file(f) {};
  Lambda(file_t&& f) : pro(ExtensionClass::identifier), f_file(std::move(f)) {};
  Lambda(const func_t& f) : pro(ExtensionClass::identifier), f_func(f) {};
  Lambda(func_t&& f) : pro(ExtensionClass::identifier), f_func(std::move(f)) {};
  Lambda(const met_t& f) : pro(ExtensionClass::identifier), f_met(f) {};
  Lambda(met_t&& f) : pro(ExtensionClass::identifier), f_met(std::move(f)) {};
  Lambda(const ctx_t& f) : pro(ExtensionClass::identifier), f_ctx(f) {};
  Lambda(ctx_t&& f) : pro(ExtensionClass::identifier), f_ctx(std::move(f)) {};
  Lambda(const thread_t& f) : pro(ExtensionClass::identifier), f_thread(f) {};
  Lambda(thread_t&& f) : pro(ExtensionClass::identifier), f_thread(std::move(f)) {};

  ExtensionClass provides() const noexcept { return pro; }

  void module(const Module& m, Classification& c) { if(f_moda) f_moda(sink, m, c); }
  void module(const Module& m, unsigned int& i) { if(f_modb) f_modb(sink, m, i); }
  void file(const File& f, unsigned int& i) { if(f_file) f_file(sink, f, i); }
  void function(const Module& m, const Function& f, unsigned int& i) { if(f_func) f_func(sink, m, f, i); }
  void metric(const Metric& m, std::pair<unsigned int, unsigned int>& ip) { if(f_met) f_met(sink, m, ip); }
  void context(const Context& c, unsigned int& i) { if(f_ctx) f_ctx(sink, c, i); }
  void thread(const Thread& t, unsigned int& i) { if(f_thread) f_thread(sink, t, i); }

private:
  ExtensionClass pro;
  moda_t f_moda;
  modb_t f_modb;
  file_t f_file;
  func_t f_func;
  met_t f_met;
  ctx_t f_ctx;
  thread_t f_thread;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_LAMBDA_H
