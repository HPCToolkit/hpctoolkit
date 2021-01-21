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

StructFile::StructFile(const stdshim::filesystem::path& p) : path(p) {
  util::call_once(xmlinit, [](){ XMLPlatformUtils::Initialize(); });
  xmlusers.fetch_add(1, std::memory_order_relaxed);

  std::string lm;
  LHandler handler([&](const std::string& ename, const Attributes& attr){
    if(ename == "LM") lm = xmlstr(attr.getValue(XMLStr("n")));
  });

  std::unique_ptr<SAX2XMLReader> parser(XMLReaderFactory::createXMLReader());
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler);

  XMLPScanToken token;
  if(!parser->parseFirst(XMLStr(p.string()), token)) {
    util::log::info{} << "Error while parsing Structfile XML prologue";
    util::log::error{} << "Error while parsing Structfile " << p.filename().string();
    return;
  }

  while(lm.empty()) {
    if(!parser->parseNext(token)) {
      util::log::info{} << "Error while parsing Structfile XML prologue element";
      util::log::error{} << "Error while parsing Structfile " << p.filename().string();
      return;
    }
  }

  modpath = std::move(lm);
}

StructFile::~StructFile() {
  if(xmlusers.fetch_sub(1, std::memory_order_relaxed) == 1)
    XMLPlatformUtils::Terminate();
}

static std::vector<Classification::Interval> parseVs(const std::string& vs) {
  // General format: {[0xstart-0xend) ...}
  if(vs.at(0) != '{' || vs.size() < 2)
    throw std::invalid_argument("Bad VMA description: bad start");
  std::vector<Classification::Interval> vals;
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

    vals.emplace_back(lo, hi-1);  // Because its a half-open interval
  }
  return vals;
}

void StructFile::module(const Module& m, Classification& c) noexcept {
  if(modpath.empty()) return;  // This one failed
  bool matches = false;
  if(m.path() == modpath) matches = true;
  if(m.userdata[sink.resolvedPath()] == modpath) matches = true;
  if(!matches) return;

  if(!c.empty()) {
    util::log::warning{}
    << "Multiple Structure files (may) apply for " << m.path().filename() << "!\n"
       " Original path: " << m.path() << "\n"
       "Alternate path: " << m.userdata[sink.resolvedPath()];
  }

  if(!parse(m, c))
    util::log::error{} << "Error while parsing Structfile " << path.string();
}

bool StructFile::parse(const Module& m, Classification& c) try {
  struct Ctx {
    char tag;
    const File* file;
    Classification::Block* scope;
    uint64_t a_line;
    Ctx() : tag('R'), file(nullptr), scope(nullptr), a_line(0) {};
    Ctx(const Ctx& o, char t) : tag(t), file(o.file), scope(o.scope), a_line(o.a_line) {};
  };
  struct Stack : public std::stack<Ctx, std::vector<Ctx>> {
    Classification::Block* rootScope() const {
      for(const auto& ctx: c)
        if(ctx.scope != nullptr) return ctx.scope;
      assert(false && "No root Scope!");
      std::abort();
    }
  } stack;
  bool seenhts = false;
  bool seenlm = false;
  std::vector<Classification::LineScope> lscopes;
  std::unordered_map<uint64_t, Classification::Block*> funcs;
  std::forward_list<std::tuple<Classification::Block*, uint64_t, uint64_t>> cfg;
  LHandler handler([&](const std::string& ename, const Attributes& attr) {
    if(ename == "HPCToolkitStructure") {
      stack.emplace();
      if(seenhts) throw std::logic_error("More than one HPCToolkitStructure tag seen");
      seenhts = true;
    } else if(ename == "LM") {
      if(seenlm) throw std::logic_error("More than one LM tag seen");
      seenlm = true;
    } else if(ename == "F") {
      stack.emplace(stack.top(), 'F');
      stack.top().file = &sink.file(xmlstr(attr.getValue(XMLStr("n"))));
    } else if(ename == "P") {
      auto& f = c.addFunction(m);
      f.name = xmlstr(attr.getValue(XMLStr("n")));
      f.offset = parseVs(xmlstr(attr.getValue(XMLStr("v")))).at(0).lo;
      f.file = stack.top().file;
      f.line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      stack.emplace(stack.top(), 'P');
      stack.top().scope = c.addScope(f, stack.top().scope);
      funcs.emplace(f.offset, stack.top().scope);
    } else if(ename == "L") {
      const auto& f = sink.file(xmlstr(attr.getValue(XMLStr("f"))));
      auto l = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      stack.emplace(stack.top(), 'L');
      stack.top().scope = c.addScope(Scope::loop, f, l, stack.top().scope);
      stack.top().file = &f;
    } else if(ename == "S") {
      auto l = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      for(const auto& i: parseVs(xmlstr(attr.getValue(XMLStr("v"))))) {
        lscopes.emplace_back(i.lo, stack.top().file, l);
        c.setScope(i, stack.top().scope);
      }
    } else if(ename == "A") {
      if(stack.top().tag != 'A') {  // Single A, just changes the file
        stack.emplace(stack.top(), 'A');
        stack.top().file = &sink.file(xmlstr(attr.getValue(XMLStr("n"))));
        stack.top().a_line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      } else {  // Double A, inlined function
        auto& f = c.addFunction(m);
        f.name = xmlstr(attr.getValue(XMLStr("n")));
        f.offset = 0;  // Struct doesn't match it up with the original
        f.file = &sink.file(xmlstr(attr.getValue(XMLStr("f"))));
        f.line = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
        stack.emplace(stack.top(), 'B');
        stack.top().scope = c.addScope(f, *stack.top().file, stack.top().a_line, stack.top().scope);
      }
    } else if(ename == "C") {
      auto l = std::stoll(xmlstr(attr.getValue(XMLStr("l"))));
      auto is = parseVs(xmlstr(attr.getValue(XMLStr("v"))));
      if(is.size() != 1) throw std::logic_error("C tags should only have a single v range");
      auto i = is[0];
      lscopes.emplace_back(i.lo, stack.top().file, l);
      c.setScope(i, stack.top().scope);
      auto d = xmlstr(attr.getValue(XMLStr("d")));
      if(!d.empty()) {
        auto tstr = xmlstr(attr.getValue(XMLStr("t")));
        auto t = std::strtoll(&tstr.c_str()[2], nullptr, 16);
        cfg.emplace_front(stack.rootScope(), i.lo, t);
      }
    } else throw std::logic_error("Unknown tag " + ename);
  }, [&](const std::string& ename){
    if(ename == "LM") return;
    if(ename == "S") return;
    if(ename == "C") return;
    stack.pop();
  });
  std::unique_ptr<SAX2XMLReader> parser(XMLReaderFactory::createXMLReader());
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler);
  parser->parse(XMLStr(path.string()));
  assert(stack.size() == 0 && "Inconsistent stack handling!");
  c.setLines(std::move(lscopes));

  // Build the reverse call graph/CFG from the data we've gathered thus far
  std::unordered_multimap<Classification::Block*,
                          std::pair<Classification::Block*, uint64_t>> rcfg;
  while(!cfg.empty()) {
    const auto [in, from, to] = cfg.front();
    auto it = funcs.find(to);
    if(it != funcs.end())
      rcfg.insert({it->second, {in, from}});
    cfg.pop_front();
  }

  // Map out the possible routes from each top-level block. We assume all routes
  // present in the static CFG are possible.
  // TODO: Apply some SCC handling around here, and some general optimizations.
  auto* unknown_block = c.addScope(Scope{});
  for(const auto [offset, root]: funcs) {
    std::vector<Classification::Block::route_t> route;
    std::unordered_set<Classification::Block*> seen;
    std::function<void(Classification::Block*)> dfs = [&](Classification::Block* b) {
      if(!seen.insert(b).second) {
        // Terminate the route, we've looped on ourselves.
        route.back() = unknown_block;
        auto froute = route;
        std::reverse(froute.begin(), froute.end());
        root->addRoute(std::move(froute));
        return;
      }
      auto range = rcfg.equal_range(b);
      if(range.first == range.second) {
        // Terminate the route, we have nowhere else to go
        if(route.empty()) return;
        auto froute = route;
        std::reverse(froute.begin(), froute.end());
        root->addRoute(std::move(froute));
      } else {
        for(auto it = range.first; it != range.second; ++it) {
          const auto [from, addr] = it->second;
          route.push_back(addr);
          dfs(from);
          route.pop_back();
        }
      }
      seen.erase(b);
    };
    dfs(root);
  }
  return true;
} catch(std::exception& e) {
  util::log::info{} << "Exception caught while parsing Structfile "
    << path.string() << "\n"
       "  what(): " << e.what() << "\n"
       "  for binary: " << modpath.string();
  return false;
} catch(xercesc::SAXException& e) {
  util::log::info{} << "Exception caught while parsing Structfile "
    << path.string() << "\n"
       "  msg: " << xmlstr(e.getMessage()) << "\n"
       "  for binary: " << modpath.string();
  return false;
}
