// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#include <lib/prof-juicy-x/PGMReader.hpp>
#include <lib/prof-juicy-x/XercesSAX2.hpp>
#include <lib/prof-juicy-x/XercesErrorHandler.hpp>

#include <lib/prof-juicy/PgmScopeTreeInterface.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>
#include <lib/prof-juicy/FlatProfileReader.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/pathfind.h>

//************************ Forward Declarations ******************************


//****************************************************************************

Driver::Driver(Analysis::Args& args)
  : Unique("Driver"), m_args(args) 
{
} 


Driver::~Driver()
{
} 


void
Driver::addMetric(PerfMetric *m) 
{
  m_metrics.push_back(m); 
  DIAG_Msg(3, "Add:: m_metrics[" << m_metrics.size()-1 << "]=" << m->ToString());
} 


/* Test the specified path against each of the paths in the database. 
 * Replace with the pair of the first matching path.
 */
string 
Driver::ReplacePath(const char* oldpath)
{
  DIAG_Assert(m_args.replaceInPath.size() == m_args.replaceOutPath.size(), "");
  for (uint i = 0 ; i<m_args.replaceInPath.size() ; i++ ) {
    uint length = m_args.replaceInPath[i].length();
    // it makes sense to test for matching only if 'oldpath' is strictly longer
    // than this replacement inPath.
    if (strlen(oldpath) > length &&  
	strncmp(oldpath, m_args.replaceInPath[i].c_str(), length) == 0 ) { 
      // it's a match
      string s = m_args.replaceOutPath[i] + &oldpath[length];
      DIAG_Msg(3, "ReplacePath: Found a match! New path: " << s);
      return s;
    }
  }
  // If nothing matched, return the original path
  DIAG_Msg(3, "ReplacePath: Nothing matched! Init path: " << oldpath);
  return string(oldpath);
}


string 
Driver::SearchPathStr() const
{
  string path = ".";
  
  for (uint i = 0; i < m_args.searchPathTpls.size(); ++i) { 
    path += string(":") + m_args.searchPathTpls[i].first;
  }

  return path;
}


string 
Driver::makeUniqueName(StringToIntMap& tbl, const std::string& nm)
{
  StringToIntMap::iterator it = tbl.find(nm);
  if (it != tbl.end()) {
    int& qualifier = it->second;
    qualifier++;
    string nm_new = nm + "-" + StrUtil::toStr(qualifier);
    return nm_new;
  }
  else {
    tbl.insert(make_pair(nm, 0));
    return nm;
  }
}


string
Driver::ToString() const 
{
  string s =  string("Driver: " )
    + "title=" + m_args.title + " " // + "path=" + path;
    + "\nm_metrics::\n"; 
  for (uint i =0; i < m_metrics.size(); i++) {
    s += m_metrics[i]->ToString() + "\n"; 
  }
  return s; 
} 


//****************************************************************************

void 
Driver::makePerfMetricDescs(std::vector<std::string>& profileFiles)
{
  StringToIntMap metricDisambiguationTbl;

  for (uint i = 0; i < profileFiles.size(); ++i) {
    const string& proffnm = profileFiles[i];

    Prof::Flat::Profile prof;
    try {
      prof.open(proffnm.c_str());
    }
    catch (...) {
      DIAG_EMsg("While reading '" << proffnm << "'");
      throw;
    }

    // For each metric: compute a canonical name and create a metric desc.
    const Prof::SampledMetricDescVec& mdescs = prof.mdescs();
    for (uint j = 0; j < mdescs.size(); ++j) {
      const Prof::SampledMetricDesc& m = *mdescs[j];
      
      string metricNm = makeUniqueName(metricDisambiguationTbl, m.name());
      string nativeNm = StrUtil::toStr(j);
      bool metricDoSortBy = (numMetrics() == 0);
      
      addMetric(new FilePerfMetric(metricNm, nativeNm, metricNm, 
				   true /*display*/, true /*percent*/, 
				   metricDoSortBy, proffnm, 
				   string("HPCRUN")));
    }
  }
}


void
Driver::createProgramStructure(PgmScopeTree& scopes) 
{
  NodeRetriever ret(scopes.GetRoot(), SearchPathStr());
  
  //-------------------------------------------------------
  // if a PGM/Structure document has been provided, use it to 
  // initialize the structure of the scope tree
  //-------------------------------------------------------
  if (!m_args.structureFiles.empty()) {
    ProcessPGMFile(&ret, PGMDocHandler::Doc_STRUCT, m_args.structureFiles);
  }

  //-------------------------------------------------------
  // if a PGM/Group document has been provided, use it to form the 
  // group partitions (as wall as initialize/extend the scope tree)
  //-------------------------------------------------------
  if (!m_args.groupFiles.empty()) {
    ProcessPGMFile(&ret, PGMDocHandler::Doc_GROUP, m_args.groupFiles);
  }
}


void
Driver::correlateMetricsWithStructure(PgmScopeTree& scopes) 
{
  // -------------------------------------------------------
  // Create metrics, file and computed metrics. 
  //
  // (File metrics are read from PROFILE/HPCRUN files and will update
  // the scope tree with any new information; computed metrics are
  // expressions of file metrics).
  // -------------------------------------------------------
  try {
    computeSampledMetrics(scopes);
    computeDerivedMetrics(scopes);
  } 
  catch (const MetricException &x) {
    DIAG_EMsg(x.message());
    throw;
  }
  catch (...) {
    throw;
  }
}


void
Driver::computeSampledMetrics(PgmScopeTree& scopes) 
{
  typedef std::map<string, MetricList_t> StringToMetricListMap_t;
  StringToMetricListMap_t fileToMetricMap;

  // create HPCRUN file metrics.  Process all metrics associated with
  // a certain file.

  PgmScope* pgmScope = scopes.GetRoot();
  NodeRetriever ret(pgmScope, SearchPathStr());
  for (uint i = 0; i < m_metrics.size(); i++) {
    PerfMetric* m = m_metrics[i];
    FilePerfMetric* mm = dynamic_cast<FilePerfMetric*>(m);
    if (mm) {
      DIAG_Assert(pgmScope && pgmScope->ChildCount() > 0, "Attempting to correlate HPCRUN type profile-files without STRUCTURE information.");
      fileToMetricMap[mm->FileName()].push_back(mm);
    }
  }
  
  for (StringToMetricListMap_t::iterator it = fileToMetricMap.begin();
       it != fileToMetricMap.end(); ++it) {
    const string& fnm = it->first;
    const MetricList_t& metrics = it->second;
    computeFlatProfileMetrics(scopes, fnm, metrics);
  }
}


void
Driver::computeDerivedMetrics(PgmScopeTree& scopes) 
{
  // create PROFILE file and computed metrics
  NodeRetriever ret(scopes.GetRoot(), SearchPathStr());
  
  for (uint i = 0; i < m_metrics.size(); i++) {
    PerfMetric* m = m_metrics[i];
    ComputedPerfMetric* mm = dynamic_cast<ComputedPerfMetric*>(m);
    if (mm) {
      mm->Make(ret);
    }
  } 
}


void
Driver::computeFlatProfileMetrics(PgmScopeTree& scopes,
				  const string& profFilenm,
				  const MetricList_t& metricList)
{
  PgmScope* pgm = scopes.GetRoot();
  NodeRetriever nodeRet(pgm, SearchPathStr());
  
  vector<PerfMetric*> eventToPerfMetricMap;

  //-------------------------------------------------------
  // Read the profile and insert the data
  //-------------------------------------------------------
  Prof::Flat::Profile prof;
  try {
    prof.read(profFilenm.c_str());
  }
  catch (...) {
    DIAG_EMsg("While reading '" << profFilenm << "'");
    throw;
  }

  uint num_samples = 0;
  
  for (Prof::Flat::Profile::const_iterator it = prof.begin(); 
       it != prof.end(); ++it) {
    const Prof::Flat::LM* proflm = it->second;
    std::string lmname = proflm->name();
    lmname = ReplacePath(lmname);

    LoadModScope* lmScope = nodeRet.MoveToLoadMod(lmname);
    if (lmScope->ChildCount() == 0) {
      DIAG_WMsg(1, "No STRUCTURE for " << lmname << ".");
    }


    binutils::LM* lm = NULL;
    try {
      lm = new binutils::LM();
      lm->open(lmname.c_str());
    }
    catch (const binutils::Exception& x) {
      DIAG_EMsg("While opening " << lmname.c_str() << ":\n" << x.message());
      continue;
    }
    catch (...) {
      DIAG_EMsg("Exception encountered while opening " << lmname.c_str());
      throw;
    }
    
    //-------------------------------------------------------
    // For each metric, insert performance data into scope tree
    //-------------------------------------------------------
    for (MetricList_t::const_iterator it = metricList.begin(); 
	 it != metricList.end(); ++it) {
      FilePerfMetric* m = *it;
      uint mIdx = (uint)StrUtil::toUInt64(m->NativeName());
      
      const Prof::Flat::EventData& profevent = proflm->event(mIdx);
      unsigned long period = profevent.mdesc().period();
      double mval = 0.0;
      double mval_nostruct = 0.0;

      for (uint k = 0; k < profevent.num_data(); ++k) {
	const Prof::Flat::Datum& dat = profevent.datum(k);
	VMA vma = dat.first; // relocated VMA
	uint32_t samples = dat.second;
	double events = samples * period; // samples * (events/sample)
	
	// 1. Unrelocate vma.
	VMA ur_vma = vma;
	if (lm->type() == binutils::LM::SharedLibrary 
	    && vma > proflm->load_addr()) {
	  ur_vma = vma - proflm->load_addr();
	}
	
	// 2. Find associated scope and insert into scope tree
	ScopeInfo* scope = lmScope->findByVMA(ur_vma);
	if (!scope) {
	  if (!m_args.structureFiles.empty() && lmScope->ChildCount() > 0) {
	    DIAG_WMsg(3, "Cannot find STRUCTURE for " << lmname << ":0x" << hex << ur_vma << dec << " [" << m->Name() << ", " << events << "]");
	  }
	  scope = lmScope;
	  mval_nostruct += events;
	}

	mval += events;
	scope->SetPerfData(m->Index(), events); // implicit add!
	DIAG_DevMsg(6, "Metric associate: " 
		    << m->Name() << ":0x" << hex << ur_vma << dec 
		    << " --> +" << events << "=" 
		    << scope->PerfData(m->Index()) << " :: " 
		    << scope->toXML());
      }

      num_samples += profevent.num_data();
      if (mval_nostruct > 0.0) {
	// INVARIANT: division by 0 is impossible since mval >= mval_nostruct
	double percent = (mval_nostruct / mval) * 100;
	DIAG_WMsg(1, "Cannot find STRUCTURE for " << m->Name() << ":" << mval_nostruct << " (" << percent << "%) in " << lmname << ". (Large percentages indicate useless STRUCTURE information. Stale STRUCTURE file? Did STRUCTURE binary have debugging info?)");
      }
    }

    delete lm;
  }

  if (num_samples == 0) {
    DIAG_WMsg(1, profFilenm << " contains no samples!");
  }

  DIAG_If(3) {
    DIAG_Msg(3, "Initial scope tree, before aggregation:");
    XML_Dump(pgm, 0, std::cerr);
  }
  
  //-------------------------------------------------------
  // Accumulate metrics
  //-------------------------------------------------------
  for (MetricList_t::const_iterator it = metricList.begin(); 
       it != metricList.end(); ++it) {
    FilePerfMetric* m = *it;
    AccumulateMetricsFromChildren(pgm, m->Index());
  }
}


void
Driver::ProcessPGMFile(NodeRetriever* nretriever, 
		       PGMDocHandler::Doc_t docty, 
		       const std::vector<string>& files)
{
  for (uint i = 0; i < files.size(); ++i) {
    const string& fnm = files[i];
    DriverDocHandlerArgs args(this);
    read_PGM(nretriever, fnm.c_str(), docty, args);
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

  os << pre1 << "<TITLE name=\042" << m_args.title << "\042/>" << endl;

  const Analysis::PathTupleVec& pVec = m_args.searchPathTpls;
  for (uint i = 0; i < pVec.size(); i++) {
    const string& pathStr = pVec[i].first;
    os << pre1 << "<PATH name=\042" << pathStr << "\042/>" << endl;
  }
  
  os << pre1 << "<METRICS>" << endl;
  for (uint i = 0; i < NumberOfPerfDataInfos(); i++) {
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
Driver::write_config(std::ostream &os) const
{
  os << "<HPCVIEW>\n\n";

  // title
  os << "<TITLE name=\"" << m_args.title << "\"/>\n\n";

  // search paths
  for (uint i = 0; i < m_args.searchPaths.size(); ++i) { 
    const Analysis::PathTuple& x = m_args.searchPathTpls[i];
    os << "<PATH name=\"" << x.first << "\" viewname=\"" << x.second <<"\"/>\n";
  }
  if (!m_args.searchPaths.empty()) { os << "\n"; }
  
  // structure files
  for (uint i = 0; i < m_args.structureFiles.size(); ++i) { 
    os << "<STRUCTURE name=\"" << m_args.structureFiles[i] << "\"/>\n";
  }
  if (!m_args.structureFiles.empty()) { os << "\n"; }

  // group files
  for (uint i = 0; i < m_args.groupFiles.size(); ++i) { 
    os << "<STRUCTURE name=\"" << m_args.groupFiles[i] << "\"/>\n";
  }
  if (!m_args.groupFiles.empty()) { os << "\n"; }

  // metrics
  for (uint i = 0; i < m_metrics.size(); i++) {
    PerfMetric* m = m_metrics[i];
    FilePerfMetric* mm = dynamic_cast<FilePerfMetric*>(m);
    if (mm) {
      const char* sortbystr = ((i == 0) ? " sortBy=\"true\"" : "");
      os << "<METRIC name=\"" << m->Name() 
	 << "\" displayName=\"" << m->DisplayInfo().Name() << "\"" 
	 << sortbystr << ">\n";
      os << "  <FILE name=\"" << mm->FileName() 
	 << "\" select=\"" << mm->NativeName()
	 << "\" type=\"" << mm->FileType() << "\"/>\n";
      os << "</METRIC>\n\n";
    }
  }
  if (!m_metrics.empty()) { os << "\n"; }
  
  os << "</HPCVIEW>\n";
}



void
Driver::CSV_Dump(PgmScope* pgm, std::ostream &os) const
{
  os << "File name,Routine name,Start line,End line,Loop level";
  for (uint i = 0; i < NumberOfPerfDataInfos(); i++) {
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
  for (uint i = 0; i < NumberOfPerfDataInfos(); i++) {
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

