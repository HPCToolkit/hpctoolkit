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

#include "experimentxml.hpp"

#include "hpctracedb.hpp"
#include "hpcmetricdb.hpp"
#include "../util/xml.hpp"

#include <iomanip>
#include <algorithm>
#include <sstream>
#include <limits>

using namespace hpctoolkit;
using namespace sinks;
namespace fs = stdshim::filesystem;

// Function name transformation table
namespace fancynames {
  static const std::string program_root     = "<program root>";
  static const std::string thread_root      = "<thread root>";
  static const std::string omp_idle         = "<omp idle>";
  static const std::string omp_overhead     = "<omp overhead>";
  static const std::string omp_barrier_wait = "<omp barrier wait>";
  static const std::string omp_task_wait    = "<omp task wait>";
  static const std::string omp_mutex_wait   = "<omp mutex wait>";
  static const std::string no_thread        = "<no thread>";
  static const std::string partial_unwind   = "<partial call paths>";
  static const std::string no_activity      = "<no activity>";
  static const std::string gpu_copy         = "<gpu copy>";
  static const std::string gpu_copyin       = "<gpu copyin>";
  static const std::string gpu_copyout      = "<gpu copyout>";
  static const std::string gpu_alloc        = "<gpu alloc>";
  static const std::string gpu_delete       = "<gpu delete>";
  static const std::string gpu_sync         = "<gpu sync>";
  static const std::string gpu_kernel       = "<gpu kernel>";

  static const std::string unknown_proc     = "<unknown procedure>";
}
static const std::unordered_map<std::string, const std::string&> nametrans = {
  {"monitor_main", fancynames::program_root},
  {"monitor_main_fence1", fancynames::program_root},
  {"monitor_main_fence2", fancynames::program_root},
  {"monitor_main_fence3", fancynames::program_root},
  {"monitor_main_fence4", fancynames::program_root},
  {"monitor_begin_thread", fancynames::thread_root},
  {"monitor_thread_fence1", fancynames::thread_root},
  {"monitor_thread_fence2", fancynames::thread_root},
  {"monitor_thread_fence3", fancynames::thread_root},
  {"monitor_thread_fence4", fancynames::thread_root},
  {"ompt_idle_state", fancynames::omp_idle},
  {"ompt_idle", fancynames::omp_idle},
  {"ompt_overhead_state", fancynames::omp_overhead},
  {"omp_overhead", fancynames::omp_overhead},
  {"ompt_barrier_wait_state", fancynames::omp_barrier_wait},
  {"ompt_barrier_wait", fancynames::omp_barrier_wait},
  {"ompt_task_wait_state", fancynames::omp_task_wait},
  {"ompt_task_wait", fancynames::omp_task_wait},
  {"ompt_mutex_wait_state", fancynames::omp_mutex_wait},
  {"ompt_mutex_wait", fancynames::omp_mutex_wait},
  {"NO_THREAD", fancynames::no_thread},
  {"gpu_op_copy", fancynames::gpu_copy},
  {"gpu_op_copyin", fancynames::gpu_copyin},
  {"gpu_op_copyout", fancynames::gpu_copyout},
  {"gpu_op_alloc", fancynames::gpu_alloc},
  {"gpu_op_delete", fancynames::gpu_delete},
  {"gpu_op_sync", fancynames::gpu_sync},
  {"gpu_op_kernel", fancynames::gpu_kernel},
  {"gpu_op_trace", fancynames::gpu_kernel},
  {"hpcrun_no_activity", fancynames::no_activity}
};

// ud Module bits

ExperimentXML::udModule::udModule(const Module& m, ExperimentXML& exml)
  : id(m.userdata[exml.src.identifier()]), unknown_file(exml, m),
    used(false) {};

void ExperimentXML::udModule::incr(const Module& mod, ExperimentXML&) {
  if(!used.exchange(true, std::memory_order_relaxed)) {
    std::ostringstream ss;
    ss << "<LoadModule i=\"" << id << "\" n=" << util::xmlquoted(mod.path().string()) << "/>";
    tag = ss.str();
  }
}

// ud File bits

ExperimentXML::udFile::udFile(const File& f, ExperimentXML& exml)
  : id(f.userdata[exml.src.identifier()]), used(false), fl(&f),
    m(nullptr) {};

ExperimentXML::udFile::udFile(ExperimentXML& exml, const Module& mm)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)), used(false),
    fl(nullptr), m(&mm) {};

ExperimentXML::udFile::udFile(ExperimentXML& exml)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)), used(false),
    fl(nullptr), m(nullptr) {};

void ExperimentXML::udFile::incr(ExperimentXML& exml) {
  namespace fs = stdshim::filesystem;
  namespace fsx = stdshim::filesystemx;
  if(!used.exchange(true, std::memory_order_relaxed)) {
    std::ostringstream ss;
    if(fl == nullptr) {
      ss << "<File i=\"" << id << "\" n=\"&lt;unknown file&gt;";
      if(m != nullptr) ss << " [" << util::xmlquoted(m->path().filename().string(), false) << "]";
      ss << "\"/>";
    } else {
      ss << "<File i=\"" << id << "\" n=";
      const fs::path& rp = fl->userdata[exml.src.resolvedPath()];
      if(exml.include_sources && !rp.empty()) {
        fs::path p = fs::path("src") / rp.relative_path().lexically_normal();
        ss << util::xmlquoted("./" + p.string());
        p = exml.dir / p;
        if(!exml.dir.empty()) {
          fs::create_directories(p.parent_path());
          fs::copy_file(rp, p, fsx::copy_options::overwrite_existing);
        }
      } else ss << util::xmlquoted(fl->path().string());
      ss << "/>";
    }
    tag = ss.str();
  }
}

// ud Metric bits

ExperimentXML::udMetric::udMetric(const Metric& m, ExperimentXML& exml) {
  const auto& ids = m.userdata[exml.src.identifier()];
  inc_id = ids.first;
  ex_id = ids.second;
  // Every (for now, later will be most) metrics have an inclusive and
  // exclusive side. So we write out two metrics as if that were still the
  // case.
  // A lot of this is still very indev as more data is ferried through the
  // system as a whole.
  std::ostringstream ss;
  ss << "<Metric i=\"" << inc_id << "\" o=\"" << inc_id << "\" "
                    "n=" << util::xmlquoted(m.name() + ":Sum (I)") << " "
                    "md=" << util::xmlquoted(m.description()) << " "
                    "v=\"derived-incr\" "
                    "t=\"inclusive\" partner=\"" << ex_id << "\" "
                    "show=\"1\" show-percent=\"1\">"
        "<MetricFormula t=\"combine\" frm=\"sum($" << inc_id << ", $" << inc_id << ")\"/>"
        "<MetricFormula t=\"finalize\" frm=\"$" << inc_id << "\"/>"
        "<Info><NV n=\"units\" v=\"events\"/></Info>"
        "</Metric>"
        "<Metric i=\"" << ex_id << "\" o=\"" << ex_id << "\" "
                    "n=" << util::xmlquoted(m.name() + ":Sum (E)") << " "
                    "md=" << util::xmlquoted(m.description()) << " "
                    "v=\"derived-incr\" "
                    "t=\"exclusive\" partner=\"" << inc_id << "\" "
                    "show=\"1\" show-percent=\"1\">"
        "<MetricFormula t=\"combine\" frm=\"sum($" << ex_id << ", $" << ex_id << ")\"/>"
        "<MetricFormula t=\"finalize\" frm=\"$" << ex_id << "\"/>"
        "<Info><NV n=\"units\" v=\"events\"/></Info>"
        "</Metric>";
  tag = ss.str();
}

// ud Context bits

ExperimentXML::udContext::udContext(const Context& c, ExperimentXML& exml)
  : id(c.userdata[exml.src.identifier()]*2 + 1), premetrics(false) {
  const auto& s = c.scope();
  auto& proc = exml.getProc(s);
  switch(s.type()) {
  case Scope::Type::unknown: {
    auto& uproc = (c.direct_parent()->scope().type() == Scope::Type::global)
                   ? exml.proc_partial() : exml.proc_unknown();
    exml.file_unknown.incr(exml);
    std::ostringstream ss;
    ss << "<PF i=\"" << id << "\""
             " n=\"" << uproc.id << "\" s=\"" << uproc.id << "\""
             " f=\"" << exml.file_unknown.id << "\""
             " l=\"0\"";
    open = ss.str();
    close = "</PF>";
    break;
  }
  case Scope::Type::loop: {
    auto fl = s.line_data();
    std::ostringstream ss;
    ss << "<L i=\"" << id << "\" s=\"" << proc.id << "\" v=\"0\""
             " l=\"" << fl.second << "\""
             " f=\"" << fl.first.userdata[exml.ud].id << "\"";
    open = ss.str();
    close = "</L>";
    break;
  }
  case Scope::Type::global:
    open = "<SecCallPathProfileData";
    close = "</SecCallPathProfileData>";
    break;
  case Scope::Type::point: {
    const auto& mo = s.point_data();
    auto fl = mo.first.userdata[exml.src.classification()].getLine(mo.second);
    if(c.direct_parent()->scope().type() == Scope::Type::point) {
      if(proc.prep()) {  // We're in charge of the tag, and this is a tag we want.
        std::ostringstream ss;
        ss << fancynames::unknown_proc << " "
              "0x" << std::hex << mo.second << " "
              "[" << mo.first.path().filename().string() << "]";
        proc.setTag(ss.str(), mo.second, true);
      }
      auto& udm = mo.first.userdata[exml.ud];
      auto& udf = fl.first ? fl.first->userdata[exml.ud] : udm.unknown_file;
      udf.incr(exml);
      std::ostringstream ss;
      ss << "<PF i=\"" << id << "\""
               " lm=\"" << udm.id << "\""
               " n=\"" << proc.id << "\" s=\"" << proc.id << "\""
               " l=\"" << fl.second << "\""
               " f=\"" << udf.id << "\">";
      id++;  // Switch to our other id
      pre = ss.str();
      premetrics = true;
      post = "</PF>";
    }
    open = "<";
    std::ostringstream ss;
    ss << " i=\"" << id << "\" s=\"" << proc.id << "\""
          " l=\"" << fl.second << "\"";
    attr = ss.str();
    mo.first.userdata[exml.ud].incr(mo.first, exml);
    break;
  }
  case Scope::Type::inlined_function: {
    auto fl = s.line_data();
    std::ostringstream ss;
    ss << "<C i=\"" << id << "\" s=\"" << proc.id << "\""
            " v=\"0\""
            " l=\"" << fl.second << "\">";
    id++;  // Switch to our other id
    pre = ss.str();
    premetrics = true;
    post = "</C>";
    (s.line_data().first).userdata[exml.ud].incr(exml);
    // fallthrough
  }
  case Scope::Type::function: {
    const auto& f = s.function_data();
    if(proc.prep()) {  // Our job to do the tag
      if(f.name.empty()) { // Anonymous function, write as an <unknown proc>
        std::ostringstream ss;
        ss << fancynames::unknown_proc << " "
              "0x" << std::hex << f.offset << " "
              "[" << f.module().path().string() << "]";
        proc.setTag(ss.str(), f.offset, true);
      } else {  // Normal function, but might have a name translation
        auto it = nametrans.find(f.name);
        if(it != nametrans.end()) proc.setTag(it->second, 0, true);
        else proc.setTag(f.name, f.offset, false);
      }
    }
    auto& udm = f.module().userdata[exml.ud];
    auto& udf = f.file ? f.file->userdata[exml.ud] : udm.unknown_file;
    udm.incr(f.module(), exml);
    udf.incr(exml);
    std::ostringstream ss;
    ss << "<PF i=\"" << id << "\""
             " l=\"" << f.line << "\""
             " f=\"" << udf.id << "\""
             " n=\"" << proc.id << "\" s=\"" << proc.id << "\""
             " lm=\"" << udm.id << "\"";
    open = ss.str();
    close = "</PF>";
    partial = false;  // If we have a Function, we're not lost. Probably.
    break;
  }
  }
}

// ExperimentXML bits

ExperimentXML::ExperimentXML(const fs::path& out, bool srcs, HPCTraceDB* db,
                             HPCMetricDB* mdb)
  : ProfileSink(), dir(out), of(), next_id(0x7FFFFFFF), tracedb(db),
    metricdb(mdb), include_sources(srcs), file_unknown(*this), next_procid(2),
    proc_unknown_proc(0), proc_partial_proc(1), next_cid(0x7FFFFFFF) {
  if(dir.empty()) {  // Dry run
    util::log::info() << "ExperimentXML issuing a dry run!";
  } else {
    stdshim::filesystem::create_directory(dir);
    of.open((dir / "experiment.xml").native());
  }
}

ExperimentXML::Proc& ExperimentXML::getProc(const Scope& k) {
  return procs.emplace(k, next_procid.fetch_add(1, std::memory_order_relaxed)).first;
}

const ExperimentXML::Proc& ExperimentXML::proc_unknown() {
  proc_unknown_flag.call_nowait([&](){
    proc_unknown_proc.setTag(fancynames::unknown_proc, 0, true);
  });
  return proc_unknown_proc;
}

const ExperimentXML::Proc& ExperimentXML::proc_partial() {
  proc_partial_flag.call_nowait([&](){
    proc_partial_proc.setTag(fancynames::partial_unwind, 0, true);
  });
  return proc_partial_proc;
}

void ExperimentXML::Proc::setTag(std::string n, std::size_t v, bool fake) {
  std::ostringstream ss;
  ss << "<Procedure i=\"" << id << "\" n=" << util::xmlquoted(n)
     << " v=\"" << std::hex << (v == 0 ? "" : "0x") << v << "\"";
  if(fake) ss << " f=\"1\"";
  ss << "/>";
  tag = ss.str();
}

bool ExperimentXML::Proc::prep() {
  bool x = false;
  return done.compare_exchange_strong(x, true, std::memory_order_relaxed);
}

void ExperimentXML::notifyPipeline() noexcept {
  auto& ss = src.structs();
  ud.file = ss.file.add<udFile>(std::ref(*this));
  ud.context = ss.context.add<udContext>(std::ref(*this));
  ud.module = ss.module.add<udModule>(std::ref(*this));
  ud.metric = ss.metric.add<udMetric>(std::ref(*this));
}

void ExperimentXML::write() {
  const auto& name = src.attributes().name();
  of << "<?xml version=\"1.0\"?>"
        "<HPCToolkitExperiment version=\"2.2\">"
        "<Header n=" << util::xmlquoted(name) << ">"
        "<Info/>"
        "</Header>"
        "<SecCallPathProfile i=\"0\" n=" << util::xmlquoted(name) << ">"
        "<SecHeader>";

  // MetricTable: from the Metrics
  of << "<MetricTable>";
  for(const auto& m: src.metrics().iterate()) of << m().userdata[ud].tag;
  of << "</MetricTable>";

  if(metricdb != nullptr)
    of << metricdb->exmlTag();
  else
    of << "<MetricDBTable/>";
  if(tracedb != nullptr)
    of << "<TraceDBTable>" << tracedb->exmlTag() << "</TraceDBTable>";
  of << "<LoadModuleTable>";
  // LoadModuleTable: from the Modules
  for(const auto& m: src.modules().iterate()) {
    auto& udm = m().userdata[ud];
    if(!udm) continue;
    of << udm.tag;
  }
  of << "</LoadModuleTable>"
        "<FileTable>";
  // FileTable: from the Files
  if(file_unknown) of << file_unknown.tag;
  for(const auto& f: src.files().iterate()) {
    auto& udf = f().userdata[ud];
    if(!udf) continue;
    of << udf.tag;
  }
  for(const auto& m: src.modules().iterate()) {
    auto& udm = m().userdata[ud].unknown_file;
    if(!udm) continue;
    of << udm.tag;
  }
  of << "</FileTable>"
        "<ProcedureTable>";
  // ProcedureTable: from the Functions for each Module.
  for(const auto& sp: procs.iterate()) of << sp.second.tag;
  if(proc_unknown_flag.query()) of << proc_unknown_proc.tag;
  if(proc_partial_flag.query()) of << proc_partial_proc.tag;
  of << "</ProcedureTable>"

        "<Info/>"
        "</SecHeader>";

  // Spit out the CCT
  emit(src.contexts());

  of << "</SecCallPathProfile>"
        "</HPCToolkitExperiment>" << std::flush;
}

void ExperimentXML::emitMetrics(const Context& c, bool ex) {
  for(const auto& met: src.metrics().iterate()) {
    auto& udm = met().userdata[ud];
    auto v = met().getFor(c);
    if(v.first != 0 && ex)
      of << "<M n=\"" << udm.ex_id << "\" v=\""
        << std::scientific << v.first << std::defaultfloat
        << "\"/>";
    if(v.second != 0)
      of << "<M n=\"" << udm.inc_id << "\" v=\""
        << std::scientific << v.second << std::defaultfloat
        << "\"/>";
  }
}

void ExperimentXML::emit(const Context& c) {
  const auto& s = c.scope();
  auto& udc = c.userdata[ud];

  bool emitmetrics = true;

  // First emit our tags, and whatever extensions are nessesary.
  of << udc.pre;
  if(udc.premetrics && emitmetrics) emitMetrics(c, false);
  of << udc.open;
  switch(s.type()) {
  case Scope::Type::unknown:
  case Scope::Type::global:
  case Scope::Type::loop:
  case Scope::Type::inlined_function:
  case Scope::Type::function:
    break;
  case Scope::Type::point:
    of << (c.children().empty() ? 'S' : 'C') << udc.attr;
    // C nodes don't emit metrics, but S nodes do.
    emitmetrics = emitmetrics && c.children().empty();
    break;
  }
  if(s.type() != Scope::Type::global && tracedb != nullptr && tracedb->seen(c))
    of << " it=\"" << c.userdata[src.identifier()] << "\"";

  // If this is an empty tag, use the shorter form, otherwise close the tag.
  if(c.children().empty() && !emitmetrics) {
    of << "/>" << udc.post;
    return;
  }
  of << ">";

  // Emit the metrics (if we're emitting metrics) above the other contents.
  if(emitmetrics) emitMetrics(c);

  // Recurse through the children.
  for(const auto& cc: c.children().iterate()) emit(cc());

  // Close off this tag.
  switch(s.type()) {
  case Scope::Type::unknown:
  case Scope::Type::global:
  case Scope::Type::function:
  case Scope::Type::inlined_function:
  case Scope::Type::loop:
    break;
  case Scope::Type::point:
    of << "</" << (c.children().empty() ? 'S' : 'C') << ">";
    break;
  }
  of << udc.close << udc.post;
}
