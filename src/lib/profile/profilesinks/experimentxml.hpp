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

#ifndef HPCTOOLKIT_PROFILE_PROFILESINKS_EXPERIMENTXML_H
#define HPCTOOLKIT_PROFILE_PROFILESINKS_EXPERIMENTXML_H

#include "../profilesink.hpp"
#include "../profargs.hpp"

#include <atomic>
#include <fstream>
#include <cstdio>
#include <mutex>
#include "../stdshim/filesystem.hpp"

namespace hpctoolkit::profilesinks {

class HPCTraceDB;
class HPCMetricDB;

/// ProfileSink for the current experiment.xml format. Just so we can spit
/// something out.
class ExperimentXML final : public ProfileSink {
public:
  ~ExperimentXML() = default;

  /// Constructor, with a reference to the output database directory.
  ExperimentXML(const ProfArgs&, HPCTraceDB* = nullptr, HPCMetricDB* = nullptr);

  /// Write out as much data as possible. See ProfileSink::write.
  bool write(
    std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

  DataClass accepts() const noexcept {
    using namespace hpctoolkit::literals;
    return "armc"_dat;
  }

  ExtensionClass requires() const noexcept {
    using namespace hpctoolkit::literals;
    return "ci"_ext;
  }

  void notifyPipeline();

private:
  stdshim::filesystem::path dir;
  std::ofstream of;
  std::atomic<unsigned int> next_id;
  HPCTraceDB* tracedb;
  HPCMetricDB* metricdb;
  const ProfArgs& args;

  void emit(const Context& c);
  void emitMetrics(const Context&, bool = true);

  struct uds;

  class udFile {
  public:
    udFile(ExperimentXML&);
    udFile(ExperimentXML&, const Module&);
    udFile(const File&, ExperimentXML&);
    ~udFile() = default;

    void incr();
    operator bool() { return refcnt.load(std::memory_order_relaxed) != 0; }

    std::string tag;
    unsigned int id;

  private:
    const stdshim::filesystem::path& dir;
    std::atomic<unsigned int> refcnt;
    const File* fl;
    const Module* m;
    struct uds& uds;
    const ProfArgs& args;
  };

  udFile file_unknown;

  class udMetric {
  public:
    udMetric(const Metric&, ExperimentXML&);
    ~udMetric() = default;

    std::string tag;
    unsigned int inc_id;
    unsigned int ex_id;

  private:
    struct uds& uds;
  };

  class udFunction {
  public:
    udFunction(const Module&, const Function&, ExperimentXML&);
    udFunction(ExperimentXML&, const Module&);
    enum class fancy { unknown, partial };
    udFunction(ExperimentXML&, fancy);
    ~udFunction() = default;

    void incr();
    operator bool();
    const std::string& tag();

    unsigned int id;
    udFunction* root;
    const Module* mod;

  private:
    const Function* fn;
    bool f1;
    const std::string* name;
    std::string name_store;
    std::string _tag;
    std::atomic<unsigned int> refcnt;
    struct uds& uds;
  };

  struct strptrhash {
    std::hash<std::string> sh;
    std::size_t operator()(const std::string* sp) const noexcept {
      return sh(*sp);
    }
  };
  struct strptreq {
    bool operator()(const std::string* a, const std::string* b) const noexcept {
      return *a == *b;
    }
  };
  internal::locked_unordered_map<const std::string*, udFunction*,
                                 stdshim::shared_mutex,
                                 strptrhash, strptreq> procs;

  udFunction proc_unknown;
  udFunction proc_partial;

  class udModule {
  public:
    udModule(const Module&, ExperimentXML&);
    ~udModule() = default;

    void incr();
    operator bool() { return refcnt.load(std::memory_order_relaxed) != 0; }

    std::string tag;
    unsigned int id;
    udFunction unknown_proc;
    udFile unknown_file;

  private:
    std::atomic<unsigned int> refcnt;
    const Module& mod;
    struct uds& uds;
  };

  std::atomic<unsigned int> next_cid;
  class udContext {
  public:
    udContext(const Context&, ExperimentXML&);
    ~udContext() = default;

    unsigned int id;
    std::string pre;
    std::string open;
    std::string attr;
    bool premetrics;
    std::string close;
    std::string post;
    bool partial;

  private:
    struct uds& uds;
  };

  struct uds {
    File::ud_t::typed_member_t<udFile> file;
    Context::ud_t::typed_member_t<udContext> context;
    Function::ud_t::typed_member_t<udFunction> function;
    Module::ud_t::typed_member_t<udModule> module;
    Metric::ud_t::typed_member_t<udMetric> metric;
    const ProfilePipeline::ExtUserdata* ext;
  } uds;
};

}

#endif  // HPCTOOLKIT_PROFILE_PROFILESINKS_EXPERIMENTXML_H
