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
using std::hex;
using std::dec;
using std::endl;

#include <string>
using std::string;

#include <map>
using std::map;

#include <vector>
using std::vector;

//************************* User Include Files *******************************


#include "Driver.hpp" 
#include "HPCViewSAX2.hpp"
#include "HPCViewXMLErrHandler.hpp"
#include "NodeRetriever.hpp"
#include "NodeRetriever.hpp"
#include "DerivedPerfMetrics.hpp" // FIXME: for AccumulateMetricsFromChildren

#include <lib/prof-juicy/PgmScopeTree.hpp>
#include <lib/prof-juicy/FlatProfileReader.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Assertion.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/pathfind.h>

//************************ Forward Declarations ******************************

//****************************************************************************

Driver::Driver(int deleteUnderscores, bool _cpySrcFiles)
  : Unique("Driver") 
{
  path = ".";
  deleteTrailingUnderscores = deleteUnderscores;
  cpySrcFiles = _cpySrcFiles;
} 

Driver::~Driver( )
{
} 

void
Driver::AddPath(const char* _path, const char* _viewname)
{
  path += string(":") + _path;
  pathVec.push_back( PathTuple(string(_path), string(_viewname)) );
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
    replaceInPath.push_back(string(inPath) + '/');
    replaceOutPath.push_back(string(outPath) + '/');
  }
  else {
    replaceInPath.push_back(string(inPath));
    replaceOutPath.push_back(string(outPath)); 
  }
  DIAG_Msg(3, "AddReplacePath: " << inPath << " -to- " << outPath);
}

/* Test the specified path against each of the paths in the database. 
 * Replace with the pair of the first matching path.
 */
string 
Driver::ReplacePath(const char* oldpath)
{
  BriefAssertion( replaceInPath.size() == replaceOutPath.size() );
  for( unsigned int i=0 ; i<replaceInPath.size() ; i++ ) {
    unsigned int length = replaceInPath[i].length();
    // it makes sense to test for matching only if 'oldpath' is strictly longer
    // than this replacement inPath.
    if (strlen(oldpath) > length &&  
	strncmp(oldpath, replaceInPath[i].c_str(), length) == 0 ) { 
      // it's a match
      string s = replaceOutPath[i] + &oldpath[length];
      DIAG_Msg(3, "ReplacePath: Found a match! New path: " << s);
      return s;
    }
  }
  // If nothing matched, return the original path
  DIAG_Msg(3, "ReplacePath: Nothing matched! Init path: " << oldpath);
  return string(oldpath);
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
  DIAG_Msg(3, "Add:: dataSrc[" << dataSrc.size()-1 << "]=" << m->ToString());
} 

string
Driver::ToString() const {
  string s =  string("Driver: " ) + "title=" + title + " " + 
    "path=" + path; 
  s += "\ndataSrc::\n"; 
  for (unsigned int i =0; i < dataSrc.size(); i++) {
    s += dataSrc[i]->ToString() + "\n"; 
  }
  return s; 
} 

void
Driver::ScopeTreeInitialize(PgmScopeTree& scopes) 
{
  NodeRetriever ret(scopes.GetRoot(), path);
  
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
}


void
Driver::ScopeTreeInsertProfileData(PgmScopeTree& scopes,
				   const std::list<std::string>& profileFiles)
{
  PgmScope* pgm = scopes.GetRoot();
  NodeRetriever nodeRet(pgm, path);
  
  map<string, int> disambiguationMap;
  vector<PerfMetric*> eventToPerfMetricMap;

  //-------------------------------------------------------
  // Read the profiles and insert the data
  //-------------------------------------------------------
  for (std::list<string>::const_iterator it = profileFiles.begin();
       it != profileFiles.end(); ++it) {
    
    ProfFile prof;
    int ret = prof.read(*it);
    DIAG_Assert(ret == 0, "While reading profile '" << *it << "'");

    for (unsigned int i = 0; i < prof.num_load_modules(); ++i) {

      const ProfFileLM& proflm = prof.load_module(i);
      const std::string& lmname = proflm.name();
      LoadModScope* lmScope = nodeRet.MoveToLoadMod(lmname);
      
      binutils::LM lm;
      if (!lm.Open(lmname.c_str())) { 
	exit(1); // Error already printed 
      }

      //-------------------------------------------------------
      // Create metrics for this profile (if necessary)
      //-------------------------------------------------------
      if (i == 0) {
	// Inspect only the first load module of each prof file because
	// while one prof file contains multiple load modules, each load
	// module contains the same events.
	eventToPerfMetricMap.resize(proflm.num_events());
	for (unsigned int j = 0; j < proflm.num_events(); ++j) {
	  const ProfFileEvent& profevent = proflm.event(j);
	  string name = profevent.name();
	  string qname = name;
	  
	  map<string, int>::iterator it = disambiguationMap.find(name);
	  if (it == disambiguationMap.end()) {
	    disambiguationMap[name] = 1;
	  }
	  else {
	    int qualification = it->second;
	    qname = name + StrUtil::toStr(qualification);
	    it->second++;
	  }
	  PerfMetric* m 
	    = new PerfMetric(qname, name, qname,
			     true/*display*/, false/*percent*/, 
			     false /*propComputed*/, false/*sortBy*/);
	  Add(m);
	  eventToPerfMetricMap[j] = m;
	}
      }
      
      //-------------------------------------------------------
      // Insert performance data into scope tree
      //-------------------------------------------------------
      for (unsigned int j = 0; j < proflm.num_events(); ++j) {
	const ProfFileEvent& profevent = proflm.event(j);
	PerfMetric* m = eventToPerfMetricMap[j];
	uint64_t period = profevent.period();
	
	for (unsigned int k = 0; k < profevent.num_data(); ++k) {
	  const ProfFileEventDatum& dat = profevent.datum(k);
	  VMA vma = dat.first; // relocated VMA
	  uint32_t samples = dat.second;
	  double events = samples * period; // samples * (events/sample)
	  
	  // 1. Unrelocate vma.
	  VMA ur_vma = vma;
	  if (lm.GetType() == binutils::LM::SharedLibrary 
	      && vma > proflm.load_addr()) {
	    ur_vma = vma - proflm.load_addr();
	  }
	  
	  // 2. Find associated scope and insert into scope tree
	  ScopeInfo* scope = lmScope->findByVMA(ur_vma);
	  if (!scope) {
	    DIAG_DevMsg(3, "Can't find non-LM scope for " << lmname << ":0x" 
			<< hex << ur_vma << dec);
	    scope = lmScope;
	  }
	  
	  double perfdata = events;
	  scope->SetPerfData(m->Index(), perfdata); // implicit add!
	  DIAG_DevMsg(6, "Metric associate: 0x" << hex << ur_vma << dec 
		      << " --> +" << perfdata << "=" 
		      << scope->PerfData(m->Index()) << " :: " 
		      << scope->toXML());
	}
      }
    }
  }

  DIAG_If(4) {
    DIAG_Msg(4, "Initial scope tree, before aggregation:");
    XML_Dump(pgm, 0, std::cerr);
  }
  
  //-------------------------------------------------------
  // Accumulate metrics
  //-------------------------------------------------------
  for (unsigned i = 0; i < eventToPerfMetricMap.size(); ++i) {
    PerfMetric* m = eventToPerfMetricMap[i];
    AccumulateMetricsFromChildren(pgm, m->Index());
  }
}


void
Driver::ScopeTreeInsertPROFILEData(PgmScopeTree& scopes) 
{
  NodeRetriever ret(scopes.GetRoot(), path);

  //-------------------------------------------------------
  // Create metrics, file and computed. (File metrics are read from
  // PROFILE files and will update the scope tree with any new
  // information; computed metrics are expressions of file metrics).
  //-------------------------------------------------------
  for (unsigned int i = 0; i < dataSrc.size(); i++) {
    try {
      dataSrc[i]->Make(ret);
    } 
    catch (const MetricException &x) {
      DIAG_EMsg("Could not construct METRIC '" << dataSrc[i]->Name() << "':" 
		<< x.error);
      exit(1);
    }
    catch (...) {
      throw;
    }
  } 
}


void
Driver::ProcessPGMFile(NodeRetriever* nretriever, 
		       PGMDocHandler::Doc_t docty, 
		       std::vector<string*>* files)
{
  if (!files) {
    return;
  }
  
  for (unsigned int i = 0; i < files->size(); i++) {
    const string& fnm = *((*files)[i]);
    if (!fnm.empty()) {
      const char* pf = pathfind(".", fnm.c_str(), "r");
      string fpath = (pf) ? pf : "";
      if (!fpath.empty()) {
	try {
	  SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	
	  parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
	  parser->setFeature(XMLUni::fgXercesDynamic, true);
	  parser->setFeature(XMLUni::fgXercesValidationErrorAsFatal, true);
	  
	  PGMDocHandler* handler = new PGMDocHandler(docty, nretriever, this);
	  parser->setContentHandler(handler);
	  parser->setErrorHandler(handler);
	  
	  parser->parse(fpath.c_str());

	  if (parser->getErrorCount() > 0) {
	    DIAG_EMsg("terminating because of previously reported " << PGMDocHandler::ToString(docty) << " file parse errors.");
	    exit(1);
	  }
	  delete handler;
	  delete parser;
	}
	catch (const SAXException& x) {
	  DIAG_EMsg("parsing '" << fpath << "'");
	  exit(1);
	}
	catch (const PGMException& x) {
	  DIAG_EMsg("reading '" << fpath << "'");
	  x.report(std::cerr);
	  exit(1);
	}
	catch (...) {
	  DIAG_EMsg("While processing '" << fpath << "'...");
	  throw;
	};
      } 
      else {
	DIAG_EMsg("Could not open " << PGMDocHandler::ToString(docty) 
		  << " file '" << fnm << "'.");
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
  string pre1 = string(pre) + "  ";
  string pre2 = string(pre1) + "  ";
  
  os << pre << "<HPCVIEWER>" << endl;

  // Dump CONFIG
  os << pre << "<CONFIG>" << endl;

  os << pre1 << "<TITLE name=\042" << Title() << "\042/>" << endl;

  const PathTupleVec& pVec = PathVec();
  for (unsigned int i = 0; i < pVec.size(); i++) {
    const string& pathStr = pVec[i].first;
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
  pgm->XML_DumpLineSorted(os, dumpFlags, pre);
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

