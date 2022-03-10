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
// Copyright ((c)) 2019-2022, Rice University
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

#include "experimentxml4.hpp"

#include "hpctracedb2.hpp"
#include "../util/xml.hpp"

#include <iomanip>
#include <algorithm>
#include <sstream>
#include <stack>
#include <limits>

using namespace hpctoolkit;
using namespace sinks;
namespace fs = stdshim::filesystem;

// ud Module bits

ExperimentXML4::udModule::udModule(const Module& m, ExperimentXML4& exml)
  : id(m.userdata[exml.src.identifier()]+2), unknown_file(exml, m),
    used(false) {};

ExperimentXML4::udModule::udModule(ExperimentXML4& exml)
  : id(0), unknown_file(exml), used(true) {
  std::ostringstream ss;
  ss << "<LoadModule i=\"" << id << "\" n=" << util::xmlquoted("<unknown module>") << "/>\n";
  tag = ss.str();
}
ExperimentXML4::udModule::udModule(ExperimentXML4& exml, int)
  : id(1), unknown_file(exml), used(true) {
  std::ostringstream ss;
  ss << "<LoadModule i=\"" << id << "\" n=" << util::xmlquoted("<no module>") << "/>\n";
  tag = ss.str();
}


void ExperimentXML4::udModule::incr(const Module& mod, ExperimentXML4&) {
  if(!used.exchange(true, std::memory_order_relaxed)) {
    std::ostringstream ss;
    ss << "<LoadModule i=\"" << id << "\" n=" << util::xmlquoted(mod.path().string()) << "/>\n";
    tag = ss.str();
  }
}

// ud File bits

ExperimentXML4::udFile::udFile(const File& f, ExperimentXML4& exml)
  : id(f.userdata[exml.src.identifier()]), used(false), fl(&f),
    m(nullptr) {};

ExperimentXML4::udFile::udFile(ExperimentXML4& exml, const Module& mm)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)), used(false),
    fl(nullptr), m(&mm) {};

ExperimentXML4::udFile::udFile(ExperimentXML4& exml)
  : id(exml.next_id.fetch_sub(1, std::memory_order_relaxed)), used(false),
    fl(nullptr), m(nullptr) {};

void ExperimentXML4::udFile::incr(ExperimentXML4& exml) {
  namespace fs = stdshim::filesystem;
  if(!used.exchange(true, std::memory_order_relaxed)) {
    std::ostringstream ss;
    if(fl == nullptr) {
      ss << "<File i=\"" << id << "\" n=\"&lt;unknown file&gt;";
      if(m != nullptr) ss << " [" << util::xmlquoted(m->path().filename().string(), false) << "]";
      ss << "\"/>\n";
    } else {
      ss << "<File i=\"" << id << "\" n=";
      const fs::path& rp = fl->userdata[exml.src.resolvedPath()];
      if(exml.include_sources && !rp.empty()) {
        fs::path p = fs::path("src") / rp.relative_path().lexically_normal();
        ss << util::xmlquoted("./" + p.string());
        p = exml.dir / p;
        if(!exml.dir.empty()) {
          fs::create_directories(p.parent_path());
          fs::copy_file(rp, p, fs::copy_options::overwrite_existing);
        }
      } else ss << util::xmlquoted(fl->path().string());
      ss << "/>\n";
    }
    tag = ss.str();
  }
}

// ud Metric bits

static void combineFormula(std::ostream& os, unsigned int id,
                           const StatisticPartial& p) {
  os << "<MetricFormula t=\"combine\" frm=\"";
  switch(p.combinator()) {
  case Statistic::combination_t::sum: os << "sum"; break;
  case Statistic::combination_t::min: os << "min"; break;
  case Statistic::combination_t::max: os << "max"; break;
  }
  os << "($" << id << ", $" << id << ")\"/>\n";
}

static void finalizeFormula(std::ostream& os, const std::string& mode,
                            const Metric& m, const Metric::Identifier& id,
                            MetricScope ms, const Statistic& s) {
  os << "<MetricFormula t=\"" << mode << "\" frm=\"";

  std::stack<bool, std::vector<bool>> first;
  first.push(true);
  std::stack<std::string, std::vector<std::string>> infix;
  infix.push("!!!");
  s.finalizeFormula().citerate_all(
    [&](double v){
      if(!first.top()) os << infix.top();
      first.top() = false;
      os << v;
    },
    [&](Expression::uservalue_t idx){
      if(!first.top()) os << infix.top();
      first.top() = false;
      os << "$" << id.getFor(m.partials()[idx], ms);
    },
    [&](const Expression& e) {
      if(!first.top()) os << infix.top();
      first.top() = false;
      std::string fix = "!!!";
      switch(e.kind()) {
      case Expression::Kind::constant:
      case Expression::Kind::subexpression:
      case Expression::Kind::variable:
        std::abort();
      case Expression::Kind::op_sum:  os << '('; fix = "+"; break;
      case Expression::Kind::op_sub:  os << '('; fix = "-"; break;
      case Expression::Kind::op_neg:  os << "-("; break;
      case Expression::Kind::op_prod: os << '('; fix = "*"; break;
      case Expression::Kind::op_div:  os << '('; fix = "/"; break;
      case Expression::Kind::op_pow:  os << '('; fix = "^"; break;
      case Expression::Kind::op_sqrt: os << "sqrt("; break;
      case Expression::Kind::op_log:  os << "log("; fix = ","; break;
      case Expression::Kind::op_ln:   os << "ln(";break;
      case Expression::Kind::op_min:  os << "min("; fix = ","; break;
      case Expression::Kind::op_max:  os << "max("; fix = ","; break;
      case Expression::Kind::op_floor: os << "floor("; break;
      case Expression::Kind::op_ceil: os << "ceil("; break;
      }
      first.push(true);
      infix.push(std::move(fix));
    },
    [&](const Expression&) {
      first.pop();
      infix.pop();
      os << ')';
    }
  );

  os << "\"/>\n";
}

ExperimentXML4::udMetric::udMetric(const Metric& m, ExperimentXML4& exml) {
  if(!m.scopes().has(MetricScope::function) && !m.scopes().has(MetricScope::execution))
    util::log::fatal{} << "Metric " << m.name() << " has neither function nor execution!";
  const auto& id = m.userdata[exml.src.identifier()];

  {
    std::ostringstream ss;

    // First pass: get all the Partials out there.
    for(size_t idx = 0; idx < m.partials().size(); idx++) {
      const auto& partial = m.partials()[idx];
      const std::string name = m.name() + ":PARTIAL:" + std::to_string(idx);

      const auto f = [&](MetricScope ms, MetricScope p_ms, int id, int p_id,
                         std::string suffix, std::string type) {
        if(m.scopes().has(ms)) {
          ss << "<Metric i=\"" << id << "\" o=\"" << id << "\" "
                            "n=" << util::xmlquoted(m.scopes().has(p_ms) ? name+suffix : name) << " "
                            "md=" << util::xmlquoted(m.description()) << " "
                            "v=\"derived-incr\" "
                            "t=\"" << type << "\" partner=\"" << p_id << "\" "
                            "show=\"4\" show-percent=\"0\">\n";
          combineFormula(ss, id, partial);
          ss << "<Info><NV n=\"units\" v=\"events\"/></Info>\n"
                "</Metric>\n";
        } else {
          ss << "<Metric i=\"" << id << "\" o=\"" << id << "\" "
                        "n=" << util::xmlquoted(name+" INTERNAL") << " "
                        "v=\"derived-incr\" "
                        "t=\"" << type << "\" partner=\"" << p_id << "\" "
                        "show=\"4\" show-percent=\"0\"/>\n";
        }
      };

      const int exec_id = m.scopes().has(MetricScope::execution)
                          ? id.getFor(MetricScope::execution)
                          : exml.next_statid.fetch_sub(1, std::memory_order_relaxed);
      const int func_id = m.scopes().has(MetricScope::function)
                          ? id.getFor(MetricScope::function)
                          : exml.next_statid.fetch_sub(1, std::memory_order_relaxed);
      f(MetricScope::execution, MetricScope::function, exec_id, func_id,
        " (I)", "inclusive");
      f(MetricScope::function, MetricScope::execution, func_id, exec_id,
        " (E)", "exclusive");
    }

    // Second pass: handle all the Statistics.
    for(size_t idx = 0; idx < m.statistics().size(); idx++) {
      const auto& stat = m.statistics()[idx];
      const std::string name = m.name() + ":" + stat.suffix();
      const auto f = [&](MetricScope ms, MetricScope p_ms, int m_id, int p_id,
                         std::string suffix, std::string type) {
        if(m.scopes().has(ms)) {
          ss << "<Metric i=\"" << m_id << "\" o=\"" << m_id << "\" "
                            "n=" << util::xmlquoted(m.scopes().has(p_ms) ? name+suffix : name) << " "
                            "md=" << util::xmlquoted(m.description()) << " "
                            "v=\"derived-incr\" "
                            "t=\"" << type << "\" partner=\"" << p_id << "\" "
                            "show=\"" << (stat.visibleByDefault() ? "1" : "0") << "\" "
                            "show-percent=\"" << (stat.showPercent() ? "1" : "0") << "\">\n";
          finalizeFormula(ss, "view", m, id, ms, stat);
          ss << "<Info><NV n=\"units\" v=\"events\"/></Info>\n"
                "</Metric>\n";
        } else {
          ss << "<Metric i=\"" << m_id << "\" o=\"" << m_id << "\" "
                        "n=" << util::xmlquoted(name+" INTERNAL") << " "
                        "v=\"derived-incr\" "
                        "t=\"" << type << "\" partner=\"" << p_id << "\" "
                        "show=\"4\" show-percent=\"0\"/>\n";
        }
      };

      const auto exec_id = exml.next_statid.fetch_sub(1, std::memory_order_relaxed);
      const auto func_id = exml.next_statid.fetch_sub(1, std::memory_order_relaxed);
      f(MetricScope::execution, MetricScope::function, exec_id, func_id,
        " (I)", "inclusive");
      f(MetricScope::function, MetricScope::execution, func_id, exec_id,
        " (E)", "exclusive");
    }

    metric_tags = ss.str();
  }

  std::ostringstream ss2;
  const auto f = [&](MetricScope ms, MetricScope p_ms, std::string suffix) {
    if(!m.scopes().has(ms)) return;
    ss2 << "<MetricDB i=\"" << id.getFor(ms) << "\""
                    " n=" << util::xmlquoted(m.scopes().has(p_ms) ? m.name()+suffix : m.name())
        << "/>\n";
  };
  f(MetricScope::execution, MetricScope::function, " (I)");
  f(MetricScope::function, MetricScope::execution, " (E)");
  metricdb_tags = ss2.str();
}

std::string ExperimentXML4::eStatMetricTags(const ExtraStatistic& es) {
  std::ostringstream ss;
  const auto f = [&](MetricScope ms, MetricScope p_ms, int id, int p_id,
                     std::string suffix, std::string type) {
    if(es.scopes().has(ms)) {
      ss << "<Metric i=\"" << id << "\" o=\"" << id << "\" "
                        "n=" << util::xmlquoted(es.scopes().has(p_ms) ? es.name()+suffix : es.name()) << " "
                        "md=" << util::xmlquoted(es.description()) << " "
                        "v=\"derived-incr\" "
                        "t=\"" << type << "\" partner=\"" << p_id << "\" "
                        "show=\"" << (es.visibleByDefault() ? "1" : "0") << "\" "
                        "show-percent=\"" << (es.showPercent() ? "1" : "0") << "\" ";
      if(!es.format().empty())
        ss << "fmt=" << util::xmlquoted(es.format()) << " ";
      ss << ">\n"
            "<MetricFormula t=\"view\" frm=\"";
      for(const auto& e: es.formula()) {
        if(const auto* frag = std::get_if<std::string>(&e)) {
          ss << *frag;
        } else if(const auto* mp = std::get_if<ExtraStatistic::MetricPartialRef>(&e)) {
          const auto& id = mp->metric.userdata[src.identifier()];
          ss << "$" << id.getFor(mp->metric.partials()[mp->partialIdx], ms);
        } else std::abort();
      }
      ss << "\"/>\n"
            "<Info><NV n=\"units\" v=\"events\"/></Info>\n"
            "</Metric>\n";
    } else {
      ss << "<Metric i=\"" << id << "\" o=\"" << id << "\" "
                    "n=" << util::xmlquoted(es.name()+" INTERNAL") << " "
                    "v=\"derived-incr\" "
                    "t=\"" << type << "\" partner=\"" << p_id << "\" "
                    "show=\"4\" show-percent=\"0\"/>\n";
    }
  };

  const auto exec_id = next_statid.fetch_sub(1, std::memory_order_relaxed);
  const auto func_id = next_statid.fetch_sub(1, std::memory_order_relaxed);
  f(MetricScope::execution, MetricScope::function, exec_id, func_id,
    " (I)", "inclusive");
  f(MetricScope::function, MetricScope::execution, func_id, exec_id,
    " (E)", "exclusive");

  return ss.str();
}

// ud Context bits

ExperimentXML4::udContext::udContext(const Context& c, ExperimentXML4& exml)
  : onlyOutputWithChildren(false), openIsClosedTag(false),
    siblingS(exml.file_unknown, 0) {
  const auto& s = c.scope().flat();
  auto& proc = exml.getProc(s);
  auto id = c.userdata[exml.src.identifier()];
  bool parentIsC = false;
  bool hasUncleS = false;
  std::pair<std::reference_wrapper<udFile>, uint64_t> uncleS(exml.file_unknown, 0);
  if(auto p = c.direct_parent()) {
    const auto& udp = p->userdata[exml.ud];
    parentIsC = udp.tagIsC;
    hasUncleS = udp.hasSiblingS;
    uncleS = udp.siblingS;
  }

  switch(s.type()) {
  case Scope::Type::global:
    open = "<SecCallPathProfileData";
    close = "</SecCallPathProfileData>\n";
    tagIsC = true;  // Not actually C, but next is PF so effectively like C
    hasSiblingS = false;
    break;
  case Scope::Type::unknown: {
    auto& uproc = c.direct_parent()->scope().relation() == Relation::global
                  ? exml.proc_partial() : exml.proc_unknown();
    exml.file_unknown.incr(exml);
    std::ostringstream ss;
    if(!parentIsC) {
      ss << "<C i=\"-" << id << "\""
              " s=\"" << uproc.id << "\" v=\"0\""
              " f=\"" << exml.file_unknown.id << "\" l=\"0\""
              " it=\"" << id << "\">\n";
      post = "</C>\n";
    }
    ss << "<PF i=\"" << id << "\""
             " n=\"" << uproc.id << "\" s=\"" << uproc.id << "\""
             " f=\"" << exml.file_unknown.id << "\" l=\"0\""
             " lm=\"" << exml.unknown_module.id << "\"";
    close = "</PF>\n";
    open = ss.str();
    onlyOutputWithChildren = true;
    tagIsC = false;
    hasSiblingS = false;
    break;
  }
  case Scope::Type::placeholder: {
    if(proc.prep()) {  // Create a (fake) Procedure for this marker context
      auto pretty = s.enumerated_pretty_name();
      if(!pretty.empty())
        proc.setTag(std::string(pretty), 0, 1);
      else {
        std::ostringstream ss;
        ss << "<unrecognized placeholder: " << s.enumerated_fallback_name() << ">";
        proc.setTag(ss.str(), 0, 1);
      }
    }
    exml.file_unknown.incr(exml);
    std::ostringstream ss;
    if(!parentIsC) {
      ss << "<C i=\"-" << id << "\""
              " s=\"" << proc.id << "\" v=\"0\""
              " f=\"" << exml.file_unknown.id << "\" l=\"0\""
              " it=\"" << id << "\">";
      post = "</C>\n";
    }
    ss << "<PF i=\"" << id << "\""
             " n=\"" << proc.id << "\" s=\"" << proc.id << "\""
             " f=\"" << exml.file_unknown.id << "\" l=\"0\""
             " lm=\"" << exml.no_module.id << "\">\n";
    open = ss.str();
    std::ostringstream ssattr;
    ssattr << " i=\"-" << id << "\""
              " s=\"" << proc.id << "\" v=\"0\""
              " f=\"" << exml.file_unknown.id << "\" l=\"0\""
              " it=\"" << id << "\"";
    attr = ssattr.str();
    post = "</PF>\n" + post;
    tagIsC = true;
    hasSiblingS = false;
    break;
  }
  case Scope::Type::point: {
    auto mo = s.point_data();
    if(parentIsC) {
      // We don't seem to have lexical context, so note as <unknown proc>
      if(proc.prep()) {  // Create a Procedure for this special <unknown proc>
        std::ostringstream ss;
        ss << "<unknown procedure> 0x" << std::hex << mo.second
           << " [" << mo.first.path().filename().string() << "]";
        proc.setTag(ss.str(), mo.second, 1);
      }
      auto& udm = mo.first.userdata[exml.ud];
      udm.unknown_file.incr(exml);
      udm.incr(mo.first, exml);
      std::ostringstream ss;
      ss << "<PF i=\"" << id << "\""
               " n=\"" << proc.id << "\" s=\"" << proc.id << "\""
               " l=\"0\" f=\"" << udm.unknown_file.id << "\""
               " lm=\"" << udm.id << "\">\n";
      open = ss.str();
      post = "</PF>\n";
    }
    std::ostringstream ss;
    uncleS.first.get().incr(exml);
    ss << " i=\"" << (parentIsC ? "-" : "") << id << "\""
          " s=\"" << proc.id << "\" "
          " f=\"" << uncleS.first.get().id << "\" l=\"" << uncleS.second << "\""
          " v=\"0x" << std::hex << mo.second << std::dec << "\""
          " it=\"" << id << "\"";
    attr = ss.str();
    // If our data will be folded into an <S>, we only need the <C> here if we
    // have children.
    onlyOutputWithChildren = hasUncleS;
    tagIsC = true;
    hasSiblingS = false;
    break;
  }
  case Scope::Type::line: {
    auto fl = s.line_data();
    std::ostringstream ss;
    auto& udf = fl.first.userdata[exml.ud];
    siblingS = {udf, fl.second};

    udf.incr(exml);
    if(parentIsC) {
      // Lines without *any* lexical context are noted as <unknown proc>
      auto& fproc = exml.proc_unknown();
      ss << "<PF i=\"" << id << "\""
               " n=\"" << fproc.id << "\" s=\"" << fproc.id << "\""
               " f=\"" << udf.id << "\" l=\"" << fl.second << "\""
               " lm=\"" << exml.unknown_module.id << "\">\n";
      post = "</PF>\n";
    }

    ss << "<S i=\"" << (parentIsC ? "-" : "") << id << "\""
            " it=\"" << id << "\""
            " f=\"" << udf.id << "\" l=\"" << siblingS.second << "\""
            " s=\"" << proc.id << "\""
            " v=\"0\"/>\n";
    open = ss.str();
    openIsClosedTag = true;
    tagIsC = false;  // It's PF
    hasSiblingS = true;
    break;
  }
  case Scope::Type::loop: {
    const auto fl = s.line_data();
    auto& udf = fl.first.userdata[exml.ud];
    udf.incr(exml);
    std::ostringstream ss;
    ss << "<L i=\"" << id << "\""
            " s=\"" << proc.id << "\" v=\"0\""
            " f=\"" << udf.id << "\" l=\"" << fl.second << "\"";
    open = ss.str();
    close = "</L>\n";
    tagIsC = false;  // It's L
    hasSiblingS = false;
    break;
  }
  case Scope::Type::function: {
    const auto& f = s.function_data();
    if(proc.prep()) {  // Create the Procedure for this Function
      const auto& name = f.name();
      if(name.empty()) { // Anonymous function, write as an <unknown proc>
        std::ostringstream ss;
        ss << "<unknown procedure>";
        if(auto o = f.offset()) ss << " 0x" << std::hex << *o;
        ss << " [" << f.module().path().string() << "]";
        proc.setTag(ss.str(), f.offset().value_or(0), 1);
      } else {  // Normal function
        proc.setTag(name, f.offset().value_or(0), 0);
      }
    }

    auto& udm = f.module().userdata[exml.ud];
    udm.incr(f.module(), exml);

    std::ostringstream ss;
    if(!parentIsC) {
      // We need an extra C tag around the outside. If we have an enclosing line
      // Scope (in range) we'll use that for the data.
      ss << "<C i=\"-" << id << "\" it=\"" << id << "\""
              " s=\"" << proc.id << "\" v=\"0\"";
      if(hasUncleS) {
        uncleS.first.get().incr(exml);
        ss << " f=\"" << uncleS.first.get().id << "\" l=\"" << uncleS.second << "\">\n";
      } else {
        // ...Guess it's just called from nowhere.
        exml.file_unknown.incr(exml);
        ss << " f=\"" << exml.file_unknown.id << "\" l=\"0\">\n";
      }
      post = "</C>\n" + post;
    }
    ss << "<PF";
    close = "</PF>\n";

    auto src = f.sourceLocation();
    auto& udf = src ? src->first.userdata[exml.ud] : udm.unknown_file;
    udf.incr(exml);
    ss << " i=\"" << c.userdata[exml.src.identifier()] << "\""
          " s=\"" << proc.id << "\" n=\"" << proc.id << "\""
          " v=\"0x" << std::hex << f.offset().value_or(0) << std::dec << "\""
          " f=\"" << udf.id << "\" l=\"" << (src ? src->second : 0) << "\""
          " lm=\"" << udm.id << "\"";
    open = ss.str();
    tagIsC = false;  // It's PF
    hasSiblingS = false;
    break;
  }
  }
}

// ExperimentXML4 bits

ExperimentXML4::ExperimentXML4(const fs::path& out, bool srcs, HPCTraceDB2* db)
  : ProfileSink(), dir(out), of(), next_id(0x7FFFFFFF), tracedb(db),
    include_sources(srcs), file_unknown(*this), next_statid(-1), next_procid(2),
    proc_unknown_proc(0), proc_partial_proc(1), unknown_module(*this),
    no_module(*this, 0), next_cid(0x7FFFFFFF) {
  if(dir.empty()) {  // Dry run
    util::log::info() << "ExperimentXML4 issuing a dry run!";
  } else {
    stdshim::filesystem::create_directory(dir);
    of.open((dir / "experiment.xml").native());
  }
}

ExperimentXML4::Proc& ExperimentXML4::getProc(const Scope& k) {
  return procs.emplace(k, next_procid.fetch_add(1, std::memory_order_relaxed)).first;
}

const ExperimentXML4::Proc& ExperimentXML4::proc_unknown() {
  proc_unknown_flag.call_nowait([&](){
    proc_unknown_proc.setTag("<unknown>", 0, 1);
  });
  return proc_unknown_proc;
}

const ExperimentXML4::Proc& ExperimentXML4::proc_partial() {
  proc_partial_flag.call_nowait([&](){
    proc_partial_proc.setTag("<partial call paths>", 0, 1);
  });
  return proc_partial_proc;
}

void ExperimentXML4::Proc::setTag(std::string n, std::size_t v, int fake) {
  std::ostringstream ss;
  ss << "<Procedure i=\"" << id << "\" n=" << util::xmlquoted(n)
     << " v=\"" << std::hex << (v == 0 ? "" : "0x") << v << "\"";
  if(fake > 0) ss << " f=\"" << fake << "\"";
  ss << "/>\n";
  tag = ss.str();
}

bool ExperimentXML4::Proc::prep() {
  bool x = false;
  return done.compare_exchange_strong(x, true, std::memory_order_relaxed);
}

void ExperimentXML4::notifyPipeline() noexcept {
  auto& ss = src.structs();
  ud.file = ss.file.add<udFile>(std::ref(*this));
  ud.context = ss.context.add<udContext>(std::ref(*this));
  ud.module = ss.module.add<udModule>(std::ref(*this));
  ud.metric = ss.metric.add<udMetric>(std::ref(*this));
}

void ExperimentXML4::write() {
  const auto& o_name = src.attributes().name();
  std::string_view name = o_name ? *o_name : std::string_view{};
  of << "<?xml version=\"1.0\"?>\n"
        "<HPCToolkitExperiment version=\"4.0\">\n"
        "<Header n=" << util::xmlquoted(name) << ">\n"
        "<Info/>\n"
        "</Header>\n"
        "<SecCallPathProfile i=\"0\" n=" << util::xmlquoted(name) << ">\n"
        "<SecHeader>\n";

  of << "<IdentifierNameTable>\n";
  for(const auto& kv: src.attributes().idtupleNames())
    of << "<Identifier i=\"" << kv.first << "\" n=" << util::xmlquoted(kv.second) << "/>\n";
  of << "</IdentifierNameTable>\n";

  // MetricTable: from the Metrics
  of << "<MetricTable>\n";
  for(const auto& m: src.metrics().iterate()) {
    const auto& udm = m().userdata[ud];
    of << udm.metric_tags;
  }
  for(const auto& es: src.extraStatistics().iterate())
    of << eStatMetricTags(es);
  of << "</MetricTable>\n";

  of << "<MetricDBTable>\n";
  for(const auto& m: src.metrics().iterate()) of << m().userdata[ud].metricdb_tags;
  of << "</MetricDBTable>\n";
  if(tracedb != nullptr)
    of << "<TraceDBTable>\n" << tracedb->exmlTag() << "</TraceDBTable>\n";
  of << "<LoadModuleTable>\n";
  // LoadModuleTable: from the Modules
  of << unknown_module.tag << no_module.tag;
  for(const auto& m: src.modules().iterate()) {
    auto& udm = m().userdata[ud];
    if(!udm) continue;
    of << udm.tag;
  }
  of << "</LoadModuleTable>\n"
        "<FileTable>\n";
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
  of << "</FileTable>\n"
        "<ProcedureTable>\n";
  // ProcedureTable: from the Functions for each Module.
  for(const auto& sp: procs.iterate()) of << sp.second.tag;
  if(proc_unknown_flag.query()) of << proc_unknown_proc.tag;
  if(proc_partial_flag.query()) of << proc_partial_proc.tag;
  of << "</ProcedureTable>\n"

        "<Info/>\n"
        "</SecHeader>\n";

  // Early check: the global Context must have id 0
  assert(src.contexts().userdata[src.identifier()] == 0 && "Global Context must have id 0!");

  // Spit out the CCT
  src.contexts().citerate([&](const Context& c){
    auto& udc = c.userdata[ud];
    if(udc.onlyOutputWithChildren && c.children().empty())
      return;

    of << udc.open;
    if(udc.attr.empty()) {
      if(udc.open.empty() || udc.openIsClosedTag)
        return;
    } else
      of << (c.children().empty() ? "<S" : "<C") << udc.attr;

    // Close the tag. If we have no children, use the shortened tag syntax.
    of << (c.children().empty() ? "/>\n" : ">\n");
  }, [&](const Context& c){
    auto& udc = c.userdata[ud];
    if(!c.children().empty()) {
      if(!udc.attr.empty()) of << "</C>\n";
      of << udc.close;
      if(udc.onlyOutputWithChildren) of << udc.post;
    }
    if(!udc.onlyOutputWithChildren) of << udc.post;
  });

  of << "</SecCallPathProfile>\n"
        "</HPCToolkitExperiment>\n" << std::flush;
}
