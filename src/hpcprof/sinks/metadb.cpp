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
// Copyright ((c)) 2002-2024, Rice University
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

#include "../util/vgannotations.hpp"

#include "metadb.hpp"

#include "../util/log.hpp"
#include "../util/once.hpp"

#include "../formats/metadb.hpp"
#include "../formats/primitives.hpp"

#include "../../common/prof-lean/placeholders.h"

#include <fstream>
#include <stack>

using namespace hpctoolkit;
using namespace sinks;

MetaDB::MetaDB(stdshim::filesystem::path in_dir, bool copySources)
  : dir(std::move(in_dir)), copySources(copySources) {

  if(dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);

  metadb = util::File(dir / "meta.db", true);
}

void MetaDB::notifyPipeline() noexcept {
  auto& ss = src.structs();
  ud.file = ss.file.add_default<udFile>();
  ud.module = ss.module.add_default<udModule>();
  ud.context = ss.context.add_default<udContext>();
}

template<class I>
static constexpr I align(I v, uint8_t a) {
  return (v + a - 1) / a * a;
}

std::string MetaDB::accumulateFormulaString(const Expression& e) {
  std::ostringstream ss;
  std::stack<bool, std::vector<bool>> first;
  first.push(true);
  std::stack<std::string, std::vector<std::string>> infix;
  infix.push("!!!");
  e.citerate_all(
    [&](double v){
      if(!first.top()) ss << infix.top();
      first.top() = false;
      ss << v;
    },
    [&](Expression::uservalue_t){
      if(!first.top()) ss << infix.top();
      first.top() = false;
      ss << "$$";
    },
    [&](const Expression& e) {
      if(!first.top()) ss << infix.top();
      first.top() = false;
      std::string fix = "!!!";
      switch(e.kind()) {
      case Expression::Kind::constant:
      case Expression::Kind::subexpression:
      case Expression::Kind::variable:
        std::abort();
      case Expression::Kind::op_sum:  ss << '('; fix = "+"; break;
      case Expression::Kind::op_sub:  ss << '('; fix = "-"; break;
      case Expression::Kind::op_neg:  ss << "-("; break;
      case Expression::Kind::op_prod: ss << '('; fix = "*"; break;
      case Expression::Kind::op_div:  ss << '('; fix = "/"; break;
      case Expression::Kind::op_pow:  ss << '('; fix = "^"; break;
      case Expression::Kind::op_sqrt: ss << "sqrt("; break;
      case Expression::Kind::op_log:  ss << "log("; fix = ","; break;
      case Expression::Kind::op_ln:   ss << "ln(";break;
      case Expression::Kind::op_min:  ss << "min("; fix = ","; break;
      case Expression::Kind::op_max:  ss << "max("; fix = ","; break;
      case Expression::Kind::op_floor: ss << "floor("; break;
      case Expression::Kind::op_ceil: ss << "ceil("; break;
      }
      first.push(true);
      infix.push(std::move(fix));
    },
    [&](const Expression&) {
      first.pop();
      infix.pop();
      ss << ')';
    }
  );
  return ss.str();
}

std::size_t MetaDB::stringsTableLookup(const std::string& s) {
  {
    std::shared_lock<std::shared_mutex> l(stringsLock);
    auto it = stringsTable.find(s);
    if(it != stringsTable.end()) return it->second;
  }
  std::unique_lock<std::shared_mutex> l(stringsLock);
  auto [it, first] = stringsTable.try_emplace(s, stringsList.size());
  if(first)
    stringsList.push_back(it->first);
  return it->second;
}

void MetaDB::instance(const File& f) {
  auto& udf = f.userdata[ud];
  util::call_once(udf.once, [&]{
    if(copySources) {
      const auto& rp = f.userdata[src.resolvedPath()];
      if(!rp.empty()) {
        assert(rp.is_absolute());  // This technique only works on absolute paths
        auto relative = stdshim::filesystem::path("src") / rp.relative_path().lexically_normal();
        auto p = dir / relative;

        bool ok = false;
        try {
          stdshim::filesystem::create_directories(p.parent_path());
          stdshim::filesystem::copy_file(rp, p,
              stdshim::filesystem::copy_options::overwrite_existing);
          ok = true;
        } catch(const stdshim::filesystem::filesystem_error& e) {
          util::log::warning{} << "Failed to copy source file to database: "
            << e.code().message() << " [" << rp.string() << "]";
        }
        if(ok) {
          udf.pathSIdx = stringsTableLookup(relative.string());
          udf.copied = true;
          return;
        }
      }
    }
    udf.pathSIdx = stringsTableLookup(f.path().string());
    udf.copied = false;
  });
}

void MetaDB::instance(const Module& m) {
  auto& udm = m.userdata[ud];
  util::call_once(udm.once, [&]{
    if(m.relative_path().empty()){
      udm.pathSIdx = stringsTableLookup(m.path().string());
    }else{
      udm.pathSIdx = stringsTableLookup(m.relative_path().string());
    }
  });
}

void MetaDB::instance(const Function& f) {
  auto [udf, first] = udFuncs.try_emplace(f);
  if(!first) return;
  udf.nameSIdx = stringsTableLookup(f.name());
  instance(f.module());
  if(auto sl = f.sourceLocation()) instance(sl->first);
}

void MetaDB::instance(Scope::placeholder_t, const Scope& s) {
  auto [udf, first] = udPlaceholders.try_emplace(s.enumerated_data());
  if(!first) return;
  std::string name(s.enumerated_pretty_name());
  if(name.empty()) name = s.enumerated_fallback_name();
  udf.nameSIdx = stringsTableLookup(std::move(name));
}

void MetaDB::notifyContext(const Context& c) {
  const auto& s = c.scope();
  const auto& sf = s.flat();

  if(c.direct_parent() && !c.direct_parent()->direct_parent()) {
    // This is a top-level Context, which will be converted into an entry point,
    // so most of the usual operations don't apply.
    auto& udc = c.userdata[ud];
    switch(sf.type()) {
    case Scope::Type::unknown:
      udc.entryPoint = FMT_METADB_ENTRYPOINT_UNKNOWN_ENTRY;
      udc.prettyNameSIdx = stringsTableLookup("unknown entry");
      break;
    case Scope::Type::placeholder:
      switch(sf.enumerated_data()) {
      case hpcrun_placeholder_fence_main:
        udc.entryPoint = FMT_METADB_ENTRYPOINT_MAIN_THREAD;
        udc.prettyNameSIdx = stringsTableLookup("main thread");
        break;
      case hpcrun_placeholder_fence_thread:
        udc.entryPoint = FMT_METADB_ENTRYPOINT_APPLICATION_THREAD;
        udc.prettyNameSIdx = stringsTableLookup("application thread");
        break;
      default:
        util::log::fatal{} << "Invalid top-level Scope: " << s;
      }
      break;
    default:
      util::log::fatal{} << "Invalid top-level Scope: " << s;
      break;
    }

    return;
  }

  // Otherwise handle it as a normal Context
  switch(sf.type()) {
  case Scope::Type::unknown:
  case Scope::Type::global:
    break;
  case Scope::Type::placeholder:
    instance(Scope::placeholder, sf);
    break;
  case Scope::Type::point:
    instance(sf.point_data().first);
    break;
  case Scope::Type::lexical_loop:
  case Scope::Type::line:
    instance(sf.line_data().first);
    break;
  case Scope::Type::binary_loop:
    instance(sf.point_data().first);
    instance(sf.line_data().first);
    break;
  case Scope::Type::function:
    instance(sf.function_data());
    break;
  }

  auto& udc = c.userdata[ud];
  udc.propagation = 0;
  switch(s.relation()) {
  case Relation::global:
  case Relation::call:
  case Relation::inlined_call:
    break;
  case Relation::enclosure:
    udc.propagation |= 0x1;
    break;
  }
}

void MetaDB::write() try {
  metadb->initialize();
  auto file = metadb->open(true, true);
  formats::LayoutEngine l(file);

  {
    formats::WriteGuard<fmt_metadb_fHdr_t> fileHdr(l);

    { // General Properties section
      formats::SubWriteGuard<fmt_metadb_generalSHdr_t> gphdr(l, *fileHdr);

      formats::Written<std::string_view, formats::NullTerminatedString> title(l,
          src.attributes().name() ? std::string_view(src.attributes().name().value())
                                  : std::string_view("<unnamed>"));
      gphdr->pTitle = title.ptr();

      formats::Written<std::string_view, formats::NullTerminatedString> description(l,
          "TODO database description");
      gphdr->pDescription = description.ptr();
    }

    { // Identifier Names section
      formats::SubWriteGuard<fmt_metadb_idNamesSHdr_t> idhdr(l, *fileHdr);

      std::deque<util::optional_ref<const std::string>> names(src.attributes().idtupleNames().size());
      for(const auto& kv: src.attributes().idtupleNames()) {
        if(names.size() <= kv.first) names.resize(kv.first+1);
        names[kv.first] = kv.second;
      }

      // Write out the names
      formats::Written<std::string, formats::NullTerminatedString> nullStr(l, "");
      std::deque<formats::Written<std::string_view, formats::NullTerminatedString>> strs;
      for(const auto& n: names) {
        if (n)
          strs.emplace_back(l, *n);
      }

      // Then the array of names
      formats::Written pStrs(l, std::move(strs));

      idhdr->nKinds = pStrs->size();
      idhdr->ppNames = pStrs.ptr();
    }

    { // Performance Metrics section
      formats::SubWriteGuard<fmt_metadb_metricsSHdr_t> shdr(l, *fileHdr);

      // First the propagation scopes
      std::map<MetricScope, std::size_t> scopeIdxs;
      auto scopes_f = [&l, &scopeIdxs]() -> auto {
        std::deque<fmt_metadb_propScope_t> scopes;
        for(MetricScope ms: MetricScopeSet(MetricScopeSet::all)) {
          fmt_metadb_propScope_t scope;
          scope.propagationIndex = 255;
          switch(ms) {
          case MetricScope::point:
            scope.pScopeName =
                formats::Written<std::string_view, formats::NullTerminatedString>(l, "point").ptr();
            scope.type = FMT_METADB_SCOPETYPE_Point;
            break;
          case MetricScope::function:
            scope.pScopeName =
                formats::Written<std::string_view, formats::NullTerminatedString>(l, "function").ptr();
            scope.type = FMT_METADB_SCOPETYPE_Transitive;
            scope.propagationIndex = 0;
            break;
          case MetricScope::lex_aware:
            scope.pScopeName =
                formats::Written<std::string_view, formats::NullTerminatedString>(l, "lex_aware").ptr();
            scope.type = FMT_METADB_SCOPETYPE_Custom;
            break;
          case MetricScope::execution:
            scope.pScopeName =
                formats::Written<std::string_view, formats::NullTerminatedString>(l, "execution").ptr();
            scope.type = FMT_METADB_SCOPETYPE_Execution;
            break;
          }
          scopeIdxs[ms] = scopes.size();
          scopes.push_back(scope);
        }
        return scopes;
      };
      formats::Written scopes(l, scopes_f());
      shdr->pScopes = scopes.ptr();
      shdr->nScopes = static_cast<std::uint16_t>(scopes->size());

      // Next the metrics
      auto metrics_f = [this, &l, &scopeIdxs, &scopes]() -> auto {
        std::deque<fmt_metadb_metricDesc_t> metrics;
        for(const Metric& m: src.metrics().citerate()) {
          const Metric::Identifier& id = m.userdata[src.identifier()];

          auto scopeInsts_f = [&m, &id, &scopeIdxs, &scopes]() -> auto {
            std::deque<fmt_metadb_propScopeInst_t> scopeInsts;
            for(MetricScope ms: m.scopes()) {
              scopeInsts.push_back((fmt_metadb_propScopeInst_t){
                .pScope = scopes.ptr(scopeIdxs.at(ms)),
                .propMetricId = static_cast<uint16_t>(id.getFor(ms)),
              });
            }
            return scopeInsts;
          };
          formats::Written scopeInsts(l, scopeInsts_f());

          auto summaries_f = [&m, &id, &l, &scopeIdxs, &scopes]() -> auto {
            std::deque<fmt_metadb_summaryStat_t> summaries;
            for(MetricScope ms: m.scopes()) {
              auto pScope = scopes.ptr(scopeIdxs.at(ms));
              for(const auto& p: m.partials()) {
                std::uint8_t combine = 255;
                switch(p.combinator()) {
                case Statistic::combination_t::sum:
                  combine = FMT_METADB_COMBINE_Sum;
                  break;
                case Statistic::combination_t::min:
                  combine = FMT_METADB_COMBINE_Min;
                  break;
                case Statistic::combination_t::max:
                  combine = FMT_METADB_COMBINE_Max;
                  break;
                }
                summaries.push_back((fmt_metadb_summaryStat_t){
                  .pScope = pScope,
                  .pFormula = formats::Written<std::string, formats::NullTerminatedString>(l,
                      accumulateFormulaString(p.accumulate())).ptr(),
                  .combine = combine,
                  .statMetricId = static_cast<uint16_t>(id.getFor(p, ms)),
                });
              }
            }
            return summaries;
          };
          formats::Written summaries(l, summaries_f());

          metrics.push_back((fmt_metadb_metricDesc_t){
            .pName = formats::Written<std::string_view, formats::NullTerminatedString>(l,
                m.name()).ptr(),
            .pScopeInsts = scopeInsts.ptr(),
            .pSummaries = summaries.ptr(),
            .nScopeInsts = static_cast<std::uint16_t>(scopeInsts->size()),
            .nSummaries = static_cast<std::uint16_t>(summaries->size()),
          });
        }
        return metrics;
      };
      formats::Written metrics(l, metrics_f());
      shdr->pMetrics = metrics.ptr();
      shdr->nMetrics = static_cast<std::uint32_t>(metrics->size());
    }

    // Common String Table (section)
    formats::Written strings(l, std::move(stringsList),
        formats::DynamicArray<formats::NullTerminatedString>());
    fileHdr->pStrings = strings.ptr();
    fileHdr->szStrings = strings.bytesize();

    { // Load Modules section
      formats::SubWriteGuard<fmt_metadb_modulesSHdr_t> shdr(l, *fileHdr);

      std::deque<std::reference_wrapper<udModule>> moduleUds;
      auto modules_f = [&]() -> auto {
        std::deque<fmt_metadb_moduleSpec_t> modules;
        for(const Module& m: src.modules().citerate()) {
          auto& udm = m.userdata[ud];
          if(udm.pathSIdx == std::numeric_limits<std::size_t>::max()) continue;
          moduleUds.emplace_back(std::ref(udm));
          modules.push_back((fmt_metadb_moduleSpec_t){
            .pPath = strings.ptr(udm.pathSIdx),
          });
        }
        return modules;
      };
      formats::Written modules(l, modules_f());
      shdr->pModules = modules.ptr();
      shdr->nModules = modules->size();
      std::size_t i = 0;
      for(udModule& ud: moduleUds)
        ud.ptr = modules.ptr(i++);
    }

    { // Source Files section
      formats::SubWriteGuard<fmt_metadb_filesSHdr_t> shdr(l, *fileHdr);

      std::deque<std::reference_wrapper<udFile>> fileUds;
      auto files_f = [&]() -> auto {
        std::deque<fmt_metadb_fileSpec_t> files;
        for(const File& ff: src.files().citerate()) {
          auto& udf = ff.userdata[ud];
          if(udf.pathSIdx == std::numeric_limits<std::size_t>::max()) continue;
          fileUds.emplace_back(std::ref(udf));
          files.push_back((fmt_metadb_fileSpec_t){
            .copied = udf.copied,
            .pPath = strings.ptr(udf.pathSIdx),
          });
        }
        return files;
      };
      formats::Written files(l, files_f());
      shdr->pFiles = files.ptr();
      shdr->nFiles = files->size();
      std::size_t i = 0;
      for(udFile& ud: fileUds)
        ud.ptr = files.ptr(i++);
    }

    { // Functions section
      formats::SubWriteGuard<fmt_metadb_functionsSHdr_t> shdr(l, *fileHdr);

      std::deque<std::reference_wrapper<udFunction>> functionUds;
      auto functions_f = [&]() -> auto {
        std::deque<fmt_metadb_functionSpec_t> functions;
        for(auto& [rff, udf]: udFuncs.iterate()) {
          const Function& ff = rff;
          functionUds.emplace_back(std::ref(udf));
          auto sl = ff.sourceLocation();
          functions.push_back((fmt_metadb_functionSpec_t){
            .pName = strings.ptr(udf.nameSIdx),
            .pModule = ff.module().userdata[ud].ptr,
            .offset = ff.offset().value_or(0),
            .pFile = sl ? sl->first.userdata[ud].ptr : 0,
            .line = sl ? static_cast<uint32_t>(sl->second) : 0,
          });
        }
        for(auto& [dat, udf]: udPlaceholders.iterate()) {
          functionUds.emplace_back(std::ref(udf));
          functions.push_back((fmt_metadb_functionSpec_t){
            .pName = strings.ptr(udf.nameSIdx),
            .pModule = 0, .offset = 0,
            .pFile = 0, .line = 0,
          });
        }
        return functions;
      };
      formats::Written functions(l, functions_f());
      shdr->pFunctions = functions.ptr();
      shdr->nFunctions = functions->size();
      std::size_t i = 0;
      for(udFunction& ud: functionUds)
        ud.ptr = functions.ptr(i++);
    }

    { // Context Tree section
      formats::SubWriteGuard<fmt_metadb_contextsSHdr_t> shdr(l, *fileHdr);

      const auto compose = [this](const Context& c) -> fmt_metadb_context_t {
        const auto& udc = c.userdata[ud];
        fmt_metadb_context_t ctx;
        ctx.szChildren = udc.szChildren;
        ctx.pChildren = udc.pChildren;
        ctx.ctxId = c.userdata[src.identifier()];
        ctx.propagation = udc.propagation;
        ctx.pFunction = ctx.pFile = ctx.pModule = 0;
        switch(c.scope().relation()) {
        case Relation::global: std::abort();
        case Relation::enclosure:
          ctx.relation = FMT_METADB_RELATION_LexicalNest;
          break;
        case Relation::call:
          ctx.relation = FMT_METADB_RELATION_Call;
          break;
        case Relation::inlined_call:
          ctx.relation = FMT_METADB_RELATION_InlinedCall;
          break;
        }

        const auto setFunction = [&](const udFunction& ud){
          ctx.pFunction = ud.ptr;
          assert(ctx.pFunction != std::numeric_limits<uint64_t>::max());
        };
        const auto setSrcLine = [&]{
          const auto [f, l] = c.scope().flat().line_data();
          ctx.pFile = f.userdata[ud].ptr;
          assert(ctx.pFile != std::numeric_limits<uint64_t>::max());
          ctx.line = l;
        };
        const auto setPoint = [&]{
          const auto [m, o] = c.scope().flat().point_data();
          ctx.pModule = m.userdata[ud].ptr;
          assert(ctx.pModule != std::numeric_limits<uint64_t>::max());
          ctx.offset = o;
        };

        switch(c.scope().flat().type()) {
        case Scope::Type::global: std::abort();
        case Scope::Type::unknown:
          ctx.lexicalType = FMT_METADB_LEXTYPE_Function;
          // ...But we don't know the function, so pFunction = 0 still.
          break;
        case Scope::Type::function:
          ctx.lexicalType = FMT_METADB_LEXTYPE_Function;
          setFunction(udFuncs.at(c.scope().flat().function_data()));
          break;
        case Scope::Type::placeholder:
          ctx.lexicalType = FMT_METADB_LEXTYPE_Function;
          setFunction(udPlaceholders.at(c.scope().flat().enumerated_data()));
          break;
        case Scope::Type::line:
          ctx.lexicalType = FMT_METADB_LEXTYPE_Line;
          setSrcLine();
          break;
        case Scope::Type::lexical_loop:
        case Scope::Type::binary_loop:
          ctx.lexicalType = FMT_METADB_LEXTYPE_Loop;
          setSrcLine();
          if(c.scope().flat().type() == Scope::Type::binary_loop)
            setPoint();
          break;
        case Scope::Type::point:
          ctx.lexicalType = FMT_METADB_LEXTYPE_Instruction;
          setPoint();
          break;
        }
        return ctx;
      };

      auto entryPoints_f = [&]() -> auto {
        std::deque<fmt_metadb_entryPoint_t> entryPoints;
        for(const Context& top: src.contexts().children().citerate()) {
          // Output Contexts in reversed DFS order, since that's easier to implement
          top.citerate(nullptr, [&](const Context& c){
            if(c.children().empty()) return;
            if(elide(c)) return;

            auto& udc = c.userdata[ud];
            auto children_f = [&]() -> auto {
              std::deque<fmt_metadb_context_t> children;
              for(const Context& cc: c.children().citerate()) {
                if(elide(cc)) {
                  for(const Context& gcc: cc.children().citerate()) {
                    assert(!elide(gcc) && "Recursion needed for this algorithm!");
                    children.push_back(compose(gcc));
                  }
                } else {
                  children.push_back(compose(cc));
                }
              }
              return children;
            };
            formats::Written children(l, children_f());
            udc.pChildren = children.ptr();
            udc.szChildren = children.bytesize();
          });

          // Then compose the entry point
          auto& udc = top.userdata[ud];
          entryPoints.push_back((fmt_metadb_entryPoint_t){
            .szChildren = udc.szChildren, .pChildren = udc.pChildren,
            .ctxId = top.userdata[src.identifier()],
            .entryPoint = udc.entryPoint,
            .pPrettyName = strings.ptr(udc.prettyNameSIdx),
          });
        }
        return entryPoints;
      };
      formats::Written entryPoints(l, entryPoints_f());
      shdr->pEntryPoints = entryPoints.ptr();
      shdr->nEntryPoints = entryPoints->size();
    }
  }

  // File footer
  file.writeat(l.size(), sizeof fmt_metadb_footer, fmt_metadb_footer);
} catch(const std::exception& e) {
  util::log::fatal{} << "Error while writing meta.db: " << e.what();
}
