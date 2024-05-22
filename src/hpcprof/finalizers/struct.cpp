// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "struct.hpp"

#include "../util/log.hpp"

#include <limits>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/util/XMLString.hpp>

#include <atomic>
#include <mutex>
#include <functional>
#include <stack>

using namespace hpctoolkit;
using namespace finalizers;
using namespace xercesc;

// Xerces requires the global XMLPlatformUtils to be called before and after all
// usage. Since that's a big pain, we just tie it into the static constructors.
namespace {
struct XercesState {
  XercesState() {
    XMLPlatformUtils::Initialize();
  }
  ~XercesState() {
    XMLPlatformUtils::Terminate();
  }
};
}

static XercesState xercesState;

static std::string xmlstr(const XMLCh* const str) {
  char* n = XMLString::transcode(str);
  if(n == nullptr) return "";
  std::string r(n);
  XMLString::release(&n);
  return r;
}

struct XMLStr {
  XMLStr(const std::string& s) : str(XMLString::transcode(s.c_str())) {};
  ~XMLStr() { XMLString::release(&str); }
  operator const XMLCh*() const { return str; }
  XMLCh* str;
};

struct LHandler : public DefaultHandler {
  std::function<void(const std::string&, const Attributes&)> start;
  std::function<void(const std::string&)> end;

  void startElement(const XMLCh* const uri, const XMLCh* const localname,
                    const XMLCh* const qname, const Attributes& attrs) {
    start(xmlstr(localname), attrs);
  }

  void endElement(const XMLCh* const uri, const XMLCh* const localname,
                  const XMLCh* const qname) {
    if(end) end(xmlstr(localname));
  }

  LHandler(std::function<void(const std::string&, const Attributes&)> s,
           std::function<void(const std::string&)> e)
    : start(s), end(e) {};
  LHandler(std::function<void(const std::string&, const Attributes&)> s)
    : start(s) {};
};

namespace hpctoolkit::finalizers::detail {
struct LMData {
  stdshim::filesystem::path path;
  bool has_calls;
};

class StructFileParser {
public:
  StructFileParser(const stdshim::filesystem::path&) noexcept;
  ~StructFileParser() = default;

  StructFileParser(StructFileParser&&) = default;
  StructFileParser& operator=(StructFileParser&&) = default;
  StructFileParser(const StructFileParser&) = delete;
  StructFileParser& operator=(const StructFileParser&) = delete;

  bool valid() const noexcept { return ok; }

  std::optional<LMData> seekToNextLM(const stdshim::filesystem::path measDirPath) noexcept;
  bool parse(ProfilePipeline::Source&, const Module&, bool, StructFile::udModule&) noexcept;

private:
  std::unique_ptr<SAX2XMLReader> parser;
  XMLPScanToken token;
  bool ok;
};
}

using LMData = hpctoolkit::finalizers::detail::LMData;
using StructFileParser = hpctoolkit::finalizers::detail::StructFileParser;

StructFile::StructFile(stdshim::filesystem::path p, stdshim::filesystem::path meas, std::shared_ptr<RecommendationStore> rs)
  : recstore(std::move(rs)), path(stdshim::filesystem::absolute(std::move(p))), measDirPath(stdshim::filesystem::weakly_canonical(meas)) {
  while(1) {  // Exit on EOF or error
    auto parser = std::make_unique<StructFileParser>(path);
    if(!parser->valid()) {
      util::log::warning{} << "Failed to parse Structfile " << path.filename().native();
      return;
    }

    auto lm = std::make_unique<LMData>();
    do {
      if(auto o_lm = parser->seekToNextLM(measDirPath)) {
        *lm = std::move(*o_lm);
      } else {
        // EOF or error
        if(!parser->valid())
          util::log::warning{} << "Failed to parse Structfile " << path.filename().native();
        return;
      }
    } while(lms.find(lm->path) != lms.end());
    auto path = std::move(lm->path);
    lms.emplace(std::move(path), std::make_pair(std::move(lm), std::move(parser)));
  }
}

StructFile::~StructFile() = default;

void StructFile::notifyPipeline() noexcept {
  ud = sink.structs().module.add_default<udModule>(
    [this](udModule& data, const Module& m){
      load(m, data);
    });
}

std::optional<std::pair<util::optional_ref<Context>, Context&>>
StructFile::classify(Context& c, NestedScope& ns) noexcept {
  if(ns.flat().type() == Scope::Type::point) {
    auto mo = ns.flat().point_data();
    const auto& udm = mo.first.userdata[ud];
    if(udm.leaves.empty()) {
      // We don't have any data for this Module, so pass it on
      return std::nullopt;
    }

    auto leafit = udm.leaves.find(mo.second);
    if(leafit == udm.leaves.end()) {
      // We have data for this module, but we don't have data for this specific
      // point (i.e. a gap in the Structfile). Assume we are better than any
      // other available Finalizer and report no information.
      return std::make_pair(std::nullopt, std::ref(c));
    }

    util::optional_ref<Context> cr;
    std::reference_wrapper<Context> cc = c;
    const std::function<void(const udModule::trienode&)> handle =
      [&](const udModule::trienode& tn){
        if(tn.second != nullptr)
          handle(*(const udModule::trienode*)tn.second);
        cc = sink.context(cc, {ns.relation(), tn.first.first}).second;
        if(!cr) cr = cc;
        ns.relation() = tn.first.second;
      };
    handle(leafit->second.first);
    return std::make_pair(cr, cc);
  }
  return std::nullopt;
}

bool StructFile::resolve(ContextFlowGraph& fg) noexcept {
  if(fg.scope().type() == Scope::Type::point) {
    auto mo = fg.scope().point_data();
    const auto& udm = mo.first.userdata[ud];

    switch(udm.cfgStatus) {
    case CallGraphStatus::VALID:
      // All good, nothing to see here
      break;

    case CallGraphStatus::NONE:
      // Structfile didn't match, just ignore
      return false;

    case CallGraphStatus::NOT_PRESENT:
      // hpcstruct was run without --gpucfg yes. We can't do reconstruction
      // in this state so let the user know they did a bad.
      if(recstore) {
        util::call_once(recstore->once, [&]{
          util::log::warning w;
          w << "Unable to reconstruct calling contexts (or loops) within GPU kernels\n"
                "  To enable this reconstruction, please run:\n"
                "    $ hpcstruct --gpucfg yes" << recstore->hpcstructArgs;
          if(!recstore->measDir) {
            const auto& binary = mo.first.path();
            auto sfile = binary.filename().concat(".hpcstruct");
            w << " " << binary.native() << "\n"
                  "  And pass the output file as -S " << sfile.native();
          }
        });
      }
      return false;

    case CallGraphStatus::ERRORED:
      // hpcstruct was run but the output was corrupt in some way. Warn that this
      // is a problem for context reconstruction.
      util::call_once(recstore->once, [&]{
        util::log::warning()
          << "Control flow data is corrupt, disabling affected GPU context reconstruction";
      });
      return false;
    }

    // First move from the instruction to it's enclosing function's entry. That
    // makes things easier for the DFS later.
    const auto leafit = udm.leaves.find(mo.second);
    if(leafit == udm.leaves.end()) {
      // Sample outside of our knowledge of function bounds. We know nothing.
      // TODO: Emit an error in this case?
      return false;
    }

    // DFS through the call graph to iterate all possible paths from a kernel
    // entry point (uncalled function) to this function.
    std::vector<Scope> rpath;
    const std::function<void(const Function&)> dfs = [&](const Function& callee){
      // Step to any potential callers. If this function can be called, this is part of the path.
      bool terminal = true;
      for(auto [callerit, end] = udm.rcg.equal_range(callee);
          callerit != end; ++callerit) {
        terminal = false;
        rpath.push_back(Scope(mo.first, callerit->second.first));

        // Continue the DFS from the caller Function.
        dfs(callerit->second.second);
        rpath.pop_back();
      }

      if(terminal) {
        // This function is a kernel entry point. The path to get here is the
        // reverse of the path we constructed along the way.
        auto fpath = rpath;
        std::reverse(fpath.begin(), fpath.end());
        // Record the full Template representing this route.
        fg.add({Scope(callee), std::move(fpath)});
      }
    };
    dfs(leafit->second.second);

    // If we made it here, we found at least one path. Set up the handler and
    // report it as the final answer.
    fg.handler([](const Metric& m){
      ContextFlowGraph::MetricHandling ret;
      if(m.name() == "GINS") ret.interior = true;
      else if(m.name() == "GKER:COUNT") ret.exterior = ret.exteriorLogical = true;
      else if(m.name() == "GKER:SAMPLED_COUNT") ret.exterior = true;
      return ret;
    });
    return true;
  }
  return false;
}

std::vector<stdshim::filesystem::path> StructFile::forPaths() const {
  std::vector<stdshim::filesystem::path> out;
  out.reserve(lms.size());
  for(const auto& lm: lms) out.emplace_back(lm.first);
  return out;
}

void StructFile::load(const Module& m, udModule& ud) noexcept {
  std::unique_ptr<LMData> lm;
  std::unique_ptr<StructFileParser> parser;
  {
    std::unique_lock<std::mutex> l(lms_lock);

    // Structfiles are (usually) generated on the system where the measurement
    // took place, so try to match against the path seen during measurement
    // first.
    auto it = lms.find(m.path());
    // It might have been generated on the current system instead. In that case,
    // we want the path to the file on the current filesystem (resolvedPath).
    if(it == lms.end()) it = lms.find(m.userdata[sink.resolvedPath()]);
    // Otherwise, give up. This Structfile doesn't match the Module.
    if(it == lms.end()) return;  // We got nothing

    std::tie(lm, parser) = std::move(it->second);
    lms.erase(it);
  }

  // TODO: Check if this is the only StructFile for this Module.

  ud.cfgStatus = lm->has_calls ? CallGraphStatus::ERRORED : CallGraphStatus::NOT_PRESENT;
  if(!parser->parse(sink, m, lm->has_calls, ud))
    util::log::warning{} << "Error parsing Structfile " << path.filename().native();
}

StructFileParser::StructFileParser(const stdshim::filesystem::path& path) noexcept
  : parser(XMLReaderFactory::createXMLReader()), ok(false) {
  try {
    if(!parser) return;
    if(!parser->parseFirst(XMLStr(path.native()), token)) {
      util::log::info{} << "Error while parsing Structfile XML prologue";
      return;
    }
    ok = true;
  } catch(std::exception& e) {
    util::log::info{} << "Exception caught while parsing Structfile prologue\n"
         "  what(): " << e.what() << "\n";
  } catch(xercesc::SAXException& e) {
    util::log::info{} << "Exception caught while parsing Structfile prologue\n"
         "  msg: " << xmlstr(e.getMessage()) << "\n";
  }
}

std::optional<LMData> StructFileParser::seekToNextLM(const stdshim::filesystem::path measDirPath) noexcept try {
  assert(ok);
  ok = false;
  LMData lm;
  bool eof = false;
  LHandler handler([&](const std::string& ename, const Attributes& attr){
    if(ename == "LM") {
      lm.path = xmlstr(attr.getValue(XMLStr("n")));
      if(!lm.path.has_root_path()){
        if(measDirPath.empty()){
          util::log::warning{} << "No measurement directory path provided for a load module with relative path "
                  << lm.path << ", so we ignore this StructFile\n";
          lm.path = "";
        }else{
          lm.path = measDirPath / lm.path;
        }
      }
      lm.has_calls = xmlstr(attr.getValue(XMLStr("has-calls"))) == "1";
    }
  }, [&](const std::string& ename){
    if(ename == "HPCToolkitStructure") eof = true;
  });
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler);

  while(lm.path.empty()) {
    if(!parser->parseNext(token)) {
      if(!eof) {
        util::log::info{} << "Error while parsing for Structfile LM";
      } else ok = true;
      parser->setContentHandler(nullptr);
      parser->setErrorHandler(nullptr);
      return std::nullopt;
    }
  }
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  ok = true;
  return lm;
} catch(std::exception& e) {
  util::log::info{} << "Exception caught while parsing Structfile for LM\n"
       "  what(): " << e.what() << "\n";
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  return std::nullopt;
} catch(xercesc::SAXException& e) {
  util::log::info{} << "Exception caught while parsing Structfile for LM\n"
       "  msg: " << xmlstr(e.getMessage()) << "\n";
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  return std::nullopt;
}

static std::vector<util::interval<uint64_t>> parseVs(const std::string& vs) {
  // General format: {[0xstart-0xend) ...}
  if(vs.at(0) != '{' || vs.size() < 2)
    throw std::invalid_argument("Bad VMA description: bad start");
  std::vector<util::interval<uint64_t>> vals;
  const char* c = vs.data() + 1;
  while(*c != '}') {
    char* cx;
    if(std::isspace(*c)) { c++; continue; }
    if(*c != '[') throw std::invalid_argument("Bad VMA description: bad segment opening");
    c++;
    auto lo = std::strtoll(c, &cx, 16);
    c = cx;
    if(*c != '-') throw std::invalid_argument("Bad VMA description: bad segment middle");
    c++;
    auto hi = std::strtoll(c, &cx, 16);
    c = cx;
    if(*c != ')') throw std::invalid_argument("Bad VMA description: bad segment closing");
    c++;

    vals.emplace_back(lo, hi);
  }
  return vals;
}

bool StructFileParser::parse(ProfilePipeline::Source& sink, const Module& m,
                             bool has_calls, StructFile::udModule& ud) noexcept try {
  using trienode = std::pair<std::pair<Scope, Relation>, const void*>;
  assert(ok);
  struct Ctx {
    char tag;
    util::optional_ref<const File> file;
    util::optional_ref<const Function> func;
    const trienode* node;
    uint64_t a_line;
    Ctx() : tag('R'), node(nullptr), a_line(0) {};
    Ctx(const Ctx& o, char t) : Ctx(o) { tag = t; }
  };
  std::stack<Ctx, std::deque<Ctx>> stack;

  // Reversed call graph, but with callee function entries instead of Functions
  std::deque<std::pair<uint64_t, std::pair<uint64_t,
      std::reference_wrapper<const Function>>>> tmp_rcg;
  // Mapping of function entries to Functions
  std::unordered_map<uint64_t, const Function&> funcs;

  bool done = false;
  LHandler handler([&](const std::string& ename, const Attributes& attr) {
    const auto& top = stack.top();
    if(ename == "LM") {  // Load Module
      throw std::logic_error("More than one LM tag seen");
    } else if(ename == "F") {  // File
      auto file = xmlstr(attr.getValue(XMLStr("n")));
      if(file.empty()) throw std::logic_error("Bad <F> tag seen");
      auto& next = stack.emplace(top, 'F');
      next.file = sink.file(std::move(file));
    } else if(ename == "P") {  // Procedure (Function)
      if(top.func) throw std::logic_error("<P> tags cannot be nested!");
      auto is = parseVs(xmlstr(attr.getValue(XMLStr("v"))));
      if(is.size() != 1) throw std::invalid_argument("VMA on <P> should only have one range!");
      if(is[0].end != is[0].begin+1) throw std::invalid_argument("VMA on <P> should represent a single byte!");
      auto name = xmlstr(attr.getValue(XMLStr("n")));
      auto& func = top.file
          ? ud.funcs.emplace_back(m, is[0].begin, std::move(name), *top.file,
                std::stoll(xmlstr(attr.getValue(XMLStr("l")))) )
          : ud.funcs.emplace_back(m, is[0].begin, std::move(name));
      if(!funcs.emplace(is[0].begin, func).second)
        throw std::logic_error("<P> tags must have unique function entries!");
      auto& next = stack.emplace(top, 'P');
      ud.trie.push_back({{Scope(func), Relation::enclosure}, top.node});
      next.node = &ud.trie.back();
      next.func = func;
    } else if(ename == "L") {  // Loop (Scope::Type::binary_loop)
      auto fpath = xmlstr(attr.getValue(XMLStr("f")));
      const File& file = fpath.empty() ? *top.file : sink.file(std::move(fpath));
      auto line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      auto addr = parseVs(xmlstr(attr.getValue(XMLStr("v"))))[0].begin;
      auto& next = stack.emplace(top, 'L');
      ud.trie.push_back({{Scope(Scope::loop, m, addr, file, line), Relation::enclosure}, top.node});
      next.node = &ud.trie.back();
      next.file = file;
    } else if(ename == "S" || ename == "C") {  // Statement (Scope::Type::line)
      if(!top.file) throw std::logic_error("<S> tag without an implicit f= attribute!");
      if(!top.func) throw std::logic_error("<S> tag without an enclosing <P>!");
      auto line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      ud.trie.push_back({{Scope(*top.file, line), Relation::enclosure}, top.node});
      const trienode& leaf = ud.trie.back();
      auto is = parseVs(xmlstr(attr.getValue(XMLStr("v"))));
      for(const auto& i: is) {
        // FIXME: Code regions may be shared by multiple functions,
        // unfortunately Struct doesn't currently sort this out for us. So if
        // there is an overlap we just ignore this tag's contribution.
        ud.leaves.try_emplace(i, leaf, *top.func);
      }
      if(ename == "C") {  // Call: <S> with an additional call edge
        if(is.size() != 1) throw std::invalid_argument("VMA on <C> tag should only have one range!");
        auto callerInst = is[0].begin;
        // FIXME: Sometimes the t= attribute is not there. No idea why, maybe
        // indirect call sites? Since the call data is basically non-existent,
        // we just ignore it and continue on.
        auto callee = xmlstr(attr.getValue(XMLStr("t")));
        if(!callee.empty())
          tmp_rcg.push_back({(uint64_t)std::stoll(callee, nullptr, 16),
                             {callerInst, *top.func}});
      }
    } else if(ename == "A") {
      if(top.tag != 'A') {  // First A, gives the caller line.
        auto& next = stack.emplace(top, 'A');
        auto fpath = xmlstr(attr.getValue(XMLStr("f")));
        if(!fpath.empty()) next.file = sink.file(std::move(fpath));
        next.a_line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      } else {  // Double A, inlined function. Gives the called function, like P
        if(!top.file) throw std::logic_error("Double-<A> without an implicit f= attribute!");
        auto fpath = xmlstr(attr.getValue(XMLStr("f")));
        auto& file = fpath.empty() ? *top.file : sink.file(std::move(fpath));
        auto& func = ud.funcs.emplace_back(m, std::nullopt,
            xmlstr(attr.getValue(XMLStr("n"))), file,
            std::stoll(xmlstr(attr.getValue(XMLStr("l")))));
        auto& next = stack.emplace(top, 'B');
        next.file = file;
        ud.trie.push_back({{Scope(*top.file, top.a_line), Relation::inlined_call}, top.node});
        ud.trie.push_back({{Scope(func), Relation::enclosure}, &ud.trie.back()});
        next.node = &ud.trie.back();
      }
    } else throw std::logic_error("Unknown tag " + ename);
  }, [&](const std::string& ename){
    if(ename == "LM") {
      done = true;
      return;
    }
    if(ename == "S") return;
    if(ename == "C") return;
    stack.pop();
  });

  // We can't repeat the parsing process, so nab ownership in this function
  assert(parser);
  auto my_parser = std::move(parser);
  my_parser->setContentHandler(&handler);
  my_parser->setErrorHandler(&handler);
  stack.emplace();
  bool fine;
  while((fine = my_parser->parseNext(token)) && !done);
  stack.pop();
  if(!fine) {
    util::log::info{} << "Error while parsing Structfile\n";
    return false;
  }
  assert(stack.size() == 0 && "Inconsistent stack handling!");

  // If we have a call graph, do some processing on it to generate a final Reverse Call Graph (RCG).
  if(has_calls || !tmp_rcg.empty()) {
    ud.cfgStatus = StructFile::CallGraphStatus::VALID;

    // First, go through and associate all the call edges with their callees.
    bool ok = true;
    ud.rcg.reserve(tmp_rcg.size());
    for(const auto& [callee, caller]: tmp_rcg) {
      auto target_it = funcs.find(callee);
      if(target_it != funcs.end()) {
        ud.rcg.emplace(target_it->second, std::move(caller));
      } else {
        // One of the <C> tags is obviously not correct. This usually indicates an error in
        // hpcstruct's mapping of calls to procedures, so
        ud.cfgStatus = StructFile::CallGraphStatus::ERRORED;
        ud.rcg.clear();
        ok = false;
        util::log::info{} << "Missing callee at 0x" << std::hex << callee
                          << " when processing Structfile for binary\n  "
                          << m.path().string();
        break;
      }
    }

    if(ok) {
      // At this point, the RCG may contain cycles from recursive functions. This makes things
      // harder later when we try to associate call paths, so we adjust the edges until we achieve
      // a proper DAG.

      // First, using Tarjan's algorithm to group all the Functions in the RCG together into
      // strongly connected components.
      std::unordered_multimap<util::reference_index<const Function>, std::reference_wrapper<const Function>> sccs;
      std::unordered_map<util::reference_index<const Function>, std::reference_wrapper<const Function>> scc_roots;
      {
        struct FuncState {
          std::size_t encounterIndex;
          std::size_t rootEncounterIndex;
          bool isActive;
        };
        std::unordered_map<util::reference_index<const Function>, FuncState> states;
        states.reserve(funcs.size());
        std::stack<std::reference_wrapper<const Function>> active;
        std::size_t numEncountered = 0;
        const std::function<std::pair<const FuncState&, bool>(const Function&)> scc =
          [&](const Function& func) -> std::pair<const FuncState&, bool> {
            auto [stateit, first] = states.try_emplace(func);
            FuncState& state = stateit->second;
            if(first) {
              state.encounterIndex = state.rootEncounterIndex = numEncountered++;
              active.push(std::cref(func));
              state.isActive = true;
              for(auto [it, end] = ud.rcg.equal_range(func); it != end; ++it) {
                const Function& caller = it->second.second;
                auto [callerState, callerFirstVisit] = scc(caller);
                if(callerFirstVisit) {
                  state.rootEncounterIndex = std::min(state.rootEncounterIndex, callerState.rootEncounterIndex);
                } else if(callerState.isActive) {
                  state.rootEncounterIndex = std::min(state.rootEncounterIndex, callerState.encounterIndex);
                }
              }
              if(state.encounterIndex == state.rootEncounterIndex) {
                const Function* sccfunc;
                do {
                  sccfunc = &active.top().get();
                  sccs.emplace(std::cref(func), std::cref(*sccfunc));
                  scc_roots.emplace(std::cref(*sccfunc), std::cref(func));
                  states.at(*sccfunc).isActive = false;
                  active.pop();
                } while(sccfunc != &func);
              }
            }
            return {std::cref(state), first};
          };
        for(const Function& func: ud.funcs)
          scc(func);
      }

      // Delete any edges where the callee and caller are in the same SCC. These edges cause cycles
      // which we want to remove. After this, the graph will be a DAG.
      for(auto it = ud.rcg.begin(), end = ud.rcg.end(); it != end;) {
        if(&scc_roots.at(it->first).get() == &scc_roots.at(it->second.second).get()) {
          it = ud.rcg.erase(it);
        } else {
          ++it;
        }
      }

      // The previous step may have broken call paths that required some SCC-internal edges. Recover
      // the reachability properties of the original graph (between Functions not in the same SCC)
      // by "summarizing" the intra-SCC edges with inter-SCC edges.
      // In short, we pretend every call "could" call all other Functions in the callee's SCC.
      decltype(ud.rcg) extra_rcg;
      for(const auto& [callee_w, call]: ud.rcg) {
        const Function& callee = callee_w;
        for(auto [it, end] = sccs.equal_range(callee); it != end; ++it) {
          const Function& sib_callee = it->second;
          if(&callee == &sib_callee) continue;
          extra_rcg.emplace(std::cref(sib_callee), call);
        }
      }
      ud.rcg.merge(std::move(extra_rcg));
    }
  }

  return true;
} catch(std::exception& e) {
  util::log::info{} << "Exception caught while parsing Structfile\n"
       "  what(): " << e.what() << "\n"
       "  for binary: " << m.path().string();
  return false;
} catch(xercesc::SAXException& e) {
  util::log::info{} << "Exception caught while parsing Structfile\n"
       "  msg: " << xmlstr(e.getMessage()) << "\n"
       "  for binary: " << m.path().string();
  return false;
}
