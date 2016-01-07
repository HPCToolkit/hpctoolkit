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
// Copyright ((c)) 2002-2016, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

#include <iomanip>

#include <string>
using std::string;

#include <vector>
using std::vector;

//*************************** User Include Files ****************************

#include "Flat-ObjCorrelation.hpp"
#include "TextUtil.hpp"
using Analysis::TextUtil::ColumnFormatter;

#include <lib/prof/Flat-ProfileData.hpp>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/StrUtil.hpp> 

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace Flat {


//****************************************************************************
//
//****************************************************************************

class MetricCursor {
public:
  MetricCursor(const Prof::Metric::Mgr& metricMgr,
	       const Prof::Flat::LM& proflm, 
	       const BinUtil::LM& lm);
  ~MetricCursor();
  
  const vector<const Prof::Flat::EventData*>& 
  metricDescs() const {
    return m_metricDescs; 
  }
  const vector<uint64_t>& 
  metricTots() const {
    return m_metricTots; 
  }

  // Assumptions:
  //   - vma's are unrelocated
  //   - over successsive calls, VMA ranges are ascending
  // Result is stored is metricValAtVMA()
  const vector<uint64_t>& 
  computeMetricForVMA(VMA vma) {
    return computeMetricVals(VMAInterval(vma, vma + 1), true);
  }

  // Assumptions:
  //   - same as above
  //   - vmaint: [beg_vma, end_vma), where vmaint is a region
  const vector<uint64_t>& 
  computeMetricVals(const VMAInterval vmaint, bool advanceIndices);

  const vector<uint64_t>& 
  metricValAtVMA() const { return m_metricValAtVMA; }

  static bool 
  hasNonZeroMetricVal(const vector<uint64_t>& metricVal) {
    return hasMetricValGE(metricVal, 1);
  }

  static bool 
  hasMetricValGE(const vector<uint64_t>& metricVal, uint64_t val);

private:
  VMA unrelocate(VMA vma) const {
    VMA ur_vma = (m_doUnrelocate) ? (vma - m_loadAddr) : vma;
    return ur_vma;
  }


private:
  bool m_doUnrelocate;
  VMA m_loadAddr;

  vector<const Prof::Flat::EventData*> m_metricDescs;
  vector<uint64_t> m_metricTots;

  vector<uint> m_curMetricIdx;
  vector<uint64_t> m_metricValAtVMA;
};


MetricCursor::MetricCursor(const Prof::Metric::Mgr& metricMgr,
			   const Prof::Flat::LM& proflm, 
			   const BinUtil::LM& lm)
{
  m_loadAddr = (VMA)proflm.load_addr();
  m_doUnrelocate = lm.doUnrelocate(m_loadAddr);
  
  // --------------------------------------------------------
  // Find all metrics for load module and compute totals for each metric
  // For now we have one metric per sampled event.
  // --------------------------------------------------------

  // NOTE: only handles raw events

  for (uint i = 0; i < metricMgr.size(); ++i) {
    const Prof::Metric::ADesc* m = metricMgr.metric(i);
    const Prof::Metric::SampledDesc* mm =
      dynamic_cast<const Prof::Metric::SampledDesc*>(m);
    if (mm) {
      uint mIdx = (uint)StrUtil::toUInt64(mm->profileRelId());
      const Prof::Flat::EventData& profevent = proflm.event(mIdx);
      m_metricDescs.push_back(&profevent);
    }
  }

  m_metricTots.resize(m_metricDescs.size());
  for (uint i = 0; i < m_metricDescs.size(); ++i) {
    const Prof::Flat::EventData& profevent = *(m_metricDescs[i]);
    uint64_t& metricTotal = m_metricTots[i];
    metricTotal = 0;

    for (uint j = 0; j < profevent.num_data(); ++j) {
      const Prof::Flat::Datum& evdat = profevent.datum(j);
      uint32_t count = evdat.second;
      metricTotal += count;
    }
  }

  m_curMetricIdx.resize(m_metricDescs.size());
  for (uint i = 0; i < m_curMetricIdx.size(); ++i) {
    m_curMetricIdx[i] = 0;
  }

  m_metricValAtVMA.resize(m_metricDescs.size());
}


MetricCursor::~MetricCursor()
{
}


const vector<uint64_t>&
MetricCursor::computeMetricVals(const VMAInterval vmaint,
				bool advanceCounters)
{
  // NOTE: An instruction may overlap multiple buckets.  However,
  // because only the bucket corresponding to the beginning of the
  // instruction is charged, we only have to consult one bucket.
  // However, it may be the case that a bucket contains results for
  // more than one instruction.

  for (uint i = 0; i < m_metricValAtVMA.size(); ++i) {
    m_metricValAtVMA[i] = 0;
  }

  // For each event, determine if a count exists at vma_beg
  for (uint i = 0; i < m_metricDescs.size(); ++i) {
    const Prof::Flat::EventData& profevent = *(m_metricDescs[i]);
    
    // advance ith bucket until it arrives at vmaint region:
    //   (bucket overlaps beg_vma) || (bucket is beyond beg_vma)
    for (uint& j = m_curMetricIdx[i]; j < profevent.num_data(); ++j) {
      const Prof::Flat::Datum& evdat = profevent.datum(j);
      VMA ev_vma = evdat.first;
      VMA ev_ur_vma = unrelocate(ev_vma);
      VMA ev_ur_vma_ub = ev_ur_vma + profevent.bucket_size();
      
      if ((ev_ur_vma <= vmaint.beg() && vmaint.beg() < ev_ur_vma_ub) 
	  || (ev_ur_vma > vmaint.beg())) {
	break;
      }
    }
    
    // count until bucket moves beyond vmaint region
    // INVARIANT: upon entry, bucket overlaps or is beyond region
    for (uint j = m_curMetricIdx[i]; j < profevent.num_data(); ++j) {
      const Prof::Flat::Datum& evdat = profevent.datum(j);
      VMA ev_vma = evdat.first;
      VMA ev_ur_vma = unrelocate(ev_vma);
      VMA ev_ur_vma_ub = ev_ur_vma + profevent.bucket_size();

      if (ev_ur_vma >= vmaint.end()) {
	// bucket is now beyond region
	break;
      }
      else {
	// bucket overlaps region (by INVARIANT)
	uint32_t count = evdat.second;
	m_metricValAtVMA[i] += count;
	
	// if the vmaint region extends beyond the current bucket,
	// advance bucket
	if (advanceCounters && (vmaint.end() > ev_ur_vma_ub)) {
	  m_curMetricIdx[i]++;
	}
      }
    }
  }

  return m_metricValAtVMA;
}


bool 
MetricCursor::hasMetricValGE(const vector<uint64_t>& metricVal, uint64_t val)
{
  for (uint i = 0; i < metricVal.size(); ++i) {
    if (metricVal[i] >= val) {
      return true;
    }
  }
  return false;
}


//****************************************************************************
//
//****************************************************************************

static void
writeMetricVals(ColumnFormatter& colFmt,
		const vector<uint64_t>& metricVal,
		const vector<uint64_t>& metricTot,
		ColumnFormatter::Flag flg = ColumnFormatter::Flag_NULL);

static void
correlateWithObject_LM(const Prof::Metric::Mgr& metricMgr,
		       const Prof::Flat::LM& proflm, 
		       const BinUtil::LM& lm,
		       // ----------------------------------------------
		       std::ostream& os, 
		       bool srcCode,
		       const std::vector<std::string>& procPruneGlobs,
		       uint64_t procPruneThreshold);


void
correlateWithObject(const Prof::Metric::Mgr& metricMgr,
		    // ----------------------------------------------
		    std::ostream& os, 
		    bool srcCode,
		    const std::vector<std::string>& procPruneGlobs,
		    uint64_t procPruneThreshold)
{
  using Prof::Metric::Mgr;

  const Mgr::StringToADescVecMap& fnameToFMetricMap = 
    metricMgr.fnameToFMetricMap();
  DIAG_Assert(fnameToFMetricMap.size() == 1, DIAG_UnexpectedInput);

  const string& profileFile = fnameToFMetricMap.begin()->first;

  Prof::Flat::ProfileData prof;
  try {
    prof.openread(profileFile.c_str());
  }
  catch (...) {
    DIAG_EMsg("While reading '" << profileFile << "'");
    throw;
  }

  // --------------------------------------------------------
  // For each load module, dump metrics and object code instructions
  // --------------------------------------------------------
  for (Prof::Flat::ProfileData::const_iterator it = prof.begin();
       it != prof.end(); ++it) {
    const Prof::Flat::LM* proflm = it->second;
    
    // 1. Open and read the load module
    BinUtil::LM* lm = new BinUtil::LM();
    try {
      lm->open(proflm->name().c_str());
      lm->read(BinUtil::LM::ReadFlg_ALL);
    } 
    catch (...) {
      DIAG_EMsg("While reading " << proflm->name());
      throw;
    }
    
    correlateWithObject_LM(metricMgr, *proflm, *lm, 
			   os, srcCode, procPruneGlobs, procPruneThreshold);
    delete lm;
  }
}


void
correlateWithObject_LM(const Prof::Metric::Mgr& metricMgr, 
		       const Prof::Flat::LM& proflm, 
		       const BinUtil::LM& lm,
		       // ----------------------------------------------
		       ostream& os, 
		       bool srcCode,
		       const std::vector<std::string>& procPruneGlobs,
		       uint64_t procPruneThreshold)
{
  // INVARIANT: metricMgr only contains metrics related to 'proflm'
  
  MetricCursor metricCursor(metricMgr, proflm, lm);
  ColumnFormatter colFmt(metricMgr, os, 2, 0);

  bool hasProcGlobs = !procPruneGlobs.empty();

  // --------------------------------------------------------
  // 0. Metric summary for load module
  // --------------------------------------------------------

  os << std::setfill('=') << std::setw(77) << "=" << std::endl
     << "Load module: " << proflm.name() << std::endl
     << std::setfill('-') << std::setw(77) << "-" << std::endl;

  const vector<uint64_t>& metricTots = metricCursor.metricTots();
  
  os << std::endl;
  colFmt.genColHeaderSummary();
  os << std::endl
     << "Metric summary for load module (totals):\n"
     << "  ";
  writeMetricVals(colFmt, metricTots, metricTots, 
		  ColumnFormatter::Flag_ForceVal);

  // --------------------------------------------------------
  // 1. For each procedure in the load module
  // --------------------------------------------------------
  
  for (BinUtil::LM::ProcMap::const_iterator it = lm.procs().begin();
       it != lm.procs().end(); ++it) {
    const BinUtil::Proc* p = it->second;

    // obtain name (and prune if necessary)
    string bestName = BinUtil::canonicalizeProcName(p->name());
    if (hasProcGlobs && !FileUtil::fnmatch(procPruneGlobs, bestName.c_str())) {
      continue;
    }
     
    BinUtil::Insn* endInsn = p->endInsn();
    VMAInterval procint(p->begVMA(), p->endVMA() + endInsn->size());

    // obtain counts (and prune if necessary)
    const vector<uint64_t> metricTotsProc = 
      metricCursor.computeMetricVals(procint, false);
    if (!metricCursor.hasMetricValGE(metricTotsProc, procPruneThreshold)) {
      continue;
    }

    // --------------------------------------------------------
    // Metric summary for procedure
    // --------------------------------------------------------

    os << std::endl << std::endl
       << "Procedure: " << p->name() << " (" << bestName << ")\n"
       << std::setfill('-') << std::setw(60) << "-" << std::endl;

    os << std::endl;
    colFmt.genColHeaderSummary();
    os << std::endl
       << "Metric summary for procedure (percents relative to load module):\n"
       << "  ";
    writeMetricVals(colFmt, metricTotsProc, metricTots, 
		    ColumnFormatter::Flag_ForceVal);
    os << std::endl << "  ";
    writeMetricVals(colFmt, metricTotsProc, metricTots,
		    ColumnFormatter::Flag_ForcePct);
    os << std::endl;

    // --------------------------------------------------------
    // Metric summary for instructions
    // --------------------------------------------------------
    os << std::endl
       << "Metric details for procedure (percents relative to procedure):\n";

    string the_file;
    SrcFile::ln the_line = SrcFile::ln_NULL;

    for (BinUtil::ProcInsnIterator it1(*p); it1.isValid(); ++it1) {
      BinUtil::Insn* insn = it1.current();
      VMA vma = insn->vma();
      VMA opVMA = BinUtil::LM::isa->convertVMAToOpVMA(vma, insn->opIndex());

      // 1. Collect metric annotations
      const vector<uint64_t>& metricValVMA = 
	metricCursor.computeMetricForVMA(opVMA);

      // 2. Print source line information (if necessary)
      if (srcCode) {
	string func, file;
	SrcFile::ln line;
	p->findSrcCodeInfo(vma, insn->opIndex(), func, file, line);
	
	if (file != the_file || line != the_line) {
	  the_file = file;
	  the_line = line;
	  os << the_file << ":" << the_line << std::endl;
	}
      }
	
      // 3. Print annotated instruction
      os << std::hex << opVMA << std::dec << ": ";

      if (metricCursor.hasNonZeroMetricVal(metricValVMA)) {
	writeMetricVals(colFmt, metricValVMA, metricTotsProc);
      }
      else {
	colFmt.genBlankCols();
      }

      insn->decode(os);
      os << std::endl;
    }
  }

  os << std::endl << std::endl;
}


static void
writeMetricVals(ColumnFormatter& colFmt,
		const vector<uint64_t>& metricVal,
		const vector<uint64_t>& metricTot,
		ColumnFormatter::Flag flg)
{
  for (uint i = 0; i < metricVal.size(); ++i) {
    colFmt.genCol(i, (double)metricVal[i], (double)metricTot[i], flg);
  }
}


} // namespace Flat

} // namespace Analysis
