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

#include <cmath> // pow

//*************************** User Include Files ****************************

#include "Flat_ObjCorrelation.hpp"

#include <lib/prof-juicy/FlatProfileReader.hpp>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace Flat {


//****************************************************************************
//
//****************************************************************************

class MetricCursor {
public:
  MetricCursor(const Prof::Flat::LM& proflm, const binutils::LM& lm);
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
  hasNonZeroMetricVal(const vector<uint64_t>& metricCnt) {
    return hasMetricValGE(metricCnt, 1);
  }

  static bool 
  hasMetricValGE(const vector<uint64_t>& metricCnt, uint64_t val);

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


MetricCursor::MetricCursor(const Prof::Flat::LM& proflm, const binutils::LM& lm)
{
  m_loadAddr = (VMA)proflm.load_addr();
  m_doUnrelocate = lm.doUnrelocate(m_loadAddr);
  
  // --------------------------------------------------------
  // Find all metrics for load module and compute totals for each metric
  // For now we have one metric per sampled event.
  // --------------------------------------------------------
  for (uint i = 0; i < proflm.num_events(); ++i) {
    const Prof::Flat::EventData& profevent = proflm.event(i);
    m_metricDescs.push_back(&profevent);
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
MetricCursor::hasMetricValGE(const vector<uint64_t>& metricCnt, uint64_t val)
{
  for (uint i = 0; i < metricCnt.size(); ++i) {
    if (metricCnt[i] >= val) {
      return true;
    }
  }
  return false;
}


//****************************************************************************
//
//****************************************************************************

class ColumnFormatter {
public:
  ColumnFormatter(ostream& os, int num_metrics, int num_digits, 
		  bool showAsPercent)
    : m_os(os),
      m_num_metrics(num_metrics), m_num_digits(num_digits),
      m_showAsPercent(showAsPercent),
      m_metricAnnotationWidth(0), 
      m_metricAnnotationWidthTot(0),
      m_scientificFormatThreshold(0) 
  {
    os.setf(std::ios_base::fmtflags(0), std::ios_base::floatfield);
    os << std::right << std::noshowpos;

    // Compute annotation width
    if (m_showAsPercent) {
      if (m_num_digits >= 1) {
	// xxx.(yy)% or x.xE-yy% (for small numbers)
	m_metricAnnotationWidth = std::max(8, 5 + m_num_digits);
	m_scientificFormatThreshold = 
	  std::pow((double)10, -(double)m_num_digits);
      }
      else {
	// xxx%
	m_metricAnnotationWidth = 4;
      }
      os << std::showpoint << std::scientific;
    }
    else {
      // x.xE+yy (for large numbers)
      m_metricAnnotationWidth = 7;
      
      // for floating point numbers over the scientificFormatThreshold
      // printed in scientific format.
      m_scientificFormatThreshold = 
	std::pow((double)10, (double)m_metricAnnotationWidth);
      os << std::setprecision(m_metricAnnotationWidth - 6);
      os << std::scientific;
    }
    os << std::showbase;
    
    m_metricAnnotationWidthTot = ((m_num_metrics * m_metricAnnotationWidth) 
				 + m_num_metrics);
  }

  ~ColumnFormatter() { }

  void 
  metric_col(uint64_t metricCnt, uint64_t metricTot);

  void
  fill_cols() {
    m_os << std::setw(m_metricAnnotationWidthTot) << std::setfill(' ') << " ";
  }

private:
  ostream& m_os;
  int    m_num_metrics;
  int    m_num_digits;
  bool   m_showAsPercent;
  int    m_metricAnnotationWidth;
  int    m_metricAnnotationWidthTot;
  double m_scientificFormatThreshold;
};


void
ColumnFormatter::metric_col(uint64_t metricCnt, uint64_t metricTot)
{
  if (m_showAsPercent) {
    double val = 0.0;
    if (metricTot != 0) {
      val = ((double)metricCnt / (double)metricTot) * 100;
    }
    
    m_os << std::setw(m_metricAnnotationWidth - 1);
    if (val != 0.0 && val < m_scientificFormatThreshold) {
      m_os << std::scientific 
	   << std::setprecision(m_metricAnnotationWidth - 7);
    }
    else {
      //m_os.unsetf(ios_base::scientific);
      m_os << std::fixed
	   << std::setprecision(m_num_digits);
    }
    m_os << std::setfill(' ') << val << "%";
  }
  else {
    m_os << std::setw(m_metricAnnotationWidth);
    if ((double)metricCnt >= m_scientificFormatThreshold) {
      m_os << (double)metricCnt;
    }
    else {
      m_os << std::setfill(' ') << metricCnt;
    }
  }
  m_os << " ";
}


//****************************************************************************
//
//****************************************************************************

static void
writeMetricSummary(std::ostream& os, 
		   const vector<const Prof::Flat::EventData*>& metricDescs,
		   const vector<uint64_t>& metricCnt,
		   const vector<uint64_t>* metricTot);

static void
correlateWithObject_LM(ostream& os, 
		       const Prof::Flat::LM& proflm, const binutils::LM& lm,
		       bool srcCode,
		       bool metricsAsPercent,
		       uint64_t procVisThreshold);


void
correlateWithObject(std::ostream& os, const string& profileFile,
		    bool srcCode,
		    bool metricsAsPercent,
		    uint64_t procVisThreshold)
{
  Prof::Flat::Profile prof;
  try {
    prof.read(profileFile.c_str());
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
    binutils::LM* lm = NULL;
    try {
      lm = new binutils::LM();
      lm->open(proflm->name().c_str());
      lm->read();
    } 
    catch (...) {
      DIAG_EMsg("While reading " << proflm->name());
      throw;
    }
    
    correlateWithObject_LM(os, *proflm, *lm, 
			   srcCode, metricsAsPercent, procVisThreshold);
    delete lm;
  }
}


void
correlateWithObject_LM(ostream& os, 
		       const Prof::Flat::LM& proflm, const binutils::LM& lm,
		       bool srcCode,
		       bool metricsAsPercent,
		       uint64_t procVisThreshold)
{
  MetricCursor metricCursor(proflm, lm);
  ColumnFormatter colFmt(os, metricCursor.metricDescs().size(), 2, 
			 metricsAsPercent);

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
	  uint64_t metricCnt = metricValVMA[i];
	  colFmt.metric_col(metricCnt, metricTotsProc[i]);
	}
      }
      else {
	colFmt.fill_cols();
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
		   const vector<uint64_t>& metricCnt,
		   const vector<uint64_t>* metricTot)
{
  os << std::endl 
     << "Columns correspond to the following metrics [event:period (events/sample)]\n";

  for (uint i = 0; i < metricDescs.size(); ++i) {
    const Prof::Flat::EventData& profevent = *(metricDescs[i]);
    
    const Prof::SampledMetricDesc& mdesc = profevent.mdesc();
    os << "  " << mdesc.name() << ":" << mdesc.period() 
       << " - " << mdesc.description() 
       << " (" << metricCnt[i] << " samples";
    
    if (metricTot) {
      double pct = ((double)metricCnt[i] / (double)(*metricTot)[i]) * 100;
      os << " - " << std::fixed << std::setprecision(4) 
	 << pct << "%";
    }
    
    os << ")" << std::endl;
  }
}



} // namespace Flat

} // namespace Analysis
