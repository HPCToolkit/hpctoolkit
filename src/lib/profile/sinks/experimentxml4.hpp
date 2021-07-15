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

#ifndef HPCTOOLKIT_PROFILE_SINKS_ExperimentXML4_H
#define HPCTOOLKIT_PROFILE_SINKS_ExperimentXML4_H

#include "../sink.hpp"

#include <atomic>
#include <fstream>
#include <cstdio>
#include <mutex>
#include "../stdshim/filesystem.hpp"

namespace hpctoolkit::sinks {

class HPCTraceDB2;

/// ProfileSink for the current experiment.xml format. Just so we can spit
/// something out.
class ExperimentXML4 final : public ProfileSink {
public:
  ~ExperimentXML4() = default;

  /// Constructor, with a reference to the output database directory.
  ExperimentXML4(const stdshim::filesystem::path&, bool, HPCTraceDB2*);

  /// Write out as much data as possible. See ProfileSink::write.
  void write() override;

  DataClass accepts() const noexcept override {
    using namespace hpctoolkit::literals::data;
    return attributes + references + contexts + metrics;
  }

  ExtensionClass requires() const noexcept override {
    using namespace hpctoolkit::literals::extensions;
    Class ret = classification + identifier + mscopeIdentifiers;
    if(include_sources) ret += resolvedPath;
    return ret;
  }

  void notifyPipeline() noexcept override;

private:
  stdshim::filesystem::path dir;
  std::ofstream of;
  std::atomic<unsigned int> next_id;
  HPCTraceDB2* tracedb;
  bool include_sources;

  struct uds;

  class udFile {
  public:
    udFile(ExperimentXML4&);
    udFile(ExperimentXML4&, const Module&);
    udFile(const File&, ExperimentXML4&);
    ~udFile() = default;

    void incr(ExperimentXML4&);
    operator bool() { return used.load(std::memory_order_relaxed); }

    std::string tag;
    unsigned int id;

  private:
    std::atomic<bool> used;
    const File* fl;
    const Module* m;
  };

  udFile file_unknown;

  class udMetric {
  public:
    udMetric(const Metric&, ExperimentXML4&);
    ~udMetric() = default;

    std::string metricdb_tags;
    std::string metric_tags;
    unsigned int maxId;
  };
  std::string eStatMetricTags(const ExtraStatistic&, unsigned int&);

  struct Proc {
    Proc() = default;
    Proc(unsigned int i) : id(i), tag(), done(false) {};
    Proc(const Proc& o) : id(o.id), tag(o.tag), done(false) {};

    unsigned int id;
    std::string tag;
    std::atomic<bool> done;

    void setTag(std::string n, std::size_t v, int f);
    bool prep();
  };

  std::atomic<unsigned int> next_procid;
  util::locked_unordered_map<Scope, Proc> procs;
  Proc& getProc(const Scope&);
  std::string tagProc(std::string, bool = false);

  util::OnceFlag proc_unknown_flag;
  Proc proc_unknown_proc;
  const Proc& proc_unknown();

  util::OnceFlag proc_partial_flag;
  Proc proc_partial_proc;
  const Proc& proc_partial();

  class udModule {
  public:
    udModule(const Module&, ExperimentXML4&);
    udModule(ExperimentXML4&);
    ~udModule() = default;

    void incr(const Module&, ExperimentXML4&);
    operator bool() { return used.load(std::memory_order_relaxed); }

    std::string tag;
    unsigned int id;
    udFile unknown_file;

  private:
    std::atomic<bool> used;
  };

  util::OnceFlag mod_unknown_flag;
  udModule unknown_module;

  std::atomic<unsigned int> next_cid;
  class udContext {
  public:
    udContext(const Context&, ExperimentXML4&);
    ~udContext() = default;

    std::string pre;
    std::string open;
    std::string attr;
    bool premetrics;
    std::string close;
    std::string post;
    bool partial;
  };

  struct {
    File::ud_t::typed_member_t<udFile> file;
    const auto& operator()(File::ud_t&) const noexcept { return file; }
    Context::ud_t::typed_member_t<udContext> context;
    const auto& operator()(Context::ud_t&) const noexcept { return context; }
    Module::ud_t::typed_member_t<udModule> module;
    const auto& operator()(Module::ud_t&) const noexcept { return module; }
    Metric::ud_t::typed_member_t<udMetric> metric;
    const auto& operator()(Metric::ud_t&) const noexcept { return metric; }
  } ud;
};

}

#endif  // HPCTOOLKIT_PROFILE_SINKS_ExperimentXML4_H
