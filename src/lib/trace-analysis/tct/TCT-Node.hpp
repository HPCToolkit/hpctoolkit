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
 * File:   TCT-Node.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 4, 2018, 11:22 PM
 */

#ifndef TCT_NODE_HPP
#define TCT_NODE_HPP

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "../TraceAnalysisCommon.hpp"
#include "TCT-Time.hpp"
#include "../cfg/CFGNode.hpp"

namespace TraceAnalysis {
  
  // Temporal Context Tree Abstract Node
  class TCTANode {
  public:
    TCTANode(int id, string name, int depth, CFGAGraph* cfgGraph, VMA vma) :
        id(id), name(name), depth(depth), time(NULL), 
        cfgGraph(cfgGraph), vma(vma), weight(1) {}
    TCTANode(const TCTANode& orig) : id(orig.id), name(orig.name), depth(orig.depth),
        cfgGraph(orig.cfgGraph), vma(orig.vma), weight(orig.weight) {
      if (orig.time == NULL) time = NULL;
      else time = orig.time->duplicate();
    }
    virtual ~TCTANode() {
      if (time != NULL) delete time;
    }
    virtual TCTANode* duplicate() = 0;
    
  protected:
    const int id;
    const string name;
    int depth;
    
    TCTATime* time;
    
    const CFGAGraph* cfgGraph; // The CFGAGraph node that contains the control flow graph for this node.
    const VMA vma; // RA for call sites or VMA for loops.
    
    int weight;
  };
  
  class TCTATraceNode : public TCTANode {
  public:
    TCTATraceNode(int id, string name, int depth, CFGAGraph* cfgGraph, VMA vma) :
      TCTANode(id, name, depth, cfgGraph, vma) {
      time = new TCTTraceTime();
    }
    TCTATraceNode(const TCTATraceNode& orig) : TCTANode(orig) {
      for (auto it = orig.children.begin(); it != orig.children.end(); it++)
        children.insert((*it)->duplicate());
    }
    ~TCTATraceNode() {
      for (auto it = children.begin(); it != children.end(); it++)
        delete (*it);
    }
    
  protected:
    vector<TCTANode*> children;
  };
  
}

#endif /* TCT_NODE_HPP */

