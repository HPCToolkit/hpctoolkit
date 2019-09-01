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
// Copyright ((c)) 2002-2019, Rice University
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

#ifndef __VARMAP_METRIC_HPP__
#define __VARMAP_METRIC_HPP__

#include <vector>

#include <lib/support/BaseVarMap.hpp>   // basic var map class

#include <lib/prof-lean/hpcrun-fmt.h>   // metric stuffs

#include <lib/prof/Metric-Mgr.hpp>      // Metric manager class
#include <lib/prof/CCT-Tree.hpp>      // Metric manager class


#define VAR_PREFIX '$'


/********************
 * Special mapping variable class for hpcrun derived metrics
 *
 * computing hpcrun's derived metrics
 * hpcrun can specify a formula to compute a metric
 * hpcprof should compute the formula AFTER creating derived incremental metrics
 *  and computing the inclusive and exclusive metrics.
 *
 *  Caveat: metric index in hpcrun is different than in hpcprof.
 *  in hpcrun, all metrics are exclusive, and ranged from 0 to num_metrics
 *  while in hpcprof, a metric comprises of inclusive and exclusive, plus statistics
 *    and some other helper metrics. So the number of metrics can be 20x than
 *    the number in hpcrun.
 *
 *    To solve this, we need to adjust the metric index during formula evaluation.
 ********************/
class VarMapMetric : public BaseVarMap
{
private:
  const Prof::Metric::Mgr* m_metricMgr;
  const Prof::CCT::ANode*  m_node;
  uint                     m_metricBegId;

  std::vector<hpcrun_metricVal_t> *m_metricVec;

  int m_error_code;
  int m_num_metric_groups;

public:

  VarMapMetric(const Prof::Metric::Mgr* metricMgr, uint metricBegId) {

    m_metricMgr   = metricMgr;
    m_node        = NULL;
    m_metricBegId = metricBegId;
    m_metricVec   = NULL;

    m_error_code  = 0;
    if (metricBegId == 0) {
      m_num_metric_groups = 1;
    } else {
      m_num_metric_groups = metricMgr->numMetricsPerGroup();
    }
  }

  ~VarMapMetric() {

  }
  /****
   * get the metric value either from the node or from the table of metrics
   */
  double getValue(unsigned int var) {
    double value = (double) var;

    // here, a metric comprises of inclusive and exclusive
    // the index becomes index * 2
    unsigned int metric_index = m_metricBegId + var * m_num_metric_groups;

    if (var < 0 || metric_index >= m_metricMgr->size()) return value;

    if (m_metricVec != NULL )
      return (*m_metricVec)[metric_index].r;

    if (m_node != NULL)
      return m_node->metric(metric_index);

    return value;
  }


  bool isVariable(char *expr) {
    return (expr != NULL && expr[0] == VAR_PREFIX);
  }

  int getErrorCode() {
    return m_error_code;
  }

  void
  setNode(Prof::CCT::ANode* node) {
    m_node = node;
  }

  void
  setMetrics(std::vector<hpcrun_metricVal_t> *metricVec) {
    m_metricVec = metricVec;
  }

};

#endif
