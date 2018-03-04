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
#include "cfg/CFGNode.hpp"
#include "cfg/BinaryAnalyzer.hpp"
#include "CCTVisitor.hpp"

namespace TraceAnalysis {
  bool analysis(Prof::CallPath::Profile* prof, int myRank, int numRanks) {
    if (myRank == 0) {
      BinaryAnalyzer ba;
      bool flag = true;
      
      std::cout << std::endl << "Trace analysis turned on" << std::endl;
      
      // Step 1: analyze binary files to get CFGs for later analysis
      const Prof::LoadMap* loadmap = prof->loadmap();
      for (Prof::LoadMap::LMId_t i = Prof::LoadMap::LMId_NULL;
           i <= loadmap->size(); ++i) {
        Prof::LoadMap::LM* lm = loadmap->lm(i);
        if (lm->isUsed() && lm->id() != Prof::LoadMap::LMId_NULL) {
          std::cout << "executable: " << lm->name() << std::endl;
          if (flag) {
            ba.parse(lm->name());
            flag = false;
          }
        }
      }
      
      // Step 2: visit CCT to build an cpid to CCTNode map.
      CCTVisitor cctVisitor(prof->cct());
      const unordered_map<uint, Prof::CCT::ADynNode*>& cpidMap = cctVisitor.getCpidMap();
      for (auto mit = cpidMap.begin(); mit != cpidMap.end(); ++mit)
        std::cout << "0x" << std::hex << mit->first << ", ";
      std::cout << std::endl;
    }
    return true;
  }
}