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

  std::string lm;
  LHandler handler([&](const std::string& ename, const Attributes& attr){
    if(ename == "LM") lm = xmlstr(attr.getValue(XMLStr("n")));
  });

  std::unique_ptr<SAX2XMLReader> parser(XMLReaderFactory::createXMLReader());
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler);

  XMLPScanToken token;
  if(!parser->parseFirst(XMLStr(p.string()), token))
    util::log::fatal() << "Unable to parse prologue!";

  while(lm.empty())
    if(!parser->parseNext(token))
      util::log::fatal() << "Unable to parse something!";

  modpath = std::move(lm);
  xmlusers.fetch_add(1, std::memory_order_relaxed);
}

StructFile::~StructFile() {
  if(xmlusers.fetch_sub(1, std::memory_order_relaxed) == 1)
    XMLPlatformUtils::Terminate();
}

static std::vector<Classification::Interval> parseVs(const std::string& vs) {
  // General format: {[0xstart-0xend) ...}
  if(vs.at(0) != '{' || vs.size() < 2)
    util::log::fatal() << "Bad VMA description in struct file: bad start!";
  std::vector<Classification::Interval> vals;
  const char* c = vs.data() + 1;
  while(*c != '}') {
    char* cx;
    if(std::isspace(*c)) { c++; continue; }
    if(*c != '[') util::log::fatal() << "Bad VMA description in struct file: bad segment opening!";
    c++;
    auto lo = std::strtoll(c, &cx, 16);
    c = cx;
    if(*c != '-') util::log::fatal() << "Bad VMA description in struct file: bad segment middle!";
    c++;
    auto hi = std::strtoll(c, &cx, 16);
    c = cx;
    if(*c != ')') util::log::fatal() << "Bad VMA description in struct file: bad segment closing!";
    c++;

    vals.emplace_back(lo, hi-1);  // Because its a half-open interval
  }
  return vals;
}

void StructFile::module(const Module& m, Classification& c) {
  bool matches = false;
  if(m.path() == modpath) matches = true;
  if(m.userdata[sink.resolvedPath()] == modpath) matches = true;
  if(!matches) return;

  if(!c.empty()) {
    util::log::warning()
    << "Multiple Structure files (may) apply for " << m.path().filename() << "!\n"
       " Original path: " << m.path() << "\n"
       "Alternate path: " << m.userdata[sink.resolvedPath()];
  }

  struct Ctx {
    char tag;
    const File* file;
    std::size_t scope;
    uint64_t a_line;
    Ctx() : tag('R'), file(nullptr), scope(Classification::scopenone), a_line(0) {};
    Ctx(const Ctx& o, char t) : tag(t), file(o.file), scope(o.scope), a_line(o.a_line) {};
  };
  std::stack<Ctx, std::vector<Ctx>> stack;
  bool seenhts = false;
  bool seenlm = false;
  std::vector<Classification::LineScope> lscopes;
  LHandler handler([&](const std::string& ename, const Attributes& attr) {
    if(ename == "HPCToolkitStructure") {
      stack.emplace();
      if(seenhts) util::log::fatal() << "Only one HPCToolkitStructure tag is allowed!";
      seenhts = true;
    } else if(ename == "LM") {
      if(seenlm) util::log::fatal() << "Only one LM tag is allowed!";
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
      // This one we might not care as much about
    } else util::log::fatal() << "Unknown tag in struct file!";
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
  if(stack.size() != 0) util::log::fatal() << "Too many pushes!";
  c.setLines(std::move(lscopes));
}
