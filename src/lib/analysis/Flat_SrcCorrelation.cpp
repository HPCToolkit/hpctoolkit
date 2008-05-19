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

#include "Flat_SrcCorrelation.hpp"
#include "Util.hpp"

#include <lib/prof-juicy-x/PGMReader.hpp>

#include <lib/prof-juicy/PgmScopeTreeInterface.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>
#include <lib/prof-juicy/FlatProfileReader.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/Files.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

static bool 
copySourceFiles(PgmScope* structure, 
		const Analysis::PathTupleVec& pathVec, 
		const string& dstDir);

//****************************************************************************

namespace Analysis {

namespace Flat {

Driver::Driver(const Analysis::Args& args,
	       Prof::MetricDescMgr& mMgr, PgmScopeTree& structure)
  : Unique(), m_args(args), m_mMgr(mMgr), m_structure(structure)
{
} 


Driver::~Driver()
{
} 


int
Driver::run()
{
  //-------------------------------------------------------
  // 1. Initialize static program structure
  //-------------------------------------------------------
  DIAG_Msg(2, "Initializing scope tree...");
  populatePgmStructure(m_structure);

  DIAG_If(3) {
    DIAG_Msg(3, "Initial scope tree:");
    write_experiment(DIAG_CERR);
  }

  //-------------------------------------------------------
  // 2. Correlate metrics with program structure
  //-------------------------------------------------------
  DIAG_Msg(2, "Creating and correlating metrics with program structure: ...");
  correlateMetricsWithStructure(m_mMgr, m_structure);
  
  if (m_args.outFilename_CSV.empty() && m_args.outFilename_TSV.empty()) {
    // Prune the scope tree (remove structure without metrics)
    PruneScopeTreeMetrics(m_structure.GetRoot(), m_mMgr.size());
  }
  
  m_structure.GetRoot()->Freeze();      // disallow further additions to tree 
  m_structure.CollectCrossReferences(); // collect cross referencing information

  DIAG_If(3) {
    DIAG_Msg(3, "Final scope tree:");
    write_experiment(DIAG_CERR);
  }

  //-------------------------------------------------------
  // 3. Generate Experiment database
  //-------------------------------------------------------
  if (MakeDir(m_args.db_dir.c_str()) != 0) { // FIXME: pulled out of HTMLDriver
    return 1;
  }

  if (m_args.db_copySrcFiles) {
    DIAG_Msg(1, "Copying source files reached by REPLACE/PATH statements to " << m_args.db_dir);
    
    // Note that this may modify file names in the ScopeTree
    copySourceFiles(m_structure.GetRoot(), m_args.searchPathTpls, m_args.db_dir);
  }

  if (!m_args.outFilename_CSV.empty()) {
    const string& fnm = m_args.outFilename_CSV;
    DIAG_Msg(1, "Writing final scope tree (in CSV) to " << fnm);
    string fpath = m_args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_csv(*os);
    IOUtil::CloseStream(os);
  } 

  if (!m_args.outFilename_TSV.empty()) {
    const string& fnm = m_args.outFilename_TSV;
    DIAG_Msg(1, "Writing final scope tree (in TSV) to " << fnm);
    string fpath = m_args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_tsv(*os);
    IOUtil::CloseStream(os);
  }
  
  if (m_args.outFilename_XML != "no") {
    const string& fnm = m_args.outFilename_XML;
    DIAG_Msg(1, "Writing final scope tree (in XML) to " << fnm);
    string fpath = m_args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_experiment(*os);
    IOUtil::CloseStream(os);
  }

  return 0;
} 


void
Driver::write_experiment(std::ostream &os) const
{
  string pre  = "";
  string pre1 = pre + "  ";
  string pre2 = string(pre1) + "  ";
  
  os << pre << "<HPCVIEWER>" << endl;

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << pre << "<CONFIG>" << endl;

  os << pre1 << "<TITLE name=\"" << m_args.title << "\"/>" << endl;

  const Analysis::PathTupleVec& searchPaths = m_args.searchPathTpls;
  for (uint i = 0; i < searchPaths.size(); i++) {
    const string& path = searchPaths[i].first;
    os << pre1 << "<PATH name=\"" << path << "\"/>" << endl;
  }
  
  os << pre1 << "<METRICS>" << endl;
  for (uint i = 0; i < m_mMgr.size(); ++i) {
    const PerfMetric* metric = m_mMgr.metric(i); 
    os << pre2 << "<METRIC shortName=\"" << i
       << "\" nativeName=\""  << metric->NativeName()
       << "\" displayName=\"" << metric->DisplayInfo().Name()
       << "\" display=\"" << ((metric->Display()) ? "true" : "false")
       << "\" percent=\"" << ((metric->Percent()) ? "true" : "false")
      //<< "\" sortBy=\""  << ((metric->SortBy())  ? "true" : "false")
       << "\"/>" << endl;
  }
  os << pre1 << "</METRICS>" << endl;
  os << pre << "</CONFIG>" << endl;

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << pre << "<SCOPETREE>" << endl;
  int dumpFlags = (m_args.metrics_computeInteriorValues) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;
  m_structure.GetRoot()->XML_DumpLineSorted(os, dumpFlags, pre.c_str());
  os << pre << "</SCOPETREE>" << endl;

  os << pre << "</HPCVIEWER>" << endl;
}


void
Driver::write_csv(std::ostream &os) const
{
  os << "File name,Routine name,Start line,End line,Loop level";
  for (uint i = 0; i < m_mMgr.size(); ++i) {
    const PerfMetric* m = m_mMgr.metric(i); 
    os << "," << m->DisplayInfo().Name();
    if (m->Percent())
      os << "," << m->DisplayInfo().Name() << " (%)";
  }
  os << endl;
  
  // Dump SCOPETREE
  m_structure.GetRoot()->CSV_TreeDump(os);
}


void
Driver::write_tsv(std::ostream &os) const
{
  os << "LineID";
  for (uint i = 0; i < m_mMgr.size(); ++i) {
    const PerfMetric* m = m_mMgr.metric(i); 
    os << "\t" << m->DisplayInfo().Name();
    /*
    if (m->Percent())
      os << "\t" << m->DisplayInfo().Name() << " (%)";
    */
  }
  os << endl;
  
  // Dump SCOPETREE
  m_structure.GetRoot()->TSV_TreeDump(os);
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
  for (uint i = 0; i < m_mMgr.size(); i++) {
    const PerfMetric* m = m_mMgr.metric(i);
    const FilePerfMetric* mm = dynamic_cast<const FilePerfMetric*>(m);
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
  if (!m_mMgr.empty()) { os << "\n"; }
  
  os << "</HPCVIEW>\n";
}


string
Driver::toString() const 
{
  string s =  string("Driver: " )
    + "title=" + m_args.title + " " // + "path=" + path;
    + "\nm_metrics::\n"
    + m_mMgr.toString();
  return s; 
} 


void 
Driver::dump() const 
{ 
  std::cerr << toString() << std::endl; 
}


} // namespace Flat

} // namespace Analysis


//****************************************************************************

namespace Analysis {

namespace Flat {


void
Driver::populatePgmStructure(PgmScopeTree& structure)
{
  NodeRetriever structIF(structure.GetRoot(), SearchPathStr());
  DriverDocHandlerArgs docargs(this);
  
  //-------------------------------------------------------
  // if a PGM/Structure document has been provided, use it to 
  // initialize the structure of the scope tree
  //-------------------------------------------------------
  Prof::Struct::readStructure(structIF, m_args.structureFiles,
			      PGMDocHandler::Doc_STRUCT, docargs);

  //-------------------------------------------------------
  // if a PGM/Group document has been provided, use it to form the 
  // group partitions (as wall as initialize/extend the scope tree)
  //-------------------------------------------------------
  Prof::Struct::readStructure(structIF, m_args.groupFiles,
			      PGMDocHandler::Doc_GROUP, docargs);
}


void
Driver::correlateMetricsWithStructure(Prof::MetricDescMgr& mMgr,
				      PgmScopeTree& structure) 
{
  computeRawMetrics(mMgr, structure);
  computeDerivedMetrics(mMgr, structure);
}


void
Driver::computeRawMetrics(Prof::MetricDescMgr& mMgr, PgmScopeTree& structure) 
{
  // -------------------------------------------------------
  // Create metrics
  // -------------------------------------------------------
  // create HPCRUN file metrics.  Process all metrics associated with
  // a certain file.

  const Prof::MetricDescMgr::StringPerfMetricVecMap& fnameToFMetricMap = 
    mMgr.fnameToFMetricMap();
  
  for (Prof::MetricDescMgr::StringPerfMetricVecMap::const_iterator it = 
	 fnameToFMetricMap.begin();
       it != fnameToFMetricMap.end(); ++it) {
    const string& fnm = it->first;
    const Prof::MetricDescMgr::PerfMetricVec& metrics = it->second;
    computeFlatProfileMetrics(structure, fnm, metrics);

#if 0 //FIXME
    const FilePerfMetric* mm = dynamic_cast<const FilePerfMetric*>(m);
    if (mm) {
      DIAG_Assert(pgmScope && pgmScope->ChildCount() > 0, "Attempting to correlate HPCRUN type profile-files without STRUCTURE information.");
      fileToMetricMap[mm->FileName()].push_back(mm);
    }
#endif
  }
}


void
Driver::computeDerivedMetrics(Prof::MetricDescMgr& mMgr, 
			      PgmScopeTree& structure)
{
  // create PROFILE file and computed metrics
  NodeRetriever structIF(structure.GetRoot(), SearchPathStr());
  
  for (uint i = 0; i < mMgr.size(); i++) {
    const PerfMetric* m = mMgr.metric(i);
    const ComputedPerfMetric* mm = dynamic_cast<const ComputedPerfMetric*>(m);
    if (mm) {
      mm->Make(structIF);
    }
  } 
}


void
Driver::computeFlatProfileMetrics(PgmScopeTree& structure,
						  const string& profFilenm,
						  const Prof::MetricDescMgr::PerfMetricVec& metrics)
{
  PgmScope* pgm = structure.GetRoot();
  NodeRetriever structIF(pgm, SearchPathStr());
  
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
    lmname = replacePath(lmname);

    LoadModScope* lmScope = structIF.MoveToLoadMod(lmname);
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
    for (Prof::MetricDescMgr::PerfMetricVec::const_iterator it = metrics.begin(); 
	 it != metrics.end(); ++it) {
      FilePerfMetric* m = dynamic_cast<FilePerfMetric*>(*it);
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
    write_experiment(DIAG_CERR);
  }
  
  //-------------------------------------------------------
  // Accumulate metrics
  //-------------------------------------------------------
  for (Prof::MetricDescMgr::PerfMetricVec::const_iterator it = metrics.begin(); 
       it != metrics.end(); ++it) {
    FilePerfMetric* m = dynamic_cast<FilePerfMetric*>(*it);
    AccumulateMetricsFromChildren(pgm, m->Index());
  }
}


string 
Driver::replacePath(const char* oldpath)
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
      DIAG_Msg(3, "replacePath: Found a match! New path: " << s);
      return s;
    }
  }
  // If nothing matched, return the original path
  DIAG_Msg(3, "replacePath: Nothing matched! Init path: " << oldpath);
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

} // namespace Flat

} // namespace Analysis


//****************************************************************************
//
//****************************************************************************

static std::pair<int, string>
matchFileWithPath(const string& filenm, const Analysis::PathTupleVec& pathVec);


static bool 
CSF_ScopeFilter(const ScopeInfo& x, long type)
{
  return (x.Type() == ScopeInfo::FILE || x.Type() == ScopeInfo::ALIEN);
}

// 'copySourceFiles': For every file FileScope and AlienScope in
// 'pgmScope' that can be reached with paths in 'pathVec', copy F to
// its appropriate viewname path and update F's path to be relative to
// this location.
static bool
copySourceFiles(PgmScope* structure, const Analysis::PathTupleVec& pathVec,
		const string& dstDir)
{
  bool noError = true;

  // Alien scopes mean that we may attempt to copy the same file many
  // times.  Prevent multiple copies of the same file.
  std::map<string, string> copiedFiles;

  ScopeInfoFilter filter(CSF_ScopeFilter, "CSF_ScopeFilter", 0);
  for (ScopeInfoIterator it(structure, &filter); it.Current(); ++it) {
    ScopeInfo* scope = it.CurScope();
    FileScope* fileScope = NULL;
    AlienScope* alienScope = NULL;

    // Note: 'fnm_orig' will be not be absolute if it is not possible to find
    // the file on the current filesystem. (cf. NodeRetriever::MoveToFile)
    string fnm_orig;
    if (scope->Type() == ScopeInfo::FILE) {
      fileScope = dynamic_cast<FileScope*>(scope);
      fnm_orig = fileScope->name();
    }
    else if (scope->Type() == ScopeInfo::ALIEN) {
      alienScope = dynamic_cast<AlienScope*>(scope);
      fnm_orig = alienScope->fileName();
    }
    else {
      DIAG_Die(DIAG_Unimplemented);
    }
    
    string fnm_new;

    // ------------------------------------------------------
    // Given fnm_orig, find fnm_new. (Use PATH information.)
    // ------------------------------------------------------
    std::map<string, string>::iterator it = copiedFiles.find(fnm_orig);
    if (it != copiedFiles.end()) {
      fnm_new = it->second;
    }
    else {
      std::pair<int, string> fnd = matchFileWithPath(fnm_orig, pathVec);
      int idx = fnd.first;

      if (idx >= 0) {

	string  path_fnd = pathVec[idx].first; // a real copy
	string& fnm_fnd  = fnd.second;         // just an alias
	const string& viewnm   = pathVec[idx].second;

	// canonicalize path_fnd and fnm_fnd
	if (is_recursive_path(path_fnd.c_str())) {
	  path_fnd[path_fnd.length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
	}
	path_fnd = RealPath(path_fnd.c_str());
	fnm_fnd = RealPath(fnm_fnd.c_str());

	// INVARIANT 1: fnm_fnd is an absolute path
	// INVARIANT 2: path_fnd must be a prefix of fnm_fnd
	  
	// find (fnm_fnd - path_fnd)
	char* path_sfx = const_cast<char*>(fnm_fnd.c_str());
	path_sfx = &path_sfx[path_fnd.length()];
	while (path_sfx[0] != '/') { --path_sfx; } // should start with '/'
	
	// Create new file name and copy commands
	fnm_new = "./" + viewnm + path_sfx;
	
	string fnm_to;
	if (dstDir[0]  != '/') {
	  fnm_to = "./";
	}
	fnm_to = fnm_to + dstDir + "/" + viewnm + path_sfx;
	string dir_to(fnm_to); // need to strip off ending filename to 
	uint end;              // get full path for 'fnm_to'
	for (end = dir_to.length() - 1; dir_to[end] != '/'; end--) { }
	dir_to[end] = '\0';    // should not end with '/'
	
	string cmdMkdir = "mkdir -p " + dir_to;
	string cmdCp    = "cp -f " + fnm_fnd + " " + fnm_to;
	//cerr << cmdCp << std::endl;
	
	// could use CopyFile; see StaticFiles::Copy
	if (system(cmdMkdir.c_str()) == 0 && system(cmdCp.c_str()) == 0) {
	  DIAG_Msg(1, "  " << fnm_to);
	} 
	else {
	  DIAG_EMsg("copying: '" << fnm_to);
	}
      }
    }

    // ------------------------------------------------------
    // Use find fnm_new
    // ------------------------------------------------------
    if (!fnm_new.empty()) {
      if (fileScope) {
	fileScope->SetName(fnm_new);
      }
      else {
	alienScope->fileName(fnm_new);
      }
    }
    
    copiedFiles.insert(make_pair(fnm_orig, fnm_new));
  }
  
  return noError;
}


// 'matchFileWithPath': Given a file name 'filenm' and a vector of
// paths 'pathVec', use 'pathfind_r' to determine which path in
// 'pathVec', if any, reaches 'filenm'.  Returns an index and string
// pair.  If a match is found, the index is an index in pathVec;
// otherwise it is negative.  If a match is found, the string is the
// found file name.
static std::pair<int, string>
matchFileWithPath(const string& filenm, const Analysis::PathTupleVec& pathVec)
{
  // Find the index to the path that reaches 'filenm'.
  // It is possible that more than one path could reach the same
  //   file because of substrings.
  //   E.g.: p1: /p1/p2/p3/*, p2: /p1/p2/*, f: /p1/p2/p3/f.c
  //   Choose the path that is most qualified (We assume RealPath length
  //   is valid test.) 
  int foundIndex = -1; // index into 'pathVec'
  int foundPathLn = 0; // length of the path represented by 'foundIndex'
  string foundFnm; 

  for (uint i = 0; i < pathVec.size(); i++) {
    // find the absolute form of 'curPath'
    const string& curPath = pathVec[i].first;
    string realPath(curPath);
    if (is_recursive_path(curPath.c_str())) {
      realPath[realPath.length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
    }
    realPath = RealPath(realPath.c_str());
    int realPathLn = realPath.length();
       
    // 'filenm' should be relative as input for pathfind_r.  If 'filenm'
    // is absolute and 'realPath' is a prefix, make it relative. 
    string tmpFile(filenm);
    char* curFile = const_cast<char*>(tmpFile.c_str());
    if (filenm[0] == '/') { // is 'filenm' absolute?
      if (strncmp(curFile, realPath.c_str(), realPathLn) == 0) {
	curFile = &curFile[realPathLn];
	while (curFile[0] == '/') { ++curFile; } // should not start with '/'
      } 
      else {
	continue; // pathfind_r can't posibly find anything
      }
    }
    
    const char* fnd_fnm = pathfind_r(curPath.c_str(), curFile, "r");
    if (fnd_fnm) {
      bool update = false;
      if (foundIndex < 0) {
	update = true;
      }
      else if ((foundIndex >= 0) && (realPathLn > foundPathLn)) {
	update = true;
      }
      
      if (update) {
	foundIndex = i;
	foundPathLn = realPathLn;
	foundFnm = fnd_fnm;
      }
    }
  }
  return make_pair(foundIndex, foundFnm);
}

