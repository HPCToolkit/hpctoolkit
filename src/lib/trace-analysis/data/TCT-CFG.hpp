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
 * File:   TCT-CFG.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 2, 2018, 1:16 AM
 * 
 * Stores control flows among call sites and loops.
 */

#ifndef TCT_CFG_HPP
#define TCT_CFG_HPP

#include <string>
using std::string;

#include <unordered_map>
using std::unordered_map;

#include <unordered_set>
using std::unordered_set;

#include "../TraceAnalysisCommon.hpp"

namespace TraceAnalysis {
  class BinaryAnalyzer;
  
  // For each function or loop, stores control flows among call sites and sub-loops.
  class CFGAGraph {
    friend class BinaryAnalyzer;
    
  public:
    // VMA for the function or loop
    const VMA vma;
    const string label;

  private:  
    // Keys in the map are RAs of call sites and VMAs of sub-loops contained in the function or loop
    // Value set for each key are a set of addresses that are successors of the key in the control flow.
    unordered_map<VMA, unordered_set<VMA>> successorMap;

  public:
    CFGAGraph(VMA vma, string label) : vma(vma), label(label) {}
    virtual ~CFGAGraph() {}
    
    virtual void setSuccessorMap(const unordered_map<VMA, unordered_set<VMA>>& map) {
      successorMap = map;
    }
    
    virtual bool hasChild(VMA addr) {
      return successorMap.find(addr) != successorMap.end();
    }

    // Return -1 if addr2 is a successor of addr1; 1 if addr1 is a successor of addr2;
    // 0 when there is no clear relationship between addr1 and addr2, insluding the case that 
    // addr1 or addr2 is not a child of this CFG.
    virtual int compareChild(VMA addr1, VMA addr2) {
      if (!hasChild(addr1)) return 0;
      if (!hasChild(addr2)) return 0;
      
      if (successorMap[addr1].find(addr2) != successorMap[addr1].end()) return -1;
      if (successorMap[addr2].find(addr1) != successorMap[addr2].end()) return 1;
      return 0;
    }
    
    virtual string toString() = 0;
    
    virtual string toDetailedString() {
      string str = toString() + ":\n";
      for (auto mit = successorMap.begin(); mit != successorMap.end(); ++mit) {
        str += "  0x" + vmaToHexString(mit->first) + " -> [";
        for (auto sit = (mit->second).begin(); sit != (mit->second).end(); ++sit)
          str += "0x" + vmaToHexString(*sit) + ",";
        str += "]\n";
      }
      str +="\n";
      return str;
    }
  };
  
  class CFGLoop : public CFGAGraph {
  public:
    CFGLoop(VMA vma) : CFGAGraph(vma, "loop@0x" + vmaToHexString(vma)) {}
    virtual ~CFGLoop() {}
    
    virtual string toString() {
      return "loop @ 0x" + vmaToHexString(vma);
    }
  };
  
  class CFGFunc : public CFGAGraph {
  public:
    CFGFunc(VMA vma, string label) : CFGAGraph(vma, label) {}
    virtual ~CFGFunc() {}
    
    virtual string toString() {
      return "func " + label + " @ 0x" + vmaToHexString(vma);
    }
  };
}

#endif /* TCT_CFG_HPP */

