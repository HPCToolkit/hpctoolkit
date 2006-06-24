// -*-Mode: C++;-*-
// $Id$
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

#include <iostream>

//************************* User Include Files *******************************


#include "Driver.hpp" 
#include "HPCViewSAX2.hpp"
#include "HPCViewXMLErrHandler.hpp"
#include "ScopesInfo.hpp"

#include <lib/support/Assertion.h>
#include <lib/support/pathfind.h>
#include <lib/support/Trace.hpp>

//************************ Forward Declarations ******************************

using std::cout;
using std::cerr;
using std::endl;

//****************************************************************************

Driver::Driver(int deleteUnderscores, bool _cpySrcFiles)
  : Unique("Driver") 
{
  path = ".";
  deleteTrailingUnderscores = deleteUnderscores;
  cpySrcFiles = _cpySrcFiles;
} 

Driver::~Driver() 
{
  IFTRACE << "~Driver " << endl; // << ToString() << endl; 
} 

void
Driver::AddPath(const char* _path, const char* _viewname)
{
  path += String(":") + _path;
  pathVec.push_back( PathTuple(String(_path), String(_viewname)) );
}


void
Driver::AddReplacePath(const char* inPath, const char* outPath)
{
  // Assumes error and warning check has been performed
  // (cf. HPCViewDocParser)

  // Add a '/' at the end of the in path; it's good when testing for
  // equality, to make sure that the last directory in the path is not
  // a prefix of the tested path.  
  // If we need to add a '/' to the in path, then add one to the out
  // path, too because when is time to replace we don't know if we
  // added one or not to the IN path.
  if (strlen(inPath)>0 && inPath[strlen(inPath)-1] != '/') {
    replaceInPath.push_back(String(inPath) + String('/') );
    replaceOutPath.push_back(String(outPath) + String('/') );
  }
  else {
    replaceInPath.push_back(String(inPath));
    replaceOutPath.push_back(String(outPath)); 
  }
  IFTRACE << "AddReplacePath: " << inPath << " -to- " << outPath << endl; 
}

/* Test the specified path against each of the paths in the database. 
 * Replace with the pair of the first matching path.
 */
String 
Driver::ReplacePath(const char* oldpath)
{
  BriefAssertion( replaceInPath.size() == replaceOutPath.size() );
  for( unsigned int i=0 ; i<replaceInPath.size() ; i++ ) {
    unsigned int length = replaceInPath[i].Length();
    // it makes sense to test for matching only if 'oldpath' is strictly longer
    // than this replacement inPath.
    if (strlen(oldpath) > length &&  
	strncmp(oldpath, replaceInPath[i], length) == 0 ) { // it's a match
      String s = replaceOutPath[i] + &oldpath[length];
      IFTRACE << "ReplacePath: Found a match! New path: " << s << endl;
      return s;
    }
  }
  // If nothing matched, return the original path
  IFTRACE << "ReplacePath: Nothing matched! Init path: " << oldpath << endl;
  return String(oldpath);
}


bool
Driver::MustDeleteUnderscore()
{
  return (deleteTrailingUnderscores > 0);
}

void
Driver::Add(PerfMetric *m) 
{
  dataSrc.push_back(m); 
  IFTRACE << "Add:: dataSrc[" << dataSrc.size()-1 << "]=" 
	  << m->ToString() << endl; 
} 

String
Driver::ToString() const {
  String s =  String("Driver: " ) + "title=" + title + " " + 
    "path=" + path; 
  s += "\ndataSrc::\n"; 
  for (unsigned int i =0; i < dataSrc.size(); i++) {
    s += dataSrc[i]->ToString() + "\n"; 
  }
  return s; 
} 

void
Driver::MakePerfData(ScopesInfo &scopes) 
{
  NodeRetriever ret(scopes.Root(), path);
  
  //-------------------------------------------------------
  // if a PGM/Structure document has been provided, use it to 
  // initialize the structure of the scope tree
  //-------------------------------------------------------
  if (NumberOfStructureFiles() > 0) {
    ProcessPGMFile(&ret, PGMDocHandler::Doc_STRUCT, &structureFiles);
  }

  //-------------------------------------------------------
  // if a PGM/Group document has been provided, use it to form the 
  // group partitions (as wall as initialize/extend the scope tree)
  //-------------------------------------------------------
  if (NumberOfGroupFiles() > 0) {
    ProcessPGMFile(&ret, PGMDocHandler::Doc_GROUP, &groupFiles);
  }
  
  //-------------------------------------------------------
  // Create metrics, file and computed. (File metrics are read from
  // PROFILE files and will update the scope tree with any new
  // information; computed metrics are expressions of file metrics).
  //-------------------------------------------------------
  for (unsigned int i = 0; i < dataSrc.size(); i++) {
    try {
      dataSrc[i]->Make(ret);
    } 
    catch (const MetricException &mex) {
      cerr << "hpcview fatal error: Could not construct METRIC '" 
	   << dataSrc[i]->Name() << "'." << endl
	   << "\t" << mex.error << endl;
      exit(1);
    } 
  } 
}


void
Driver::ProcessPGMFile(NodeRetriever* nretriever, 
		       PGMDocHandler::Doc_t docty, 
		       std::vector<String*>* files)
{
  if (!files) {
    return;
  }
  
  for (unsigned int i = 0; i < files->size(); i++) {
    String& pgmFileName = *((*files)[i]);
    if (!pgmFileName.Empty()) {
      String filePath = String(pathfind(".", pgmFileName, "r")); 
      if (!filePath.Empty()) {
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	
	parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
	parser->setFeature(XMLUni::fgXercesDynamic, true);
	parser->setFeature(XMLUni::fgXercesValidationErrorAsFatal, true);
	
	PGMDocHandler handler(docty, nretriever, this);
	parser->setContentHandler(&handler);
	parser->setErrorHandler(&handler);
	
	try {
	  parser->parse(filePath);
	  if (parser->getErrorCount() > 0) {
	    cerr << "hpcview fatal error: terminating because of previously reported " << PGMDocHandler::ToString(docty) << " file parse errors." << endl;
	    exit(1);
	  }
	}
	catch (const SAXException& toCatch) {
	  cerr << "hpcview fatal error: terminating because of previously reported " << PGMDocHandler::ToString(docty) << " file parse errors." << endl;
	  exit(1);
	}
	catch (const PGMException& toCatch) {
	  cerr << "ERROR: '" << filePath << "': " << toCatch.message() << endl;
	  exit(1);
	}
      } 
      else {
	cerr << "hpcview fatal error: could not open " 
	     << PGMDocHandler::ToString(docty) << " file '" 
	     << pgmFileName << "'." << endl;
	exit(1);
      }
    }
  }
}


void
Driver::XML_Dump(PgmScope* pgm, std::ostream &os, const char *pre) const
{
  int dumpFlags = 0; // no dump flags
  XML_Dump(pgm, dumpFlags, os, pre);
}

void
Driver::XML_Dump(PgmScope* pgm, int dumpFlags, std::ostream &os,
		 const char *pre) const
{
  String pre1 = String(pre) + "  ";
  String pre2 = String(pre1) + "  ";
  
  os << pre << "<HPCVIEWER>" << endl;

  // Dump CONFIG
  os << pre << "<CONFIG>" << endl;

  os << pre1 << "<TITLE name=\042" << Title() << "\042/>" << endl;

  const PathTupleVec& pVec = PathVec();
  for (unsigned int i = 0; i < pVec.size(); i++) {
    const char* pathStr = pVec[i].first;
    os << pre1 << "<PATH name=\042" << pathStr << "\042/>" << endl;
  }
  
  os << pre1 << "<METRICS>" << endl;
  for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
    const PerfMetric& metric = IndexToPerfDataInfo(i); 
    os << pre2 << "<METRIC shortName=\042" << i
       << "\042 nativeName=\042"  << metric.NativeName()
       << "\042 displayName=\042" << metric.DisplayInfo().Name()
       << "\042 display=\042" << ((metric.Display()) ? "true" : "false")
       << "\042 percent=\042" << ((metric.Percent()) ? "true" : "false")
#if 0   // hpcviewer does dynamic sorting, no need for this flag
       << "\042 sortBy=\042"  << ((metric.SortBy())  ? "true" : "false")
#endif
       << "\042/>" << endl;
  }
  os << pre1 << "</METRICS>" << endl;
  os << pre << "</CONFIG>" << endl;
  
  // Dump SCOPETREE
  os << pre << "<SCOPETREE>" << endl;
  pgm->XML_Dump(os, dumpFlags, pre);
  os << pre << "</SCOPETREE>" << endl;

  os << pre << "</HPCVIEWER>" << endl;
}

void
Driver::CSV_Dump(PgmScope* pgm, std::ostream &os) const
{
  os << "File name,Routine name,Start line,End line,Loop level";
  for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
    const PerfMetric& metric = IndexToPerfDataInfo(i); 
    os << "," << metric.DisplayInfo().Name();
    if (metric.Percent())
      os << "," << metric.DisplayInfo().Name() << " (%)";
  }
  os << endl;
  
  // Dump SCOPETREE
  pgm->CSV_TreeDump(os);
}

void
Driver::TSV_Dump(PgmScope* pgm, std::ostream &os) const
{
  os << "LineID";
  for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
    const PerfMetric& metric = IndexToPerfDataInfo(i); 
    os << "\t" << metric.DisplayInfo().Name();
    /*
    if (metric.Percent())
      os << "\t" << metric.DisplayInfo().Name() << " (%)";
    */
  }
  os << endl;
  
  // Dump SCOPETREE
  pgm->TSV_TreeDump(os);
}

