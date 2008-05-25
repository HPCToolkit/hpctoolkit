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

//***************************************************************************
//
// File:
//   $Source$
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

#include "Flat_ObjCorrelation.hpp"
#include "TextUtil.hpp"

#include <lib/prof-juicy/FlatProfileReader.hpp>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
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
  MetricCursor(const Prof::MetricDescMgr& metricMgr,
	       const Prof::Flat::LM& proflm, 
	       const binutils::LM& lm);
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

  vector<int> m_curMetricIdx;
  vector<uint64_t> m_metricValAtVMA;
};


MetricCursor::MetricCursor(const Prof::MetricDescMgr& metricMgr,
			   const Prof::Flat::LM& proflm, 
			   const binutils::LM& lm)
{
  m_loadAddr = (VMA)proflm.load_addr();
  m_doUnrelocate = lm.doUnrelocate(m_loadAddr);
  
  // --------------------------------------------------------
  // Find all metrics for load module and compute totals for each metric
  // For now we have one metric per sampled event.
  // --------------------------------------------------------

  // NOTE: only handles raw events

  for (uint i = 0; i < metricMgr.size(); ++i) {
    const PerfMetric* m = metricMgr.metric(i);
    const FilePerfMetric* mm = dynamic_cast<const FilePerfMetric*>(m);
    if (mm) {
      uint mIdx = (uint)StrUtil::toUInt64(mm->NativeName());
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
    for (int& j = m_curMetricIdx[i]; j < profevent.num_data(); ++j) {
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
    for (int j = m_curMetricIdx[i]; j < profevent.num_data(); ++j) {
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
writeMetricSummary(std::ostream& os, 
		   const vector<const Prof::Flat::EventData*>& metricDescs,
		   const vector<uint64_t>& metricVal,
		   const vector<uint64_t>* metricTot);

static void
correlateWithObject_LM(const Prof::MetricDescMgr& metricMgr,
		       const Prof::Flat::LM& proflm, 
		       const binutils::LM& lm,
		       // ----------------------------------------------
		       std::ostream& os, 
		       bool srcCode,
		       uint64_t procVisThreshold);


void
correlateWithObject(const Prof::MetricDescMgr& metricMgr,
		    // ----------------------------------------------
		    std::ostream& os, 
		    bool srcCode,
		    uint64_t procVisThreshold)
{
  using Prof::MetricDescMgr;

  const MetricDescMgr::StringPerfMetricVecMap& fnameToFMetricMap = 
    metricMgr.fnameToFMetricMap();
  DIAG_Assert(fnameToFMetricMap.size() == 1, DIAG_UnexpectedInput);

  const string& profileFile = fnameToFMetricMap.begin()->first;

  Prof::Flat::Profile prof;
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
  for (Prof::Flat::Profile::const_iterator it = prof.begin(); 
       it != prof.end(); ++it) {
    const Prof::Flat::LM* proflm = it->second;
    
    // 1. Open and read the load module
    binutils::LM* lm = new binutils::LM();
    try {
      lm->open(proflm->name().c_str());
      lm->read(binutils::LM::ReadFlg_ALL);
    } 
    catch (...) {
      DIAG_EMsg("While reading " << proflm->name());
      throw;
    }
    
    correlateWithObject_LM(metricMgr, *proflm, *lm, 
			   os, srcCode, procVisThreshold);
    delete lm;
  }
}


void
correlateWithObject_LM(const Prof::MetricDescMgr& metricMgr, 
		       const Prof::Flat::LM& proflm, 
		       const binutils::LM& lm,
		       // ----------------------------------------------
		       ostream& os, 
		       bool srcCode,
		       uint64_t procVisThreshold)
{
  // INVARIANT: metricMgr only contains metrics related to 'proflm'
  
  using Analysis::TextUtil::ColumnFormatter;

  MetricCursor metricCursor(metricMgr, proflm, lm);
  ColumnFormatter colFmt(metricMgr, os, 2);

  // --------------------------------------------------------
  // 0. Print metric list
  // --------------------------------------------------------

  os << std::setfill('=') << std::setw(77) << "=" << std::endl;
  os << "Load module: " << proflm.name() << std::endl;

  const vector<const Prof::Flat::EventData*>& metricDescs = 
    metricCursor.metricDescs();
  const vector<uint64_t>& metricTots = metricCursor.metricTots();
  
  writeMetricSummary(os, metricDescs, metricTots, NULL);

  // --------------------------------------------------------
  // 1. Print annotated load module
  // --------------------------------------------------------
  
  for (binutils::LM::ProcMap::const_iterator it = lm.procs().begin();
       it != lm.procs().end(); ++it) {
    const binutils::Proc* p = it->second;
    string bestName = GetBestFuncName(p->name());
      
    binutils::Insn* endInsn = p->lastInsn();
    VMAInterval procint(p->begVMA(), p->endVMA() + endInsn->size());
      
    const vector<uint64_t> metricTotsProc = 
      metricCursor.computeMetricVals(procint, false);
    if (!metricCursor.hasMetricValGE(metricTotsProc, procVisThreshold)) {
      continue;
    }

    // We have a 'Procedure'.  Iterate over instructions
    os << std::endl << std::endl
       << "Procedure: " << p->name() << " (" << bestName << ")\n";

    writeMetricSummary(os, metricDescs, metricTotsProc, &metricTots);
    os << std::endl << std::endl;
      
    string the_file;
    SrcFile::ln the_line = SrcFile::ln_NULL;

    for (binutils::ProcInsnIterator it(*p); it.IsValid(); ++it) {
      binutils::Insn* insn = it.Current();
      VMA vma = insn->vma();
      VMA opVMA = binutils::LM::isa->ConvertVMAToOpVMA(vma, insn->opIndex());

      // 1. Collect metric annotations
      const vector<uint64_t>& metricValVMA = 
	metricCursor.computeMetricForVMA(opVMA);

      // 2. Print line information (if necessary)
      if (srcCode) {
	string func, file;
	SrcFile::ln line;
	p->GetSourceFileInfo(vma, insn->opIndex(), func, file, line);
	
	if (file != the_file || line != the_line) {
	  the_file = file;
	  the_line = line;
	  os << the_file << ":" << the_line << std::endl;
	}
      }
	
      // 3. Print annotated instruction
      os << std::hex << opVMA << std::dec << ": ";
	
      if (metricCursor.hasNonZeroMetricVal(metricValVMA)) {
	for (uint i = 0; i < metricValVMA.size(); ++i) {
	  uint64_t metricVal = metricValVMA[i];
	  colFmt.genCol(i, metricVal, metricTotsProc[i]);
	}
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


void
writeMetricSummary(std::ostream& os, 
		   const vector<const Prof::Flat::EventData*>& metricDescs,
		   const vector<uint64_t>& metricVal,
		   const vector<uint64_t>* metricTot)
{
  os << std::endl 
     << "Columns correspond to the following metrics [event:period (events/sample)]\n";

  for (uint i = 0; i < metricDescs.size(); ++i) {
    const Prof::Flat::EventData& profevent = *(metricDescs[i]);
    
    const Prof::SampledMetricDesc& mdesc = profevent.mdesc();
    os << "  " << mdesc.name() << ":" << mdesc.period() 
       << " - " << mdesc.description() 
       << " (" << metricVal[i] << " samples";
    
    if (metricTot) {
      double pct = ((double)metricVal[i] / (double)(*metricTot)[i]) * 100;
      os << " - " << std::fixed << std::setprecision(4) 
	 << pct << "%";
    }
    
    os << ")" << std::endl;
  }
}



} // namespace Flat

} // namespace Analysis
