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

#include "struct.hpp"

#include "../util/log.hpp"

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

static std::once_flag xmlinit;
static std::atomic<unsigned int> xmlusers;

namespace hpctoolkit::finalizers::detail {
class StructFileParser {
public:
  StructFileParser(const stdshim::filesystem::path&) noexcept;
  ~StructFileParser() = default;

  StructFileParser(StructFileParser&&) = default;
  StructFileParser& operator=(StructFileParser&&) = default;
  StructFileParser(const StructFileParser&) = delete;
  StructFileParser& operator=(const StructFileParser&) = delete;

  bool valid() const noexcept { return ok; }

  std::string seekToNextLM() noexcept;
  bool parse(ProfilePipeline::Source&, const Module&, StructFile::udModule&) noexcept;

private:
  std::unique_ptr<SAX2XMLReader> parser;
  XMLPScanToken token;
  bool ok;
};
}

using StructFileParser = hpctoolkit::finalizers::detail::StructFileParser;

StructFile::StructFile(stdshim::filesystem::path p) : path(std::move(p)) {
  util::call_once(xmlinit, [](){ XMLPlatformUtils::Initialize(); });
  xmlusers.fetch_add(1, std::memory_order_relaxed);

  while(1) {  // Exit on EOF or error
    auto parser = std::make_unique<StructFileParser>(path);
    if(!parser->valid()) {
      util::log::error{} << "Error while parsing Structfile " << path.filename().string();
      return;
    }

    std::string lm;
    do {
      lm = parser->seekToNextLM();
      if(lm.empty()) {
        // EOF or error
        if(!parser->valid())
          util::log::error{} << "Error while parsing Structfile " << path.filename().string();
        return;
      }
    } while(lms.find(lm) != lms.end());

    lms.emplace(std::move(lm), std::move(parser));
  }
}

StructFile::~StructFile() {
  if(xmlusers.fetch_sub(1, std::memory_order_relaxed) == 1)
    XMLPlatformUtils::Terminate();
}

void StructFile::notifyPipeline() noexcept {
  ud = sink.structs().module.add_default<udModule>(
    [this](udModule& data, const Module& m){
      load(m, data);
    });
}

util::optional_ref<Context> StructFile::classify(Context& c, Scope& s) noexcept {
  if(s.type() == Scope::Type::point) {
    auto mo = s.point_data();
    const auto& udm = mo.first.userdata[ud];
    auto leafit = udm.leaves.find(mo.second);
    if(leafit != udm.leaves.end()) {
      std::reference_wrapper<Context> cc = c;
      const std::function<void(const udModule::trienode&)> handle =
        [&](const udModule::trienode& tn){
          if(tn.second != nullptr)
            handle(*(const udModule::trienode*)tn.second);
          cc = sink.context(cc, tn.first);
        };
      handle(leafit->second.first);
      return cc.get();
    }
  }
  return std::nullopt;
}

bool StructFile::resolve(ContextFlowGraph& fg) noexcept {
  if(fg.scope().type() == Scope::Type::point) {
    auto mo = fg.scope().point_data();
    const auto& udm = mo.first.userdata[ud];

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
    std::unordered_set<uint64_t> seen;
    std::vector<Scope> rpath;
    const std::function<void(uint64_t)> dfs = [&](uint64_t calleeFunc){
      // TODO: SCC algorithms are needed to handle recursion in a meaningful
      // way. For now just truncate the search.
      auto [seenit, first] = seen.insert(calleeFunc);
      if(!first) return;

      // Try to step "forwards" to the caller instructions. If we succeed, this
      // is part of the path.
      bool terminal = true;
      for(auto [callerit, end] = udm.rcg.equal_range(calleeFunc);
          callerit != end; ++callerit) {
        terminal = false;
        rpath.push_back(Scope(mo.first, callerit->second));

        // Find the function for the caller instruction, and continue the DFS
        // from there.
        // TODO: Gracefully handle the error if the Structfile has a bad call graph.
        dfs(udm.leaves.at({callerit->second, callerit->second}).second);
        rpath.pop_back();
      }

      if(terminal) {
        // This function is a kernel entry point. The path to get here is the
        // reverse of the path we constructed along the way.
        auto fpath = rpath;
        std::reverse(fpath.begin(), fpath.end());
        // Record the full Template representing this route.
        fg.add({Scope(mo.first, calleeFunc), std::move(fpath)});
      }

      seen.erase(seenit);
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
  auto it = lms.find(m.path());
  if(it == lms.end()) it = lms.find(m.userdata[sink.resolvedPath()]);
  if(it == lms.end()) return;  // We got nothing

  // TODO: Check if this is the only StructFile for this Module.

  if(!it->second->parse(sink, m, ud))
    util::log::error{} << "Error while parsing Structfile " << path.string();

  lms.erase(it);
}

StructFileParser::StructFileParser(const stdshim::filesystem::path& path) noexcept
  : parser(XMLReaderFactory::createXMLReader()), ok(false) {
  try {
    if(!parser) return;
    if(!parser->parseFirst(XMLStr(path.string()), token)) {
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

std::string StructFileParser::seekToNextLM() noexcept try {
  assert(ok);
  ok = false;
  std::string lm;
  bool eof = false;
  LHandler handler([&](const std::string& ename, const Attributes& attr){
    if(ename == "LM") lm = xmlstr(attr.getValue(XMLStr("n")));
  }, [&](const std::string& ename){
    if(ename == "HPCToolkitStructure") eof = true;
  });
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler);

  while(lm.empty()) {
    if(!parser->parseNext(token)) {
      if(!eof) {
        util::log::info{} << "Error while parsing for Structfile LM";
      } else ok = true;
      parser->setContentHandler(nullptr);
      parser->setErrorHandler(nullptr);
      return std::string();
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
  return std::string();
} catch(xercesc::SAXException& e) {
  util::log::info{} << "Exception caught while parsing Structfile for LM\n"
       "  msg: " << xmlstr(e.getMessage()) << "\n";
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  return std::string();
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
                             StructFile::udModule& ud) noexcept try {
  using trienode = std::pair<Scope, const void*>;
  assert(ok);
  struct Ctx {
    char tag;
    util::optional_ref<const File> file;
    std::optional<uint64_t> funcEntry;
    const trienode* node;
    uint64_t a_line;
    Ctx() : tag('R'), node(nullptr), a_line(0) {};
    Ctx(const Ctx& o, char t) : Ctx(o) { tag = t; }
  };
  std::stack<Ctx, std::vector<Ctx>> stack;
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
      if(top.funcEntry) throw std::logic_error("<P> tags cannot be nested!");
      auto is = parseVs(xmlstr(attr.getValue(XMLStr("v"))));
      if(is.size() != 1) throw std::invalid_argument("VMA on <P> should only have one range!");
      if(is[0].end != is[0].begin+1) throw std::invalid_argument("VMA on <P> should represent a single byte!");
      auto name = xmlstr(attr.getValue(XMLStr("n")));
      auto& func = top.file
          ? ud.funcs.emplace_back(m, is[0].begin, std::move(name), *top.file,
                std::stoll(xmlstr(attr.getValue(XMLStr("l")))) )
          : ud.funcs.emplace_back(m, is[0].begin, std::move(name));
      auto& next = stack.emplace(top, 'P');
      next.node = &ud.trie.emplace_back(Scope(func), top.node);
      next.funcEntry = is[0].begin;
    } else if(ename == "L") {  // Loop (Scope::Type::loop)
      auto fpath = xmlstr(attr.getValue(XMLStr("f")));
      const File& file = fpath.empty() ? *top.file : sink.file(std::move(fpath));
      auto line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      auto& next = stack.emplace(top, 'L');
      next.node = &ud.trie.emplace_back(Scope(Scope::loop, file, line), top.node);
      next.file = file;
    } else if(ename == "S" || ename == "C") {  // Statement (Scope::Type::line)
      if(!top.file) throw std::logic_error("<S> tag without an implicit f= attribute!");
      if(!top.funcEntry) throw std::logic_error("<S> tag without an enclosing <P>!");
      auto line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      const trienode& leaf = ud.trie.emplace_back(Scope(*top.file, line), top.node);
      auto is = parseVs(xmlstr(attr.getValue(XMLStr("v"))));
      for(const auto& i: is) {
        // FIXME: Code regions may be shared by multiple functions,
        // unfortunately Struct doesn't currently sort this out for us. So if
        // there is an overlap we just ignore this tag's contribution.
        ud.leaves.try_emplace(i, leaf, *top.funcEntry);
      }
      if(ename == "C") {  // Call: <S> with an additional call edge
        if(is.size() != 1) throw std::invalid_argument("VMA on <C> tag should only have one range!");
        auto caller = is[0].begin;
        // FIXME: Sometimes the t= attribute is not there. No idea why, maybe
        // indirect call sites? Since the call data is basically non-existent,
        // we just ignore it and continue on.
        auto callee = xmlstr(attr.getValue(XMLStr("t")));
        if(!callee.empty())
          ud.rcg.emplace(std::stoll(callee, nullptr, 16), caller);
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
        auto& func = ud.funcs.emplace_back(m, std::nullopt,
            xmlstr(attr.getValue(XMLStr("n"))),
            fpath.empty() ? *top.file : sink.file(std::move(fpath)),
            std::stoll(xmlstr(attr.getValue(XMLStr("l")))));
        auto& next = stack.emplace(top, 'B');
        next.node = &ud.trie.emplace_back(Scope(func, *top.file, top.a_line), top.node);
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
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler);
  stack.emplace();
  bool fine;
  while((fine = parser->parseNext(token)) && !done);
  stack.pop();
  if(!fine) {
    util::log::info{} << "Error while parsing Structfile\n";
    return false;
  }

  assert(stack.size() == 0 && "Inconsistent stack handling!");

  // Cleanup and move on.
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  return true;
} catch(std::exception& e) {
  util::log::info{} << "Exception caught while parsing Structfile\n"
       "  what(): " << e.what() << "\n"
       "  for binary: " << m.path().string();
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  return false;
} catch(xercesc::SAXException& e) {
  util::log::info{} << "Exception caught while parsing Structfile\n"
       "  msg: " << xmlstr(e.getMessage()) << "\n"
       "  for binary: " << m.path().string();
  parser->setContentHandler(nullptr);
  parser->setErrorHandler(nullptr);
  return false;
}
