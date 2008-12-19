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

#include <climits>

//************************* User Include Files *******************************

#include "Flat-SrcCorrelation.hpp"
#include "TextUtil.hpp"
#include "Util.hpp"

#include <lib/prof-juicy-x/PGMReader.hpp>

#include <lib/prof-juicy/Struct-TreeInterface.hpp>
#include <lib/prof-juicy/Struct-Tree.hpp>
#include <lib/prof-juicy/Flat-ProfileData.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/Files.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Analysis {

namespace Flat {

uint Driver::profileBatchSz = 16; // UINT_MAX;


Driver::Driver(const Analysis::Args& args,
	       Prof::Metric::Mgr& mMgr, Prof::Struct::Tree& structure)
  : Unique(), m_args(args), m_mMgr(mMgr), m_structure(structure)
{
} 


Driver::~Driver()
{
} 


int
Driver::run()
{
  if (m_mMgr.empty()) {
    return 0;
  }

  //-------------------------------------------------------
  // 1. Initialize static program structure
  //-------------------------------------------------------
  DIAG_Msg(2, "Initializing structure...");
  populatePgmStructure(m_structure);

  DIAG_If(3) {
    DIAG_Msg(3, "Initial structure:");
    write_experiment(DIAG_CERR);
  }

  //-------------------------------------------------------
  // 2. Correlate metrics with program structure
  //    (Results in a pruned structure tree)
  //-------------------------------------------------------
  DIAG_Msg(2, "Creating and correlating metrics with program structure: ...");
  correlateMetricsWithStructure(m_mMgr, m_structure);
  
  m_structure.GetRoot()->Freeze();      // disallow further additions to tree 
  m_structure.CollectCrossReferences(); // collect cross referencing information

  DIAG_If(3) {
    DIAG_Msg(3, "Final structure:");
    write_experiment(DIAG_CERR);
  }

  //-------------------------------------------------------
  // 3. Generate Experiment database
  //-------------------------------------------------------
  string db_dir = m_args.db_dir; // make copy
  bool db_use = !db_dir.empty() && db_dir != "-";
  if (db_use) {
    std::pair<string, bool> ret = FileUtil::mkdirUnique(db_dir.c_str());
    db_dir = ret.first; // exits on failure...
  }

  if (db_use && m_args.db_copySrcFiles) {
    DIAG_Msg(1, "Copying source files reached by PATH/REPLACE options to " << db_dir);
    // NOTE: makes file names in m_structure relative to database
    Analysis::Util::copySourceFiles(m_structure.GetRoot(), 
				    m_args.searchPathTpls, 
				    db_dir);
  }

  const string out_path = (db_use) ? (db_dir + "/") : "";

  if (!m_args.out_db_experiment.empty()) {
    const string& fnm = m_args.out_db_experiment;
    DIAG_Msg(1, "Writing final scope tree (in XML) to " << fnm);
    string fpath = out_path + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_experiment(*os);
    IOUtil::CloseStream(os);
  }

  if (!m_args.out_db_csv.empty()) {
    const string& fnm = m_args.out_db_csv;
    DIAG_Msg(1, "Writing final scope tree (in CSV) to " << fnm);
    string fpath = out_path + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_csv(*os);
    IOUtil::CloseStream(os);
  } 

  if (!m_args.out_txt.empty()) {
    const string& fnm = m_args.out_txt;
    string fpath = out_path + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_txt(*os);
    IOUtil::CloseStream(os);
  }

  // configuration file
  if (!m_args.out_db_config.empty()) {
    const string& fnm = m_args.out_db_config;
    string fpath = out_path + fnm;
    DIAG_Msg(1, "Writing configuration file to " << fpath);

    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    write_config(*os);
    IOUtil::CloseStream(os);
  }

  return 0;
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


void
Driver::write_experiment(std::ostream &os) const
{
  static const char* experimentDTD =
#include <lib/xml/hpc-experiment.dtd.h>
  
  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE hpc-experiment [\n" << experimentDTD << "]>" << std::endl;
  os << "<HPCToolkitExperiment version=\"2.0\">\n";
  os << "<Header n" << MakeAttrStr(m_args.title) << ">\n";
  os << "  <Info/>\n";
  os << "</Header>\n";

  os << "<SecFlatProfile i=\"0\" n" << MakeAttrStr(m_args.title) << ">\n";

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << "<SecHeader>\n";

  os << "  <MetricTable>\n";
  for (uint i = 0; i < m_mMgr.size(); ++i) {
    const PerfMetric* m = m_mMgr.metric(i);
    os << "    <Metric i" << MakeAttrNum(i)
       << " n" << MakeAttrStr(m->DisplayInfo().Name())
       << " show=\"" << ((m->Display()) ? "1" : "0") << "\"/>";
    os << "<Info>"
       << "<NV n=\"percent\" v=\"" << ((m->dispPercent()) ? "1" : "0") << "\"/>"
       << "</Info>\n";
  }
  os << "  </MetricTable>\n";

  os << "  <Info n=\"SearchPaths\">\n";
  const Analysis::PathTupleVec& searchPaths = m_args.searchPathTpls;
  for (uint i = 0; i < searchPaths.size(); i++) {
    const string& path = searchPaths[i].first;
    os << "    <NV n" << MakeAttrNum(i) << " v" << MakeAttrStr(path) << "/>\n";
  }
  os << "  </Info>\n";

  os << "</SecHeader>\n";
  os.flush();
  
  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  int dumpFlags = ((m_args.metrics_computeInteriorValues) 
		   ? 0 : Prof::Struct::Tree::DUMP_LEAF_METRICS);

  os << "<SecFlatProfileData>\n";
  m_structure.GetRoot()->writeXML(os, dumpFlags);
  os << "</SecFlatProfileData>\n";

  os << "</SecFlatProfile>\n";
  os << "</HPCToolkitExperiment>\n";
  os.flush();
}


void
Driver::write_csv(std::ostream &os) const
{
  os << "File name,Routine name,Start line,End line,Loop level";
  for (uint i = 0; i < m_mMgr.size(); ++i) {
    const PerfMetric* m = m_mMgr.metric(i); 
    os << "," << m->DisplayInfo().Name();
    if (m->dispPercent())
      os << "," << m->DisplayInfo().Name() << " (%)";
  }
  os << endl;
  
  // Dump SCOPETREE
  m_structure.GetRoot()->CSV_TreeDump(os);
}


void
Driver::write_txt(std::ostream &os) const
{
  using Analysis::TextUtil::ColumnFormatter;
  using Prof::Struct::ANodeTyFilter;
  using namespace Prof;

  //write_experiment(os);

  PerfMetric* m_sortby = m_mMgr.findSortBy();
  DIAG_Assert(m_sortby, "INVARIANT: at least on sort-by metric must exist");

  Struct::Pgm* pgmStrct = m_structure.GetRoot();

  ColumnFormatter colFmt(m_mMgr, os, 2, 0);

  os << std::setfill('=') << std::setw(77) << "=" << std::endl;
  colFmt.genColHeaderSummary();
  os << std::endl;

  if (m_args.txt_summary & Analysis::Args::TxtSum_fPgm) { 
    string nm = "Program summary (row 1: sample count for raw metrics): "
      + pgmStrct->name();
    write_txt_secSummary(os, colFmt, nm, NULL);
  }

  if (m_args.txt_summary & Analysis::Args::TxtSum_fLM) { 
    string nm = "Load module summary:";
    write_txt_secSummary(os, colFmt, nm, 
			 &ANodeTyFilter[Struct::ANode::TyLM]);
  }

  if (m_args.txt_summary & Analysis::Args::TxtSum_fFile) { 
    string nm = "File summary:";
    write_txt_secSummary(os, colFmt, nm, 
			 &ANodeTyFilter[Struct::ANode::TyFILE]);
  }

  if (m_args.txt_summary & Analysis::Args::TxtSum_fProc) { 
    string nm = "Procedure summary:";
    write_txt_secSummary(os, colFmt, nm, 
			 &ANodeTyFilter[Struct::ANode::TyPROC]);
  }

  if (m_args.txt_summary & Analysis::Args::TxtSum_fLoop) { 
    string nm = "Loop summary (dependent on structure information):";
    write_txt_secSummary(os, colFmt, nm, 
			 &ANodeTyFilter[Struct::ANode::TyLOOP]);
  }

  if (m_args.txt_summary & Analysis::Args::TxtSum_fStmt) { 
    string nm = "Statement summary:";
    write_txt_secSummary(os, colFmt, nm, 
			 &ANodeTyFilter[Struct::ANode::TySTMT]);
  }
  
  if (m_args.txt_srcAnnotation) {
    const std::vector<std::string>& fnmGlobs = m_args.txt_srcFileGlobs;
    bool hasFnmGlobs = !fnmGlobs.empty();

    Struct::ANodeIterator 
      it(m_structure.GetRoot(), &ANodeTyFilter[Struct::ANode::TyFILE]);
    for (Struct::ANode* strct = NULL; (strct = it.CurScope()); it++) {
      Struct::File* fileStrct = dynamic_cast<Struct::File*>(strct);
      const string& fnm = fileStrct->name();
      if (fnm != Struct::Tree::UnknownFileNm 
	  && (!hasFnmGlobs || FileUtil::fnmatch(fnmGlobs, fnm.c_str()))) {
	write_txt_annotateFile(os, colFmt, fileStrct);
      }
    }
  }
}


void
Driver::write_txt_secSummary(std::ostream& os, 
			     Analysis::TextUtil::ColumnFormatter& colFmt,
			     const std::string& title,
			     const Prof::Struct::ANodeFilter* filter) const
{
  using Analysis::TextUtil::ColumnFormatter;

  write_txt_hdr(os, title);

  Prof::Struct::Pgm* pgmStrct = m_structure.GetRoot();
  PerfMetric* m_sortby = m_mMgr.findSortBy();

  if (!filter) {
    // Program *sample* summary
    for (uint i = 0; i < m_mMgr.size(); ++i) {
      const PerfMetric* m = m_mMgr.metric(i);
      const FilePerfMetric* mm = dynamic_cast<const FilePerfMetric*>(m);
      if (mm) {
	const Prof::SampledMetricDesc& desc = mm->rawdesc();
	double smpl = pgmStrct->PerfData(i) / (double)desc.period();
	colFmt.genCol(i, smpl);
      }
      else {
	colFmt.genBlankCol(i);
      }
    }
    os << std::endl;

    // Program metric summary
    for (uint i = 0; i < m_mMgr.size(); ++i) {
      colFmt.genCol(i, pgmStrct->PerfData(i));
    }
    os << std::endl;
  }
  else {
    Prof::Struct::ANodeMetricSortedIterator it(pgmStrct, m_sortby->Index(), filter);
    for (; it.Current(); it++) {
      Prof::Struct::ANode* strct = it.Current();
      for (uint i = 0; i < m_mMgr.size(); ++i) {
	colFmt.genCol(i, strct->PerfData(i), pgmStrct->PerfData(i));
      }
      os << " " << strct->nameQual() << std::endl;
    } 
  }
  os << std::endl;
}


void
Driver::write_txt_annotateFile(std::ostream& os, 
			       Analysis::TextUtil::ColumnFormatter& colFmt,
			       const Prof::Struct::File* fileStrct) const
{
  const string& fnm = fileStrct->name();
  const string& fnm_qual = fileStrct->nameQual();

  string title = "Annotated file (statement/line level): " + fnm_qual;
  write_txt_hdr(os, title);
  
  std::istream* is = NULL;
  try {
    is = IOUtil::OpenIStream(fnm.c_str());
  }
  catch (const Diagnostics::Exception& x) {
    os << "  Cannot open.\n" << std::endl;
    return;
  }

  //-------------------------------------------------------
  //
  //-------------------------------------------------------

  Prof::Struct::Pgm* pgmStrct = m_structure.GetRoot();
  const uint linew = 5;
  string linetxt;
  SrcFile::ln ln_file = 1; // line number *after* next getline

  Prof::Struct::ANodeLineSortedIterator 
    it(fileStrct, &Prof::Struct::ANodeTyFilter[Prof::Struct::ANode::TySTMT]);
  for (Prof::Struct::ACodeNode* strct = NULL; (strct = it.Current()); it++) {
    SrcFile::ln ln_metric = strct->begLine();

    // Advance ln_file to just before ln_metric, if necessary
    while (ln_file < ln_metric && is->good()) {
      std::getline(*is, linetxt);
      os << std::setw(linew) << std::setfill(' ') << ln_file;
      colFmt.genBlankCols();
      os << " " << linetxt << std::endl;
      ln_file++;
    }

    // INVARIANT: isValid(ln_metric) ==> ln_metric == ln_file
    //DIAG_Assert(Logic::implies(SrcFile::isValid(ln_metric), ln_metric == ln_file, DIAG_UnexpectedInput);
    
    // Generate columns for ln_metric
    os << std::setw(linew) << std::setfill(' ') << ln_metric;
    for (uint i = 0; i < m_mMgr.size(); ++i) {
      colFmt.genCol(i, strct->PerfData(i), pgmStrct->PerfData(i));
    }

    // Generate source file line for ln_metric, if necessary
    if (SrcFile::isValid(ln_metric)) {
      std::getline(*is, linetxt);
      os << " " << linetxt;
      ln_file++;
    }
    os << std::endl;
  }
  
  // Finish generating file, if necessary
  for ( ; is->good(); ln_file++) {
    std::getline(*is, linetxt);
    os << std::setw(linew) << std::setfill(' ') << ln_file;
    colFmt.genBlankCols();
    os << " " << linetxt << std::endl;
  }

  //-------------------------------------------------------
  //
  //-------------------------------------------------------
  os << std::endl;
  IOUtil::CloseStream(is);
}


void 
Driver::write_txt_hdr(std::ostream& os, const std::string& hdr) const
{
  os << std::setfill('=') << std::setw(77) << "=" << std::endl
     << hdr << std::endl
     << std::setfill('-') << std::setw(77) << "-" << std::endl;
}


void 
Driver::write_config(std::ostream &os) const
{
  os << "<HPCVIEW>\n\n";

  // title
  os << "<TITLE name=\"" << m_args.title << "\"/>\n\n";

  // search paths
  for (uint i = 0; i < m_args.searchPathTpls.size(); ++i) { 
    const Analysis::PathTuple& x = m_args.searchPathTpls[i];
    os << "<PATH name=\"" << x.first << "\" viewname=\"" << x.second <<"\"/>\n";
  }
  if (!m_args.searchPathTpls.empty()) { os << "\n"; }
  
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
Driver::populatePgmStructure(Prof::Struct::Tree& structure)
{
  Prof::Struct::TreeInterface structIF(structure.GetRoot(), 
				       m_args.searchPathStr());
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
Driver::correlateMetricsWithStructure(Prof::Metric::Mgr& mMgr,
				      Prof::Struct::Tree& structure) 
{
  computeRawMetrics(mMgr, structure);
  
  // NOTE: Pruning the structure tree now (as opposed to after
  // computing derived metrics) can significantly speed up the
  // computation of derived metrics.  Technically, this may not be
  // correct, since a derived metric could simply add a constant to
  // every value of a raw metric, potentially creating 'unprunable'
  // nodes from those that would otherwise be prunable.  However, any
  // sane computed metric will not have this property.
  structure.GetRoot()->pruneByMetrics();

  computeDerivedMetrics(mMgr, structure);
}


//----------------------------------------------------------------------------

void
Driver::computeRawMetrics(Prof::Metric::Mgr& mMgr, Prof::Struct::Tree& structure) 
{
  Prof::Struct::TreeInterface structIF(structure.GetRoot(), 
				       m_args.searchPathStr());
  StringToBoolMap hasStructureTbl;
  
  const Prof::Metric::Mgr::StringPerfMetricVecMap& fnameToFMetricMap = 
    mMgr.fnameToFMetricMap();

  //-------------------------------------------------------
  // Insert performance data into scope tree leaves.  Batch several
  // profile files to one load module to amortize the overhead of
  // reading symbol tables and debugging information.
  //-------------------------------------------------------
  Prof::Metric::Mgr::StringPerfMetricVecMap::const_iterator it = 
    fnameToFMetricMap.begin();
  
  ProfToMetricsTupleVec batchJob;
  while (getNextRawBatch(batchJob, it, fnameToFMetricMap.end())) {

    //-------------------------------------------------------
    // FIXME: Assume that each profile file has an idential epoch.
    // This will have to change when true dlopen support is available.
    //-------------------------------------------------------

    string prev_lmname_orig;

    // For each load module: process the batch job.  A batch job
    // process a group of profile files (and their associated metrics)
    // by load module.
    Prof::Flat::ProfileData* prof = batchJob[0].first;
    for (Prof::Flat::ProfileData::const_iterator it = prof->begin(); 
	 it != prof->end(); ++it) {
      
      const string lmname_orig = it->first;
      if (lmname_orig == prev_lmname_orig) {
	// Skip multiple entries for same LM.  This is sufficient
	// b/c iteration proceeds in sorted fashion.
	continue; 
      }
      prev_lmname_orig = lmname_orig;

      const string lmname = replacePath(lmname_orig);
      
      bool useStruct = hasStructure(lmname, structIF, hasStructureTbl);
      computeRawBatchJob_LM(lmname, lmname_orig, structIF, batchJob, useStruct);
    }
    
    clearRawBatch(batchJob);
  }

  //-------------------------------------------------------
  // Accumulate leaves to interior nodes, if necessary
  //-------------------------------------------------------

  if (m_args.metrics_computeInteriorValues || mMgr.hasDerived()) {
    // 1. Compute batch jobs: all raw metrics are independent of each
    //    other and therefore may be computed en masse.
    VMAIntervalSet ivalset; // cheat using a VMAInterval set

    for (uint i = 0; i < mMgr.size(); i++) {
      const PerfMetric* m = mMgr.metric(i);
      const FilePerfMetric* mm = dynamic_cast<const FilePerfMetric*>(m);
      if (mm) {
	ivalset.insert(VMAInterval(m->Index(), m->Index() + 1)); // [ )
      }
    }
    
    // 2. Now execute the batch jobs
    for (VMAIntervalSet::iterator it = ivalset.begin(); 
	 it != ivalset.end(); ++it) {
      const VMAInterval& ival = *it;
      structIF.GetRoot()->accumulateMetrics((uint)ival.beg(), 
					    (uint)ival.end() - 1); // [ ]
    }
  }
}


void
Driver::computeRawBatchJob_LM(const string& lmname, const string& lmname_orig,
			      Prof::Struct::TreeInterface& structIF,
			      ProfToMetricsTupleVec& profToMetricsVec,
			      bool useStruct)
{
  binutils::LM* lm = openLM(lmname);
  if (!lm) {
    return;
  }

  if (!useStruct) {
    lm->read(binutils::LM::ReadFlg_Seg);
  }

  Prof::Struct::LM* lmStrct = structIF.MoveToLM(lmname);

  for (uint i = 0; i < profToMetricsVec.size(); ++i) {

    Prof::Flat::ProfileData* prof = profToMetricsVec[i].first;
    Prof::Metric::Mgr::PerfMetricVec* metrics = profToMetricsVec[i].second;

    
    using Prof::Flat::ProfileData;
    std::pair<ProfileData::iterator, ProfileData::iterator> fnd =
      prof->equal_range(lmname_orig);
    if (fnd.first == prof->end()) {
      DIAG_WMsg(1, "Cannot find LM " << lmname_orig << " within " 
		<< prof->name() << ".");
      continue;
    }

    //-------------------------------------------------------
    // For each Prof::Flat::LM that matches lmname_orig
    //-------------------------------------------------------
    for (ProfileData::iterator it = fnd.first; it != fnd.second; ++it) {
      Prof::Flat::LM* proflm = it->second;
    
      //-------------------------------------------------------
      // For each metric, insert performance data into scope tree
      //-------------------------------------------------------
      for (Prof::Metric::Mgr::PerfMetricVec::iterator it = metrics->begin();
	   it != metrics->end(); ++it) {
	FilePerfMetric* m = dynamic_cast<FilePerfMetric*>(*it);
	DIAG_Assert(m->isunit_event(), "Assumes metric should compute events!");
	uint mIdx = (uint)StrUtil::toUInt64(m->NativeName());
	
	const Prof::Flat::EventData& profevent = proflm->event(mIdx);
	m->rawdesc(profevent.mdesc());
	
	correlateRaw(m, profevent, proflm->load_addr(), 
		     structIF, lmStrct, lm, useStruct);
      }
    }
  }

  delete lm;
}


// With structure information (an object code to source structure
// map), correlation is by VMA.  Otherwise correlation is performed
// using file, function and line debugging information.
void
Driver::correlateRaw(PerfMetric* metric,
		     const Prof::Flat::EventData& profevent,
		     VMA lm_load_addr,
		     Prof::Struct::TreeInterface& structIF,
		     Prof::Struct::LM* lmStrct,
		     /*const*/ binutils::LM* lm,
		     bool useStruct)
{
  unsigned long period = profevent.mdesc().period();
  bool doUnrelocate = lm->doUnrelocate(lm_load_addr);

  for (uint i = 0; i < profevent.num_data(); ++i) {
    const Prof::Flat::Datum& dat = profevent.datum(i);
    VMA vma = dat.first; // relocated VMA
    uint32_t samples = dat.second;
    double events = samples * period; // samples * (events/sample)
    
    // 1. Unrelocate vma.
    VMA vma_ur = (doUnrelocate) ? (vma - lm_load_addr) : vma;
	
    // 2. Find associated scope and insert into scope tree
    Prof::Struct::ANode* strct = NULL;

    if (useStruct) {
      strct = lmStrct->findByVMA(vma_ur);
      if (!strct) {
	structIF.MoveToFile(Prof::Struct::Tree::UnknownFileNm);
	strct = structIF.MoveToProc(Prof::Struct::Tree::UnknownProcNm);
      }
    }
    else {
      string procnm, filenm;
      SrcFile::ln line;
      lm->GetSourceFileInfo(vma_ur, 0 /*opIdx*/, procnm, filenm, line);
      procnm = GetBestFuncName(procnm);

      if (filenm.empty()) {
	filenm = Prof::Struct::Tree::UnknownFileNm;
      }
      if (procnm.empty()) {
	procnm = Prof::Struct::Tree::UnknownProcNm;
      }
      
      structIF.MoveToFile(filenm);
      Prof::Struct::Proc* procStrct = structIF.MoveToProc(procnm);
      if (SrcFile::isValid(line)) {
	Prof::Struct::Stmt* stmtStrct = procStrct->findStmt(line);
	if (!stmtStrct) {
	  stmtStrct = new Prof::Struct::Stmt(procStrct, line, line, 0, 0);
	}
	strct = stmtStrct;
      }
      else {
	strct = procStrct;
      }
    }

    strct->SetPerfData(metric->Index(), events); // implicit add!
    DIAG_DevMsg(6, "Metric associate: " 
		<< metric->Name() << ":0x" << hex << vma_ur << dec 
		<< " --> +" << events << "=" 
		<< strct->PerfData(metric->Index()) << " :: " 
		<< strct->toXML());
  }  
}



// A batch is a vector of [Prof::Flat::Profile, <metric-vector>] pairs
bool
Driver::getNextRawBatch(ProfToMetricsTupleVec& batchJob,
			Prof::Metric::Mgr::StringPerfMetricVecMap::const_iterator& it, 
			const Prof::Metric::Mgr::StringPerfMetricVecMap::const_iterator& it_end)
{
  for (uint i = 0; i < profileBatchSz; ++i) {
    if (it != it_end) {
      const string& fnm = it->first;
      Prof::Metric::Mgr::PerfMetricVec& metrics = 
	const_cast<Prof::Metric::Mgr::PerfMetricVec&>(it->second);
      Prof::Flat::ProfileData* prof = readProf(fnm);
      batchJob.push_back(make_pair(prof, &metrics));
      it++;
    }
    else {
      break;
    }
  }

  bool haswork = !batchJob.empty();
  return haswork;
}


void
Driver::clearRawBatch(ProfToMetricsTupleVec& batchJob)
{
  for (uint i = 0; i < batchJob.size(); ++i) {
    ProfToMetricsTuple& tup = batchJob[i];
    delete tup.first;
  }
  batchJob.clear();
}


bool
Driver::hasStructure(const string& lmname, Prof::Struct::TreeInterface& structIF,
		     StringToBoolMap& hasStructureTbl)
{
  // hasStructure's test depdends on the *initial* structure information
  StringToBoolMap::iterator it = hasStructureTbl.find(lmname);
  if (it != hasStructureTbl.end()) {
    return it->second; // memoized answer
  }
  else {
    Prof::Struct::LM* lmStrct = structIF.MoveToLM(lmname);
    bool hasStruct = (lmStrct->ChildCount() > 0);
    hasStructureTbl.insert(make_pair(lmname, hasStruct));
    if (!hasStruct && !m_args.structureFiles.empty()) {
      DIAG_WMsg(2, "No STRUCTURE for " << lmname << ".");
    }
    return hasStruct;
  }
}


//----------------------------------------------------------------------------

void
Driver::computeDerivedMetrics(Prof::Metric::Mgr& mMgr, 
			      Prof::Struct::Tree& structure)
{
  using Prof::Metric::AExpr;

  // INVARIANT: All raw metrics have interior (and leaf) values before
  // derived metrics are computed.

  // 1. Compute batch jobs: a derived metric with id 'x' only depends
  //    on metrics with id's strictly less than 'x'.
  VMAIntervalSet ivalset; // cheat using a VMAInterval set
  const AExpr** mExprVec = new const AExpr*[mMgr.size()];

  for (uint i = 0; i < mMgr.size(); i++) {
    const PerfMetric* m = mMgr.metric(i);
    const ComputedPerfMetric* mm = dynamic_cast<const ComputedPerfMetric*>(m);
    if (mm) {
      ivalset.insert(VMAInterval(m->Index(), m->Index() + 1)); // [ )
      mExprVec[i] = mm->expr();
      DIAG_Assert(mExprVec[i], DIAG_UnexpectedInput);
    }
  }
  
  // 2. Now execute the batch jobs
  for (VMAIntervalSet::iterator it = ivalset.begin(); 
       it != ivalset.end(); ++it) {
    const VMAInterval& ival = *it;
    computeDerivedBatch(structure, mExprVec,
			(uint)ival.beg(), (uint)ival.end() - 1); // [ ]
  }

  delete[] mExprVec;
}


void
Driver::computeDerivedBatch(Prof::Struct::Tree& structure, 
			    const Prof::Metric::AExpr** mExprVec,
			    uint mBegId, uint mEndId)
{
  Prof::Struct::Pgm* pgmStrct = structure.GetRoot();
  Prof::Struct::ANodeIterator it(pgmStrct, NULL /*filter*/, false /*leavesOnly*/,
		       IteratorStack::PostOrder);
      
  for (; it.Current(); it++) {
    for (uint mId = mBegId; mId <= mEndId; ++mId) {
      const Prof::Metric::AExpr* expr = mExprVec[mId];
      double val = expr->eval(it.CurScope());
      // if (!Prof::Metric::AExpr::isok(val)) ...
      it.CurScope()->SetPerfData(mId, val);
    }
  }
}



//----------------------------------------------------------------------------

Prof::Flat::ProfileData*
Driver::readProf(const string& fnm)
{
  Prof::Flat::ProfileData* prof = new Prof::Flat::ProfileData(fnm.c_str());
  readProf(prof);
  return prof;
}


void
Driver::readProf(Prof::Flat::ProfileData* prof)
{
  try {
    prof->openread();
  }
  catch (...) {
    DIAG_EMsg("While reading '" << prof->name() << "'");
    throw;
  }
}


binutils::LM*
Driver::openLM(const string& fnm)
{
  binutils::LM* lm = NULL;
  try {
    lm = new binutils::LM();
    lm->open(fnm.c_str());
  }
  catch (const binutils::Exception& x) {
    DIAG_EMsg("While opening " << fnm.c_str() << ":\n" << x.message());
  }
  catch (...) {
    DIAG_EMsg("Exception encountered while opening " << fnm.c_str());
  }
  return lm;
}


} // namespace Flat

} // namespace Analysis

//****************************************************************************
