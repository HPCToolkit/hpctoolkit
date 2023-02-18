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
// Copyright ((c)) 2002-2023, Rice University
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

#include "lib/prof-lean/formats/metadb.h"
#include "lib/prof-lean/formats/primitive.h"
#include "lib/prof-lean/placeholders.h"

#include <fstream>
#include <stack>

using namespace hpctoolkit;
using namespace sinks;

MetaDB::MetaDB(stdshim::filesystem::path dir, bool copySources)
  : dir(std::move(dir)), copySources(copySources) {};

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

uint64_t MetaDB::stringsTableLookup(const std::string& s) {
  {
    std::shared_lock<std::shared_mutex> l(stringsLock);
    auto it = stringsTable.find(s);
    if(it != stringsTable.end()) return it->second;
  }
  std::unique_lock<std::shared_mutex> l(stringsLock);
  auto [it, first] = stringsTable.try_emplace(s, stringsCursor);
  if(first) {
    stringsCursor += s.size() + 1;
    stringsList.push_back(it->first);
  }
  return it->second;
}

void MetaDB::instance(const File& f) {
  auto& udf = f.userdata[ud];
  util::call_once(udf.once, [&]{
    if(copySources) {
      const auto& rp = f.userdata[src.resolvedPath()];
      if(!rp.empty()) {
        assert(rp.is_absolute());  // This technique only works on absolute paths
        auto p = stdshim::filesystem::path("src") / rp.relative_path().lexically_normal();
        udf.pPath_base = stringsTableLookup(p.string());
        p = dir / p;
        stdshim::filesystem::create_directories(p.parent_path());
        stdshim::filesystem::copy_file(rp, p,
            stdshim::filesystem::copy_options::overwrite_existing);
        udf.copied = true;
        return;
      }
    }
    udf.pPath_base = stringsTableLookup(f.path().string());
    udf.copied = false;
  });
}

void MetaDB::instance(const Module& m) {
  auto& udm = m.userdata[ud];
  util::call_once(udm.once, [&]{
    udm.pPath_base = stringsTableLookup(m.path().string());
  });
}

void MetaDB::instance(const Function& f) {
  auto [udf, first] = udFuncs.try_emplace(f);
  if(!first) return;
  udf.pName_base = stringsTableLookup(f.name());
  instance(f.module());
  if(auto sl = f.sourceLocation()) instance(sl->first);
}

void MetaDB::instance(Scope::placeholder_t, const Scope& s) {
  auto [udf, first] = udPlaceholders.try_emplace(s.enumerated_data());
  if(!first) return;
  std::string name(s.enumerated_pretty_name());
  if(name.empty()) name = s.enumerated_fallback_name();
  udf.pName_base = stringsTableLookup(std::move(name));
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
      udc.pPrettyName_base = stringsTableLookup("unknown entry");
      break;
    case Scope::Type::placeholder:
      switch(sf.enumerated_data()) {
      case hpcrun_placeholder_fence_main:
        udc.entryPoint = FMT_METADB_ENTRYPOINT_MAIN_THREAD;
        udc.pPrettyName_base = stringsTableLookup("main thread");
        break;
      case hpcrun_placeholder_fence_thread:
        udc.entryPoint = FMT_METADB_ENTRYPOINT_APPLICATION_THREAD;
        udc.pPrettyName_base = stringsTableLookup("application thread");
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
  case Scope::Type::loop:
  case Scope::Type::line:
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
  stdshim::filesystem::create_directory(dir);
  std::ofstream f(dir / "meta.db", std::ios_base::out
      | std::ios_base::trunc | std::ios_base::binary);
  f.exceptions(std::ios_base::badbit | std::ios_base::failbit);

  fmt_metadb_fHdr_t filehdr = {
    0, 0,
    1, 1,
    2, 2,
    3, 3,
    4, 4,
    5, 5,
    6, 6,
    7, 7
  };
  f.seekp(FMT_METADB_SZ_FHdr, std::ios_base::beg);
  uint64_t cursor = FMT_METADB_SZ_FHdr;

  { // General Properties section
    f.seekp(filehdr.pGeneral = align(cursor, 8), std::ios_base::beg);

    const std::string defaultTitle = "<unnamed>";
    const auto& title = src.attributes().name().value_or(defaultTitle);
    const std::string description = "TODO database description";

    // Section header
    fmt_metadb_generalSHdr_t gphdr;
    gphdr.pTitle = filehdr.pGeneral + FMT_METADB_SZ_GeneralSHdr;
    gphdr.pDescription = gphdr.pTitle + title.size() + 1;
    cursor = gphdr.pDescription + description.size() + 1;
    filehdr.szGeneral = cursor - filehdr.pGeneral;
    char buf[FMT_METADB_SZ_GeneralSHdr];
    fmt_metadb_generalSHdr_write(buf, &gphdr);
    f.write(buf, sizeof buf);

    // *pTitle string
    f.write(title.data(), title.size());
    f.put('\0');

    // *pDescription string
    f.write(description.data(), description.size());
    f.put('\0');
  }

  { // Identifier Names section
    f.seekp(filehdr.pIdNames = cursor = align(cursor, 8), std::ios_base::beg);

    std::deque<util::optional_ref<const std::string>>
        names(src.attributes().idtupleNames().size());
    uint64_t nameSize = 0;
    for(const auto& kv: src.attributes().idtupleNames()) {
      if(names.size() <= kv.first) names.resize(kv.first+1);
      names[kv.first] = kv.second;
      nameSize += kv.second.size() + 1; // for NUL
    }

    // Section header
    fmt_metadb_idNamesSHdr_t idhdr;
    idhdr.ppNames = align(filehdr.pIdNames + FMT_METADB_SZ_IdNamesSHdr, 8);
    idhdr.nKinds = names.size();
    char buf[FMT_METADB_SZ_IdNamesSHdr];
    fmt_metadb_idNamesSHdr_write(buf, &idhdr);
    f.write(buf, sizeof buf);

    // *ppNames array
    f.seekp(cursor = idhdr.ppNames, std::ios_base::beg);
    auto firstName = cursor += 8 * idhdr.nKinds;
    cursor += 1;  // "First" name is an empty string, for std::nullopt. Probably never used.
    for(const auto& n: names) {
      char buf[8];
      fmt_u64_write(buf, n ? cursor : firstName);
      f.write(buf, sizeof buf);
      if(n) cursor += n->size() + 1;
    }
    f.put('\0');  // "First" empty name
    filehdr.szIdNames = cursor - filehdr.pIdNames;

    // **ppNames strings
    for(const auto& n: names) {
      if(n) {
        f.write(n->data(), n->size());
        f.put('\0');
      }
    }
  }

  { // Performance Metrics section
    f.seekp(filehdr.pMetrics = cursor = align(cursor, 8), std::ios_base::beg);

    // section header
    fmt_metadb_metricsSHdr_t shdr = {
      .pMetrics = 0,
      .nMetrics = static_cast<uint32_t>(src.metrics().size()),
      .szMetric = 0, .szScopeInst = 0, .szSummary = 0,
      .pScopes = align(cursor + FMT_METADB_SZ_MetricsSHdr, 8),
      .nScopes = static_cast<uint16_t>(MetricScopeSet().size()),
      .szScope = 0,
    };
    shdr.pMetrics = align(shdr.pScopes + shdr.nScopes*FMT_METADB_SZ_PropScope, 8);
    {
      char buf[FMT_METADB_SZ_MetricsSHdr];
      fmt_metadb_metricsSHdr_write(buf, &shdr);
      f.write(buf, sizeof buf);
    }

    // The *pMetrics subarrays go after the *pScopes array, keep a cursor there
    uint64_t subCursor = align(shdr.pMetrics + shdr.nMetrics*FMT_METADB_SZ_MetricDesc, 8);

    // The string table goes at the end of the section. Precalculate exactly
    // where that will end up being.
    uint64_t stringCursor = subCursor;
    for(const Metric& m: src.metrics().citerate()) {
      MetricRef mr = m;
      stringCursor = align(stringCursor, 8) + mr.scopesSize();
    }
    auto stringsStart = stringCursor;
    std::unordered_map<std::string, uint64_t> stringTable;
    std::deque<std::string_view> strings;

    // The *pScopes array goes right after the section header, so write that out
    // first before moving on.
    f.seekp(shdr.pScopes, std::ios_base::beg);
    for(MetricScope ms: MetricScopeSet(MetricScopeSet::all)) {
      auto str = [&](std::string_view sv){
        strings.push_back(sv);
        auto ret = stringCursor;
        stringCursor += sv.size() + 1;
        return ret;
      };

      fmt_metadb_propScope_t scope;
      scope.propagationIndex = 255;
      switch(ms) {
      case MetricScope::point:
        scope.pScopeName = str("point");
        scope.type = FMT_METADB_SCOPETYPE_Point;
        break;
      case MetricScope::function:
        scope.pScopeName = str("function");
        scope.type = FMT_METADB_SCOPETYPE_Transitive;
        scope.propagationIndex = 0;
        break;
      case MetricScope::lex_aware:
        scope.pScopeName = str("lex_aware");
        scope.type = FMT_METADB_SCOPETYPE_Custom;
        break;
      case MetricScope::execution:
        scope.pScopeName = str("execution");
        scope.type = FMT_METADB_SCOPETYPE_Execution;
        break;
      }
      char buf[FMT_METADB_SZ_PropScope];
      fmt_metadb_propScope_write(buf, &scope);
      f.write(buf, sizeof buf);
    }

    // Skip and write out the subarrays first, using a string table to
    // save and merge shared strings for the table later.
    // All the arrays are 8-aligned and 8-sized, so we don't need to align here.
    f.seekp(subCursor, std::ios_base::beg);
    std::unordered_map<util::reference_index<const Metric>, uint64_t> pScopes;
    for(const Metric& m: src.metrics().citerate()) {
      MetricRef mr = m;
      auto subarrays = mr.composeSubarrays(subCursor, shdr.pScopes, m.userdata[src.identifier()],
        [&](const std::string& s){
          auto [it, first] = stringTable.try_emplace(s, stringCursor);
          if(first) {
            stringCursor += s.size() + 1;
            strings.push_back(it->first);
          }
          return it->second;
        });
      f.write(subarrays.data(), subarrays.size());
      pScopes.emplace(m, subCursor);
      subCursor += subarrays.size();
    }

    // Seek back and write out the *pMetrics array.
    {
      std::vector<char> buf(shdr.nMetrics * FMT_METADB_SZ_MetricDesc);
      size_t metricCursor = 0;
      for(const auto& [m, ps]: pScopes) {
        MetricRef mr = m.get();
        mr.compose(&buf.at(metricCursor), ps, [&](std::string_view sv){
          strings.push_back(sv);
          auto ret = stringCursor;
          stringCursor += sv.size() + 1;
          return ret;
        });
        metricCursor += FMT_METADB_SZ_MetricDesc;
      }
      f.seekp(shdr.pMetrics, std::ios_base::beg);
      f.write(buf.data(), buf.size());
    }

    // Seek forward and write out all the strings we've been saving
    f.seekp(stringsStart, std::ios_base::beg);
    for(const auto& sv: strings) {
      f.write(sv.data(), sv.size());
      f.put('\0');
    }
    cursor = stringCursor;
    filehdr.szMetrics = cursor - filehdr.pMetrics;
  }

  { // Common String Table section
    filehdr.pStrings = cursor;
    for(const auto& sv: stringsList) {
      f.write(sv.data(), sv.size());
      f.put('\0');
      cursor += sv.size() + 1;
    }
    filehdr.szStrings = cursor - filehdr.pStrings;
  }

  { // Load Modules section
    filehdr.pModules = cursor = align(cursor, 8);

    fmt_metadb_modulesSHdr_t shdr = {
      .pModules = align(cursor + FMT_METADB_SZ_ModulesSHdr, 8),
      .nModules = 0,
    };

    f.seekp(cursor = shdr.pModules, std::ios_base::beg);
    for(const Module& m: src.modules().citerate()) {
      auto& udm = m.userdata[ud];
      if(udm.pPath_base == std::numeric_limits<uint64_t>::max()) continue;
      shdr.nModules++;
      udm.ptr = cursor;

      fmt_metadb_moduleSpec_t mspec = {
        .pPath = filehdr.pStrings + udm.pPath_base,
      };
      char buf[FMT_METADB_SZ_ModuleSpec];
      fmt_metadb_moduleSpec_write(buf, &mspec);
      f.write(buf, sizeof buf);
      cursor += sizeof buf;
    }
    filehdr.szModules = cursor - filehdr.pModules;

    // Seek back and write the section header
    f.seekp(filehdr.pModules, std::ios_base::beg);
    char buf[FMT_METADB_SZ_ModulesSHdr];
    fmt_metadb_modulesSHdr_write(buf, &shdr);
    f.write(buf, sizeof buf);
  }
  // NOTE: cursor and file cursor out of sync here

  { // Source Files section
    filehdr.pFiles = cursor = align(cursor, 8);

    fmt_metadb_filesSHdr_t shdr = {
      .pFiles = align(cursor + FMT_METADB_SZ_FilesSHdr, 8),
      .nFiles = 0,
    };

    f.seekp(cursor = shdr.pFiles, std::ios_base::beg);
    for(const File& ff: src.files().citerate()) {
      auto& udf = ff.userdata[ud];
      if(udf.pPath_base == std::numeric_limits<uint64_t>::max()) continue;
      shdr.nFiles++;
      udf.ptr = cursor;

      fmt_metadb_fileSpec_t fspec = {
        .copied = udf.copied,
        .pPath = filehdr.pStrings + udf.pPath_base,
      };
      char buf[FMT_METADB_SZ_FileSpec];
      fmt_metadb_fileSpec_write(buf, &fspec);
      f.write(buf, sizeof buf);
      cursor += sizeof buf;
    }
    filehdr.szFiles = cursor - filehdr.pFiles;

    // Seek back and write the section header
    f.seekp(filehdr.pFiles, std::ios_base::beg);
    char buf[FMT_METADB_SZ_FilesSHdr];
    fmt_metadb_filesSHdr_write(buf, &shdr);
    f.write(buf, sizeof buf);
  }
  // NOTE: cursor and file cursor out of sync here

  { // Functions section
    filehdr.pFunctions = cursor = align(cursor, 8);

    fmt_metadb_functionsSHdr_t shdr = {
      .pFunctions = align(cursor + FMT_METADB_SZ_FunctionsSHdr, 8),
      .nFunctions = 0,
    };

    f.seekp(cursor = shdr.pFunctions, std::ios_base::beg);
    for(auto& [rff, udf]: udFuncs.iterate()) {
      const Function& ff = rff;
      shdr.nFunctions++;
      udf.ptr = cursor;

      auto sl = ff.sourceLocation();
      fmt_metadb_functionSpec_t fspec = {
        .pName = filehdr.pStrings + udf.pName_base,
        .pModule = ff.module().userdata[ud].ptr,
        .offset = ff.offset().value_or(0),
        .pFile = sl ? sl->first.userdata[ud].ptr : 0,
        .line = sl ? static_cast<uint32_t>(sl->second) : 0,
      };
      char buf[FMT_METADB_SZ_FunctionSpec];
      fmt_metadb_functionSpec_write(buf, &fspec);
      f.write(buf, sizeof buf);
      cursor += sizeof buf;
    }
    for(auto& [dat, udf]: udPlaceholders.iterate()) {
      shdr.nFunctions++;
      udf.ptr = cursor;

      fmt_metadb_functionSpec_t fspec = {
        .pName = filehdr.pStrings + udf.pName_base,
        .pModule = 0, .offset = 0,
        .pFile = 0, .line = 0,
      };
      char buf[FMT_METADB_SZ_FunctionSpec];
      fmt_metadb_functionSpec_write(buf, &fspec);
      f.write(buf, sizeof buf);
      cursor += sizeof buf;
    }
    filehdr.szFunctions = cursor - filehdr.pFunctions;

    // Seek back and write the section header
    f.seekp(filehdr.pFunctions, std::ios_base::beg);
    char buf[FMT_METADB_SZ_FunctionsSHdr];
    fmt_metadb_functionsSHdr_write(buf, &shdr);
    f.write(buf, sizeof buf);
  }
  // NOTE: cursor and file cursor out of sync here

  { // Context Tree section
    f.seekp(filehdr.pContext = cursor = align(cursor, 8), std::ios_base::beg);

    const auto compose = [this](const Context& c, std::vector<char>& out) {
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
      case Scope::Type::loop:
        ctx.lexicalType = FMT_METADB_LEXTYPE_Loop;
        setSrcLine();
        break;
      case Scope::Type::point:
        ctx.lexicalType = FMT_METADB_LEXTYPE_Instruction;
        setPoint();
        break;
      }

      auto oldsz = out.size();
      out.resize(out.size() + FMT_METADB_MAXSZ_Context);
      auto used = fmt_metadb_context_write(&out[oldsz], &ctx);
      out.resize(oldsz + used);
    };

    // Lay out the section and write out the section header
    fmt_metadb_contextsSHdr_t shdr;
    shdr.pEntryPoints = cursor = align(cursor + FMT_METADB_SZ_ContextsSHdr, 8);
    shdr.nEntryPoints = src.contexts().children().size();

    char buf[FMT_METADB_SZ_ContextsSHdr];
    fmt_metadb_contextsSHdr_write(buf, &shdr);
    f.write(buf, sizeof buf);

    f.seekp(cursor = align(cursor + shdr.nEntryPoints * FMT_METADB_SZ_EntryPoint, 8), std::ios_base::beg);

    uint16_t i = 0;
    for(const Context& top: src.contexts().children().citerate()) {
      // Output Contexts in reversed DFS order, since that's easier to implement
      // Since Contexts are 8-aligned and 8-sized, we don't need to seek in here
      auto& udc = top.userdata[ud];
      top.citerate(nullptr, [&](const Context& c){
        if(c.children().empty()) return;
        if(elide(c)) return;

        auto& udc = c.userdata[ud];
        std::vector<char> buf;
        for(const Context& cc: c.children().citerate()) {
          if(elide(cc)) {
            for(const Context& gcc: cc.children().citerate()) {
              assert(!elide(gcc) && "Recursion needed for this algorithm!");
              compose(gcc, buf);
            }
          } else {
            compose(cc, buf);
          }
        }
        udc.pChildren = cursor;
        f.write(buf.data(), buf.size());
        cursor += udc.szChildren = buf.size();
      });

      // Seek back and interpret the top-level context as an entry point
      char buf[FMT_METADB_SZ_EntryPoint];
      fmt_metadb_entryPoint_t ep = {
        .szChildren = udc.szChildren, .pChildren = udc.pChildren,
        .ctxId = top.userdata[src.identifier()],
        .entryPoint = udc.entryPoint,
        .pPrettyName = filehdr.pStrings + udc.pPrettyName_base,
      };
      fmt_metadb_entryPoint_write(buf, &ep);
      f.seekp(shdr.pEntryPoints + i * FMT_METADB_SZ_EntryPoint, std::ios_base::beg);
      f.write(buf, sizeof buf);
      f.seekp(cursor, std::ios_base::beg);

      i++;
    }

    filehdr.szContext = cursor - filehdr.pContext;
  }

  { // File footer
    f.write(fmt_metadb_footer, sizeof fmt_metadb_footer);
  }

  { // File header
    char buf[FMT_METADB_SZ_FHdr];
    fmt_metadb_fHdr_write(buf, &filehdr);
    f.seekp(0, std::ios_base::beg);
    f.write(buf, sizeof buf);
  }
} catch(const std::exception& e) {
  util::log::fatal{} << "Error while writing meta.db: " << e.what();
}

MetaDB::MetricRef::MetricRef(const Metric& m) : m(m) {};

template<class Lookup>
void MetaDB::MetricRef::compose(char buf[FMT_METADB_SZ_MetricDesc],
                                uint64_t pSubarrays, const Lookup& str) const {
  fmt_metadb_metricDesc_t mdesc = {
    .pName = str(m.name()),
    .pScopeInsts = pSubarrays,
    .pSummaries = pSubarrays + m.scopes().count() * FMT_METADB_SZ_PropScopeInst,
    .nScopeInsts = static_cast<uint16_t>(m.scopes().count()),
    .nSummaries = static_cast<uint16_t>(m.scopes().count()*m.partials().size()),
  };
  fmt_metadb_metricDesc_write(buf, &mdesc);
}

size_t MetaDB::MetricRef::scopesSize() const {
  // {MD}, {PS} and {SS} are all 8-aligned and 8-sized, so we can just add
  // without thinking about the alignments
  return m.scopes().count() * (FMT_METADB_SZ_PropScopeInst + m.partials().size() * FMT_METADB_SZ_SummaryStat);
}

template<class Lookup>
std::vector<char> MetaDB::MetricRef::composeSubarrays(uint64_t start,
    uint64_t pScopes, const Metric::Identifier& id, const Lookup& str) const {
  std::vector<char> buf;
  buf.resize(scopesSize(), 0);
  size_t scopeCursor = 0;
  size_t summaryCursor = m.scopes().count() * FMT_METADB_SZ_PropScopeInst;
  for(MetricScope ms: m.scopes()) {
    auto pScope = pScopes + static_cast<size_t>(ms) * FMT_METADB_SZ_PropScope;

    fmt_metadb_propScopeInst_t scope = {
      .pScope = pScope,
      .propMetricId = static_cast<uint16_t>(id.getFor(ms)),
    };
    fmt_metadb_propScopeInst_write(&buf[scopeCursor], &scope);
    scopeCursor += FMT_METADB_SZ_PropScopeInst;

    for(const auto& p: m.partials()) {
      fmt_metadb_summaryStat_t summary;
      summary.pScope = pScope;
      summary.pFormula = str(accumulateFormulaString(p.accumulate()));
      summary.statMetricId = static_cast<uint16_t>(id.getFor(p, ms));
      switch(p.combinator()) {
      case Statistic::combination_t::sum:
        summary.combine = FMT_METADB_COMBINE_Sum;
        break;
      case Statistic::combination_t::min:
        summary.combine = FMT_METADB_COMBINE_Min;
        break;
      case Statistic::combination_t::max:
        summary.combine = FMT_METADB_COMBINE_Max;
        break;
      }

      fmt_metadb_summaryStat_write(&buf[summaryCursor], &summary);
      summaryCursor += FMT_METADB_SZ_SummaryStat;
    }
  }
  return buf;
}
