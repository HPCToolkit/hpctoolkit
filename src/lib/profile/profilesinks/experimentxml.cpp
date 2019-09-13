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
using namespace profilesinks;
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
  {"hpcrun_no_activity", fancynames::no_activity}
};

// ud Module bits

ExperimentXML::udModule::udModule(const Module& m, ExperimentXML& exml)
  : id(m.userdata[exml.uds.ext->id_module]), unknown_proc(exml, m),
    unknown_file(exml, m), refcnt(0), mod(m), uds(exml.uds) {};

void ExperimentXML::udModule::incr() {
  if(refcnt.fetch_add(1, std::memory_order_relaxed) == 0) {
    std::ostringstream ss;
    ss << "<LoadModule i=\"" << id << "\" n=" << internal::xmlquoted(mod.path().string()) << "/>";
    tag = ss.str();
  }
}

// ud File bits

ExperimentXML::udFile::udFile(const File& f, ExperimentXML& exml)
  : id(f.userdata[exml.uds.ext->id_file]), dir(exml.dir), refcnt(0), fl(&f),
    m(nullptr), uds(exml.uds), args(exml.args) {};

ExperimentXML::udFile::udFile(ExperimentXML& exml, const Module& mm)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)), dir(exml.dir),
    refcnt(0), fl(nullptr), m(&mm), uds(exml.uds), args(exml.args) {};

ExperimentXML::udFile::udFile(ExperimentXML& exml)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)), dir(exml.dir),
    refcnt(0), fl(nullptr), m(nullptr), uds(exml.uds), args(exml.args) {};

void ExperimentXML::udFile::incr() {
  namespace fs = stdshim::filesystem;
  namespace fsx = stdshim::filesystemx;
  if(refcnt.fetch_add(1, std::memory_order_relaxed) == 0) {
    std::ostringstream ss;
    if(fl == nullptr) {
      ss << "<File i=\"" << id << "\" n=\"&lt;unknown file&gt;";
      if(m != nullptr) ss << " [" << internal::xmlquoted(m->path().filename().string(), false) << "]";
      ss << "\"/>";
    } else {
      ss << "<File i=\"" << id << "\" n=";
      bool any = false;
      if(args.include_sources) {
        for(const auto& rp: args.prefix(fl->path())) {
          if(stdshim::filesystem::exists(rp)) {
            fs::path p = fs::path("src") / rp.relative_path().lexically_normal();
            ss << internal::xmlquoted("./" + p.string());
            p = dir / p;
            fs::create_directories(p.parent_path());
            fs::copy_file(rp, p, fsx::copy_options::overwrite_existing);
            any = true;
            break;
          }
        }
      }
      if(!any) ss << internal::xmlquoted(fl->path().string());
      ss << "/>";
    }
    tag = ss.str();
  }
}

// ud Metric bits

ExperimentXML::udMetric::udMetric(const Metric& m, ExperimentXML& exml)
  : uds(exml.uds) {
  const auto& ids = m.userdata[exml.uds.ext->id_metric];
  inc_id = ids.first;
  ex_id = ids.second;
  // Every (for now, later will be most) metrics have an inclusive and
  // exclusive side. So we write out two metrics as if that were still the
  // case.
  // A lot of this is still very indev as more data is ferried through the
  // system as a whole.
  std::ostringstream ss;
  ss << "<Metric i=\"" << inc_id << "\" "
                    "n=" << internal::xmlquoted(m.name() + ":Sum (I)") << " "
                    "md=" << internal::xmlquoted(m.description()) << " "
                    "v=\"derived-incr\" "
                    "t=\"inclusive\" partner=\"" << ex_id << "\" "
                    "show=\"1\" show-percent=\"1\">"
        "<MetricFormula t=\"combine\" frm=\"sum($" << inc_id << ", $" << inc_id << ")\"/>"
        "<MetricFormula t=\"finalize\" frm=\"$" << inc_id << "\"/>"
        "<Info><NV n=\"units\" v=\"events\"/></Info>"
        "</Metric>"
        "<Metric i=\"" << ex_id << "\" "
                    "n=" << internal::xmlquoted(m.name() + ":Sum (E)") << " "
                    "md=" << internal::xmlquoted(m.description()) << " "
                    "v=\"derived-incr\" "
                    "t=\"exclusive\" partner=\"" << inc_id << "\" "
                    "show=\"1\" show-percent=\"1\">"
        "<MetricFormula t=\"combine\" frm=\"sum($" << ex_id << ", $" << ex_id << ")\"/>"
        "<MetricFormula t=\"finalize\" frm=\"$" << ex_id << "\"/>"
        "<Info><NV n=\"units\" v=\"events\"/></Info>"
        "</Metric>";
  tag = ss.str();
}

// ud Function bits

ExperimentXML::udFunction::udFunction(const Module& m, const Function& f, ExperimentXML& exml)
  : id(f.userdata[exml.uds.ext->id_function]), root(this), mod(&m), fn(&f),
    refcnt(0), uds(exml.uds) {
  // First apply any transformations on our name
  name = &f.name;
  f1 = false;
  if(!name->empty()) {
    auto it = nametrans.find(*name);
    if(it != nametrans.end()) {
      name = &it->second;
      f1 = true;
    }
  }
  // If we have a name, find or assume the role of root.
  if(!name->empty()) {
    auto x = exml.procs.emplace(name, this);
    if(!x.second) {  // Someone else got it, reroot
      root = x.first;
      id = root->id;
      return;
    }
  }
}

ExperimentXML::udFunction::udFunction(ExperimentXML& exml, const Module& m)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)),
    root(this), mod(&m), fn(nullptr), f1(false), refcnt(0), uds(exml.uds) {
  std::ostringstream ss;
  ss << fancynames::unknown_proc << " [" << m.path().filename().string() << "]";
  name_store = ss.str();
  name = &name_store;
  exml.procs.emplace(name, this);
}

ExperimentXML::udFunction::udFunction(ExperimentXML& exml, fancy fan)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)),
    root(this), mod(nullptr), fn(nullptr), f1(true), refcnt(0), uds(exml.uds) {
  switch(fan) {
  case fancy::unknown: name = &fancynames::unknown_proc; break;
  case fancy::partial: name = &fancynames::partial_unwind; break;
  }
  exml.procs.emplace(name, this);
}

void ExperimentXML::udFunction::incr() {
  if(root->refcnt.fetch_add(1, std::memory_order_relaxed) == 0) {
    std::ostringstream ss;
    ss << "<Procedure i=\"" << id << "\" n=";
    if(!root->name->empty()) ss << internal::xmlquoted(*root->name);
    else ss << "\"&lt;unknown procedure&gt; 0x"
            << std::hex << root->fn->offset << std::dec
            << " [" << internal::xmlquoted(root->mod->path().filename().string(), false) << "]\"";
    if(root->f1) ss << " f=\"1\"";
    if(root->fn && root->fn->offset != 0)
      ss << " v=\"0x" << std::hex << root->fn->offset << std::dec << "\"";
    else
      ss << " v=\"0\"";
    ss << "/>";
    root->_tag = ss.str();
  }
}

ExperimentXML::udFunction::operator bool() {
  return root->refcnt.load(std::memory_order_relaxed) != 0;
}

const std::string& ExperimentXML::udFunction::tag() {
  return root->_tag;
}

// ud Context bits

ExperimentXML::udContext::udContext(const Context& c, ExperimentXML& exml)
  : id(c.userdata[exml.uds.ext->id_context] + 1), premetrics(false),
    partial(true), uds(exml.uds) {
  const auto& s = c.scope();
  switch(s.type()) {
  case Scope::Type::unknown: {
    auto& proc = (c.direct_parent()->scope().type() == Scope::Type::global)
                 ? exml.proc_partial : exml.proc_unknown;
    proc.incr();
    exml.file_unknown.incr();
    std::ostringstream ss;
    ss << "<PF i=\"" << id << "\""
             " n=\"" << proc.id << "\""
             " s=\"" << proc.id << "\""
             " f=\"" << exml.file_unknown.id << "\""
             " l=\"0\"";
    open = ss.str();
    close = "</PF>";
    break;
  }
  case Scope::Type::loop: {
    auto fl = s.line_data();
    std::ostringstream ss;
    ss << "<L i=\"" << id << "\""
             " l=\"" << fl.second << "\""
             " f=\"" << fl.first.userdata[uds.file].id << "\"";
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
    auto fl = mo.first.userdata[uds.ext->classification].getLine(mo.second);
    if(!c.direct_parent() ||
        c.direct_parent()->scope().type() == Scope::Type::point) {
      auto& udm = mo.first.userdata[uds.module];
      auto fp = fl.first ? &(*fl.first).userdata[uds.file] : &udm.unknown_file;
      fp->incr();
      udm.unknown_proc.incr();
      std::ostringstream ss;
      ss << "<PF i=\"" << id << "\""
               " lm=\"" << udm.id << "\""
               " n=\"" << udm.unknown_proc.id << "\""
               " s=\"" << udm.unknown_proc.id << "\""
               " l=\"" << fl.second << "\""
               " f=\"" << fp->id << "\">";
      id = exml.next_cid.fetch_sub(1, std::memory_order_relaxed);
      pre = ss.str();
      premetrics = true;
      post = "</PF>";
    }
    open = "<";
    std::ostringstream ss;
    ss << " i=\"" << id << "\""
          " l=\"" << fl.second << "\"";
    attr = ss.str();
    s.point_data().first.userdata[uds.module].incr();
    break;
  }
  case Scope::Type::inlined_function: {
    auto fl = s.line_data();
    std::ostringstream ss;
    ss << "<C i=\"" << exml.next_cid.fetch_sub(1, std::memory_order_relaxed) << "\""
            " l=\"" << fl.second << "\">";
    pre = ss.str();
    premetrics = true;
    post = "</C>";
    (s.line_data().first).userdata[uds.file].incr();
    // fallthrough
  }
  case Scope::Type::function: {
    const auto& f = s.function_data();
    auto& udf = f.userdata[uds.function];
    auto& udm = udf.mod->userdata[uds.module];
    auto fp = f.file ? &(*f.file).userdata[uds.file]
            : udf.mod ? &udm.unknown_file
            : &exml.file_unknown;
    udf.incr();
    udm.incr();
    fp->incr();
    std::ostringstream ss;
    ss << "<PF i=\"" << id << "\""
             " l=\"" << f.line << "\""
             " f=\"" << fp->id << "\""
             " n=\"" << udf.id << "\" s=\"" << udf.id << "\""
             " lm=\"" << udm.id << "\"";
    open = ss.str();
    close = "</PF>";
    partial = false;  // If we have a Function, we're not lost. Probably.
    break;
  }
  }
}

// ExperimentXML bits

ExperimentXML::ExperimentXML(const ProfArgs& pargs, HPCTraceDB* db, HPCMetricDB* mdb)
  : ProfileSink(), dir(pargs.output), of(), next_id(0x7FFFFFFF), tracedb(db),
    metricdb(mdb), args(pargs),
    file_unknown(*this), proc_unknown(*this, udFunction::fancy::unknown),
    proc_partial(*this, udFunction::fancy::partial), next_cid(0x7FFFFFFF) {
  stdshim::filesystem::create_directory(dir);
  of.open((dir / "experiment.xml").string());
}

void ExperimentXML::notifyPipeline() {
  auto& ss = src.structs();
  uds.file = ss.file.add<udFile>(std::ref(*this));
  uds.context = ss.context.add<udContext>(std::ref(*this));
  uds.function = ss.function.add<udFunction>(std::ref(*this));
  uds.module = ss.module.add<udModule>(std::ref(*this));
  uds.metric = ss.metric.add<udMetric>(std::ref(*this));
  uds.ext = &src.extensions();
}

bool ExperimentXML::write(std::chrono::nanoseconds) {
  const auto& name = src.attributes().name();
  of << "<?xml version=\"1.0\"?>"
        "<HPCToolkitExperiment version=\"2.2\">"
        "<Header n=" << internal::xmlquoted(name) << ">"
        "<Info/>"
        "</Header>"
        "<SecCallPathProfile i=\"0\" n=" << internal::xmlquoted(name) << ">"
        "<SecHeader>";

  // MetricTable: from the Metrics
  of << "<MetricTable>";
  for(const auto& m: src.metrics()) of << m().userdata[uds.metric].tag;
  of << "</MetricTable>";

  if(metricdb != nullptr)
    of << metricdb->exmlTag();
  if(tracedb != nullptr)
    of << "<TraceDBTable>" << tracedb->exmlTag() << "</TraceDBTable>";
  of << "<LoadModuleTable>";
  // LoadModuleTable: from the Modules
  for(const auto& m: src.modules()) {
    auto& ud = m().userdata[uds.module];
    if(!ud) continue;
    of << ud.tag;
  }
  of << "</LoadModuleTable>"
        "<FileTable>";
  // FileTable: from the Files
  if(file_unknown) of << file_unknown.tag;
  for(const auto& f: src.files()) {
    auto& ud = f().userdata[uds.file];
    if(!ud) continue;
    of << ud.tag;
  }
  for(const auto& m: src.modules()) {
    auto& ud = m().userdata[uds.module].unknown_file;
    if(!ud) continue;
    of << ud.tag;
  }
  of << "</FileTable>"
        "<ProcedureTable>";
  // ProcedureTable: from the Functions for each Module.
  for(const auto& nf: procs) {
    auto& ud = *nf.second;
    if(!ud) continue;
    of << ud.tag();
  }
  of << "</ProcedureTable>"

        "<Info/>"
        "</SecHeader>";

  // Spit out the CCT
  emit(src.contexts());

  of << "</SecCallPathProfile>"
        "</HPCToolkitExperiment>" << std::flush;
  return true;
}

void ExperimentXML::emitMetrics(const Context& c, bool ex) {
  for(const auto& met: src.metrics()) {
    auto& udm = met().userdata[uds.metric];
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
  auto& ud = c.userdata[uds.context];

  bool emitmetrics = true;

  // First emit our tags, and whatever extensions are nessesary.
  of << ud.pre;
  if(ud.premetrics && emitmetrics) emitMetrics(c, false);
  of << ud.open;
  switch(s.type()) {
  case Scope::Type::unknown:
  case Scope::Type::global:
  case Scope::Type::loop:
  case Scope::Type::inlined_function:
  case Scope::Type::function:
    break;
  case Scope::Type::point:
    of << (c.children().empty() ? 'S' : 'C') << ud.attr;
    // C nodes don't emit metrics, but S nodes do.
    emitmetrics = emitmetrics && c.children().empty();
    break;
  }
  if(s.type() != Scope::Type::global && tracedb != nullptr && tracedb->seen(c))
    of << " it=\"" << c.userdata[src.extensions().id_context] << "\"";

  // If this is an empty tag, use the shorter form, otherwise close the tag.
  if(c.children().empty() && !emitmetrics) {
    of << "/>" << ud.post;
    return;
  }
  of << ">";

  // Emit the metrics (if we're emitting metrics) above the other contents.
  if(emitmetrics) emitMetrics(c);

  // Recurse through the children.
  for(const auto& cc: c.children()) emit(cc());

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
  of << ud.close << ud.post;
}
