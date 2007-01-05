// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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

//************************ System Include Files ******************************

#include <fstream>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using std::strchr; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include "HTMLDriver.hpp"
#include "HTMLTable.hpp"
#include "PerfIndex.hpp"
#include "HTMLFile.hpp"
#include <lib/support/String.hpp>
#include <lib/support/Assertion.h>

//************************ Forward Declarations ******************************

using std::cerr;
using std::cout;
using std::endl;

//****************************************************************************

const char* htmlDoctypeFrames =
  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Frameset//EN\">";
const char* htmlDoctypeNoFrames =
  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">";
const char* htmlCharSet =
  "<meta http-equiv='Content-Type' content='text/html; charset=iso-8859-1'>";
const char* javaScriptAttr =
  "type='text/javascript' language='JavaScript'";

static void 
MakeFill(String &str, String &fill, unsigned int width) 
{
  unsigned int len = str.Length(); 
  if (len >= width) {
     str.LeftFormat(width); 
     fill = ""; 
  } else {
     fill = String((char)' ', (unsigned int)(width - len));
  }
}

static 
String PathName(const char* dir, const char* fname)  
{
  return String(dir) + "/" + fname + ".html"; 
}

HTMLFile::HTMLFile(const char* dir, 
                   const char *filename, 
		   const char* title,
		   bool frameset,
		   int font_sz) 
  : std::ofstream(PathName(dir, filename))
{
  BriefAssertion(filename != NULL); 
  if (!good()) {
    cerr << "ERROR: Could not open file '" 
	 << filename << "' for output" << endl; 
  } 
  
  fName = filename; 
  font_size = font_sz; 
  fgcolor = "";
  bgcolor = ""; 
  onload = ""; 
  onresize = ""; 

  hasHead = true;
  headStopped = false;
  
  usesBodyOrFrameset = true;
  isFrameset = frameset; // this value affects the header that is printed
  bodyOrFramesetStarted = false;
  
  StartHead(title); 
}

HTMLFile::~HTMLFile()
{
  if (usesBodyOrFrameset) {
    StopBodyOrFrameset();
  }
  
  if (hasHead) { 
    (*this) << "</html>" << endl; 
  }
}

void HTMLFile::PrintNoBodyOrFrameset()     {usesBodyOrFrameset = false;}
void HTMLFile::SetFgColor(const char *fg)  {fgcolor = fg;}
void HTMLFile::SetBgColor(const char *bg)  {bgcolor = bg;}
void HTMLFile::SetOnLoad(const char *bg)   {onload = bg;}
void HTMLFile::SetOnResize(const char *bg) {onresize = bg;}

void 
HTMLFile::SetUp()
{
  JSFileInclude(UTILSHERE); 
  const char* dir = strchr(fName, '/'); 
  if (dir) {
    dir = ".."; 
  } 
  JSFileInclude(UTILSALL, dir); 
  (*this) << "<script " << javaScriptAttr << ">"
	  << " handle_load_start('" << fName 
	  << "'); </script>" << endl;
  CSSFileInclude(GLOBALSTYLE, dir);
  CSSFileInclude(STYLEHERE);
} 

void 
HTMLFile::StartHead(const char* title) 
{
  // eraxxon: Give every HTML file a DOCTYPE and character encoding
  BriefAssertion(hasHead);
  if (isFrameset) {
    (*this) << htmlDoctypeFrames << endl;
  } else {
    (*this) << htmlDoctypeNoFrames << endl;
  }
  (*this) << "<html>" << endl 
	  << "<head>" << endl
	  << htmlCharSet << endl;

  // eraxxon: all html documents must have a basic header now and
  // all headers must have a title element.
  if (title != NULL) {
    (*this) << "<title>" << title << "</title>" << endl << endl;
  } else {
    (*this) << "<title>(untitled)</title>" << endl << endl;
  }
}

void 
HTMLFile::StopHead() 
{
  if (!headStopped) {
     (*this) << endl; 
     SetUp(); 
     (*this) << "</head>" << endl << endl;
     headStopped = true; 
  }
}

std::ostream & 
HTMLFile::BodyOrFrameset() 
{
  StopHead();
  if (isFrameset) {
    (*this) << "<frameset ";
  } else {
    (*this) << "<body ";
  }

  // FIXME: eraxxon: colors should really be part of style sheets
  if (fgcolor.Length() > 0) (*this) << "text='" << fgcolor << "' "; 
  if (bgcolor.Length() > 0) (*this) << "bgcolor='" << bgcolor << "' "; 
  if (onload.Length() > 0) {
    (*this) << "onload='" << onload << "' "; 
  } else {
    (*this) << "onload='handle_on_load()' ";
  }

  // eraxxon: 'onresize' is not part of BODY, FRAMESET, FRAME, or any
  //   other html 4.0 element, as far as I can see.
  //if (onresize.Length() > 0) {
  //  (*this) << "onresize='" << onresize << "' "; 
  //}
  
  bodyOrFramesetStarted = true; 
  return (*this); 
}

void 
HTMLFile::StartBodyOrFrameset() 
{
  BriefAssertion(usesBodyOrFrameset);
  if (!bodyOrFramesetStarted) {
    BodyOrFrameset() << ">" << endl << endl;
  }
}

void 
HTMLFile::StopBodyOrFrameset() 
{
  BriefAssertion(usesBodyOrFrameset);
  StartBodyOrFrameset();
  if (isFrameset) {
    (*this)  << endl << "</frameset>" << endl;
  } else {
    (*this)  << endl << "</body>" << endl;
  }
}

void 
HTMLFile::TableStart(const char* name, const char *sortMetricName,
		     const char* headerFile) 
{
  bool isTableBody = (headerFile != NULL); 

  if (isTableBody) {
     String onLoad = String("handle_table_onload('") + fName 
                     + "\",\"" + headerFile + "')"; 
     SetOnLoad(onLoad); 
     (*this) << "<script " << javaScriptAttr << ">"
	     << " var isTable = true; </script>" << endl;
  }
  (*this) << "<script " << javaScriptAttr << ">"
	  << " window.top.sortMetricName = \""
	  << sortMetricName << "\"; </script>" << endl;
  (void) BodyOrFrameset(); 
  (*this)  << ">" << endl; 
} 
  
void 
HTMLFile::TableStop()
{
  (*this)  << "</pre>" << endl; 
}
  
void 
HTMLFile::Anchor(const char* id)
{
  if (id) { 
     (*this) << "<a name='" << id << "'></a>" ; 
  }
} 

void 
HTMLFile::FontColorStart(const char* color)
{
  // Note that <font> is illegal inside a <pre>.
  if (strlen(color) > 0) { 
    (*this)  << "<span style='color: " << color << ";'>";
  }
}

void 
HTMLFile::FontColorStop(const char* color)
{
  if (strlen(color) > 0) { 
   (*this)  << "</span>";  
  }
}

void 
HTMLFile::LayerIdStart(const char* id) 
{
  (*this) << "<ilayer id='" << id << "'>" ;
}

void 
HTMLFile::LayerIdStop()
{
   (*this)  << "</ilayer>";  
}

void
HTMLFile::WriteHighlightDiv()
{
  (*this) << "<div id='" << HIGHLIGHTER_DIV_NAME << "'>"
	  << "<table bgcolor='" << HIGHLIGHTER_COLOR
	  << "' cellspacing='0' border='0' cellpadding='0' width='100%'>"
	  << "<tr><td>&nbsp;</td></tr></table></div>" << endl << endl;
}

void 
HTMLFile::JavascriptHref(const char *jsCode, const char* displayName) 
{
  (*this)  << "<a href=\"javascript:" << jsCode << "\">" 
	   << displayName << "</a>"; 
}

void 
HTMLFile::GotoSrcHref(const char *displayName, const char *srcFile)
{
  (*this)  << "<a" 
	   << " onMouseOver=\"window.status ='" << displayName << "'; return true\""
	   << "  href=\"javascript:gotofile('" 
	   << srcFile << "')\">" 
	   << displayName << "</a>"; 
} 

void 
HTMLFile::LoadHref(const char *displayName, 
		   const char* frame, const char *file, unsigned int width)
{
  String txt = displayName; 
  String fill; 
  MakeFill(txt, fill, width); 
  (*this)  << "<a href=\"javascript:load_frame('" 
	   << frame << "','" << file << "')\">" 
	   << txt << "</a>" << fill; 
} 


void 
HTMLFile::SetTableAndScopes(const char* tableFrame, const char *tableFile, 
			    const char* selfFrame, 
			    // const char *selfFile, 
			    const char* kidsFrame, 
			    // const char *kidsFile,
			    const char *metricName,
			    const char *metric_display_name,
			    unsigned int width)
{
  String txt = "sort"; 
  String fill; 
  MakeFill(txt, fill, width); 
  (*this)  << "<a onMouseOver=\"window.status='Redisplay scopes sorted by " 
	   << metric_display_name << "'; return true\"" 
	   << " href=\"javascript:set_table_and_scopes('" 
	   << tableFrame << "','" <<  tableFile << "','" 
	   << selfFrame << "','" // <<  selfFile << "','" 
	   << kidsFrame << "','" // <<  kidsFile 
	   << metricName << "')\">" 
	   << txt << "</a>" << fill; 
} 

void 
HTMLFile::NavigateFrameHref(const char *anchor, const char* text, 
			    unsigned int width) 
{
  String txt = text; 
  String fill; 
  MakeFill(txt, fill, width); 

  // a new feature: make full text available in status window when 
  // mouse over

  (*this) 
    << "<a"
    << " onMouseOver=\"window.status ='" << text << "'; return true\"" 
    << " href=\"javascript:navframes('" 
    << anchor << "'" << ")\">" << HTMLEscapeStr(txt) << "</a>" << fill; 
} 

void 
HTMLFile::NavigateSourceHref(const char *navid, const char *parentid, 
			     const int scope_depth, const char* text, 
			     unsigned int width) 
{
  String txt = text; 
  String fill;
  MakeFill(txt, fill, width); 

  // a new feature: make full text available in status window when 
  // mouse over

  (*this)
    // << "<a name='" << navid << "'></a>"
    << "<a"
    << " onMouseOver=\"window.status ='" << text << "'; return true\""
    << " href=\"javascript:select_source_location('" 
    << navid << "','" << parentid << "',"
    << scope_depth << ")\">"
    << HTMLEscapeStr(txt) << "</a>" << fill; 
} 


void 
HTMLFile::NavigateFrameHrefForLoop(LoopScope* ls, unsigned int width, 
                int flattening) 
{
  // get handles on first and last lines in scope
  CodeInfo *first = ls->GetFirst();
  CodeInfo *last = ls->GetLast();

  FileScope *fs = ls->File();
  BriefAssertion( fs != NULL );
  String text1 = ScopeInfo::ScopeTypeToName(ScopeInfo::LOOP).c_str();
  text1 += String(" ") + fs->BaseName().c_str() + ": ";
  String text2 = text1;
  text1 += String(ls->begLine());
  text2 += String(ls->endLine());
  String anchor1 = 
     HTMLDriver::UniqueName(first, NO_PERF_INDEX, NO_FLATTEN_DEPTH);
  String anchor2 = 
     HTMLDriver::UniqueName(last, NO_PERF_INDEX, NO_FLATTEN_DEPTH);
  String realanchor1, realanchor2;
  if (first->begLine() == ls->begLine()) {
    realanchor1 = anchor1 + "X" + HTMLDriver::UniqueNameForSelf(ls) + "s";
  } else {
    realanchor1 = 
      HTMLDriver::UniqueName(ls, NO_PERF_INDEX, NO_FLATTEN_DEPTH) + "s";
  }
  if (last->endLine() == ls->endLine() ) {
    realanchor2 = anchor2 + "X" + HTMLDriver::UniqueNameForSelf(ls) + "e";
  } else {
    realanchor2 = 
      HTMLDriver::UniqueName(ls, NO_PERF_INDEX, NO_FLATTEN_DEPTH) + "e";
  }

  String text = ScopeInfo::ScopeTypeToName(ScopeInfo::LOOP).c_str();
  text += " " + String(ls->begLine()) + "-" + String(last->endLine());

  BriefAssertion( width >= strlen(text) );
  String txt = String(":") + fs->BaseName().c_str(); 
  String fill; 
  MakeFill(txt, fill, width - strlen(text)); 

  // a new feature: make full text available in status window when 
  // mouse over

  (*this)
    << ScopeInfo::ScopeTypeToName(ScopeInfo::LOOP) << " <a"
    << " onMouseOver=\"window.status ='" << text1 << "'; return true\"" 
    << " href=\"javascript:navframes('" << realanchor1 << "'" << ")\">" 
    << String(ls->begLine()) << "</a>-" 
    << "<a" << " onMouseOver=\"window.status ='" << text2 << "'; return true\""
    << " href=\"javascript:navframes('" << realanchor2 << "'" << ")\">" 
    << String(ls->endLine()) << "</a>"
    << HTMLEscapeStr(txt) << fill;
  if (flattening == NO_FLATTEN_DEPTH)
  {
    (*this)
      << "<a name='" << realanchor1 << "'></a>" 
      << "<a name='" << realanchor2 << "'></a>";
    if (first->begLine() == ls->begLine()) {
      (*this) << "<a name='" << anchor1 << "Y'></a>";
    }
    if (last->endLine() == ls->endLine() 
        && !(first->begLine() == ls->begLine() && first == last)) {
      // just check not to put the same anchor twice
      (*this) << "<a name='" << anchor2 << "Y'></a>";
    }
  }
  else
  {
    (*this)
      << "<a name='l" << flattening << "_" << realanchor1 << "'></a>" 
      << "<a name='l" << flattening << "_" << realanchor2 << "'></a>";

    if (first->begLine() == ls->begLine()) {
      (*this) << "<a name='l" << flattening << "_" << anchor1 << "Y'></a>";
    }
    if (last->endLine() == ls->endLine() 
        && !(first->begLine() == ls->begLine() && first == last)) {
      // just check not to put the same anchor twice
      (*this) << "<a name='l" << flattening << "_" << anchor2 << "Y'></a>";
    }
  }
} 


void 
HTMLFile::NavigateFrameHrefForProc(ProcScope* ps, unsigned int width, 
           int flattening) 
{
  // get handle on first line in scope
  CodeInfo *first = ps->GetFirst();

  FileScope *fs = ps->File();
  BriefAssertion( fs != NULL );
  String text  = HTMLTable::CodeName(*ps); 
  String anchor;
  if (first->begLine() == ps->begLine() ) {
    anchor = HTMLDriver::UniqueName(first, NO_PERF_INDEX, NO_FLATTEN_DEPTH)
      + "X" + HTMLDriver::UniqueNameForSelf(ps);
  } else {
    anchor = HTMLDriver::UniqueName(ps, NO_PERF_INDEX, NO_FLATTEN_DEPTH);
  }
  anchor += "s";
  String txt = text;
  String fill; 
  MakeFill(txt, fill, width); 

  (*this) << "<a"
	  << " onMouseOver=\"window.status ='" << text << "'; return true\"" 
	  << " href=\"javascript:navframes('" << anchor << "'" << ")\">" 
	  << txt << "</a>" << fill;
  if (flattening == NO_FLATTEN_DEPTH)
    (*this)
      << "<a name='" << anchor << "'></a>"; 
  else
    (*this)
      << "<a name='l" << flattening << "_" << anchor << "'></a>"; 
} 

void 
HTMLFile::ScopesSetCurrent(const char *anchor, const char* text, 
			   const int inout, const char *mouseOverText,
			   unsigned int width) 
{
  String txt = text; 
  String fill; 
  (*this) << "<a"
	   << " onMouseOver=\"window.status ='" << mouseOverText << "'; return true\"" 
	  << " href=\"javascript:navscopes('" << anchor << "'," << inout
	  << ")\">" << txt << "</a>" << fill; 
} 

void 
HTMLFile::JSFileInclude(StaticFileId id, const char* dir)
{
  BriefAssertion(IsJSFile(id)); 
  String prefix = ""; 
  if  (dir != NULL) {
    prefix = String(dir) + "/"; 
  } 
  (*this)  << "<script " << javaScriptAttr << " src='" 
	   << prefix << StaticFileName[id] << "'></script>" 
	   << endl;
} 

void 
HTMLFile::CSSFileInclude(StaticFileId id, const char* dir)
{
  String prefix = ""; 
  if  (dir != NULL) {
    prefix = String(dir) + "/"; 
  } 
  (*this)  << "<link rel='stylesheet' type='text/css' href='" 
	   << prefix << StaticFileName[id] << "'>"
	   << endl;
} 

// **************************************************************************

// Note: this is a copy of the XMLSubstitute function in HPCTools/src/libs/xml/
String HTMLSubstitute(const char* str, const String* fromStrs,
		     const String* toStrs); 
static const int HTMLnumSubs = 4; // number of substitutes
static const String HTMLRegStrs[]  = {"<",    ">",    "&",     "\""}; 
static const String HTMLEscStrs[]  = {"&lt;", "&gt;", "&amp;", "&quot;"};


String HTMLEscapeStr(const char* str)
{
  return HTMLSubstitute(str, HTMLRegStrs, HTMLEscStrs);
}

String HTMLUnEscapeStr(const char* str)
{
  return HTMLSubstitute(str, HTMLEscStrs, HTMLRegStrs);
}

String HTMLSubstitute(const char* str, const String* fromStrs,
		     const String* toStrs)
{
  static String newStr = String((const char*)"", 512);

  String retStr = str;
  if (!str) { return retStr; }

  // Iterate over 'str' and substitute patterns
  newStr = "";
  int strLn = strlen(str);  
  for (int i = 0; str[i] != '\0'; /* */) {

    // Attempt to find a pattern for substitution at this position
    int curSub = 0, curSubLn = 0;
    for (/*curSub = 0*/; curSub < HTMLnumSubs; curSub++) {
      curSubLn = fromStrs[curSub].Length();
      if ((strLn-i >= curSubLn) &&
	  (strncmp(str+i, fromStrs[curSub], curSubLn) == 0)) {
	break; // only one substitution possible per position
      }
    }

    // Substitute or copy current position; Adjust iteration to
    // inspect next character. (resizes if necessary)
    if (curSub < HTMLnumSubs) { // we found a string to substitute
      newStr += toStrs[curSub]; i += curSubLn;
    } else {
      newStr += str[i];         i++;
    }
  }
  retStr = newStr;

  return retStr;
}


#if 0
extern String HtmlFix(const char* str); 
          // returns str with <,>, and & replaced with &lt;, &gt;, &amp;

String
HtmlFix(const char* str)
{
  String fixed; 
  if (str != NULL) { 
    for (int i = 0; i < strlen(str); i++) {
      if (str[i] == '<') {
	fixed += "&lt;";
      } else if (str[i] == '>') {
	fixed += "&gt;";
      } else if (str[i] == '&') {
	fixed += "&amp;";
      } else {
	fixed += str[i]; 
      } 
    }
  }
  return fixed;
}
#endif
