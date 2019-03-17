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
// Copyright ((c)) 2002-2017, Rice University
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

/* 
 * File:   TraceAnalysis.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on February 28, 2018, 10:59 PM
 */

#include "TraceAnalysis.hpp"
#include "LocalTraceAnalyzer.hpp"
#include "TraceAnalysisCommon.hpp"
#include "DifferenceQuantifier.hpp"
#include "PerformanceDiagnoser.hpp"
#include "ClockSynchronizer.hpp"

#include <sys/time.h>

#include <vector>
using std::vector;

namespace TraceAnalysis {
  bool analysis(Prof::CallPath::Profile* prof, string dbDir) {
    {
      struct timeval time;
      gettimeofday(&time, NULL);
      setStartTime(time.tv_sec * 1000000 + time.tv_usec);
      print_msg(MSG_PRIO_MAX, "Trace analysis started at 0.000s.\n\n");
    }
    
    LocalTraceAnalyzer analyzer;
    vector<TCTRootNode*> rootNodes;
    TCTClusterNode* rootCluster = analyzer.analyze(prof, dbDir, 0, 1, rootNodes);
    
    {
      struct timeval time;
      gettimeofday(&time, NULL);
      long timeDiff = time.tv_sec * 1000000 + time.tv_usec - getStartTime();
      print_msg(MSG_PRIO_MAX, "\nTrace analysis finished at %s.\n", timeToString(timeDiff).c_str());
      
      rootCluster->computeAvgRep();
      print_msg(MSG_PRIO_NORMAL, "\nAvg Rep: %s", rootCluster->getAvgRep()->toString(20, 0, 10000).c_str());
      print_msg(MSG_PRIO_NORMAL, "\nRoot Cluster: %s", rootCluster->toString(20, 0, 1000).c_str());
      
      PerformanceDiagnoser diagnoser;
      diagnoser.generateDiagnosis((TCTRootNode*)rootCluster->getAvgRep(), dbDir);
      
      ClockSynchronizer clocksync((TCTRootNode*)rootCluster->getAvgRep());
      for (uint procID = 1; procID < rootNodes.size(); procID++)
        print_msg(MSG_PRIO_NORMAL, "\n Clock diff between proc #0 and proc #%d is %ld.\n", procID, clocksync.getClockDifference(rootNodes[0], rootNodes[procID]));
    }
    
    for (TCTRootNode* root : rootNodes)
      delete root;
    delete rootCluster;
    return true;
  }
}