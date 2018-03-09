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
 * 
 * Temporal Context Tree nodes.
 */

#ifndef TCT_NODE_HPP
#define TCT_NODE_HPP

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "../TraceAnalysisCommon.hpp"
#include "TCT-CFG.hpp"
#include "TCT-Time.hpp"

namespace TraceAnalysis {
  
  // Temporal Context Tree Abstract Node
  class TCTANode {
  public:
    enum NodeType {
      Root,
      Func,
      Iter,
      Loop
    };
    
    TCTANode(NodeType type, int id, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
        type(type), id(id), name(name), cfgGraph(cfgGraph), ra(ra), 
        depth(depth), time(NULL), weight(1) {}
    TCTANode(const TCTANode& orig) : type(orig.type), id(orig.id), name(orig.name), 
        cfgGraph(orig.cfgGraph), ra(orig.ra), depth(orig.depth), weight(orig.weight) {
      if (orig.time == NULL) time = NULL;
      else time = orig.time->duplicate();
    }
    virtual ~TCTANode() {
      if (time != NULL) delete time;
    }
    // returns a pointer to a duplicate of this object. 
    // Caller responsible for deallocating the duplicate.
    virtual TCTANode* duplicate() = 0;
    
    // returns a pointer to a void duplicate of this object. 
    // Caller responsible for deallocating the void duplicate.
    virtual TCTANode* voidDuplicate() = 0;
    
    // Print contents of an object to a string for debugging purpose.
    virtual string toString(int maxDepth, Time minDuration) = 0;
    
    const NodeType type;
    const int id;
    const string name;
    // CFG Abstract Graph for this node.
    CFGAGraph* const cfgGraph; 
    // RA for call sites or VMA for loops.
    const VMA ra; 
    
  protected:
    int depth;
    TCTATime* time;
    int weight;
  };
  
  // Temporal Context Tree Abstract Trace Node
  class TCTATraceNode : public TCTANode {
  public:
    TCTATraceNode(NodeType type, int id, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
      TCTANode(type, id, name, depth, cfgGraph, ra) {
      time = new TCTTraceTime();
    }
    TCTATraceNode(const TCTATraceNode& orig) : TCTANode(orig) {
      for (auto it = orig.children.begin(); it != orig.children.end(); it++)
        children.push_back((*it)->duplicate());
    }
    virtual ~TCTATraceNode() {
      for (auto it = children.begin(); it != children.end(); it++)
        delete (*it);
    }
    
    virtual TCTTraceTime* getTraceTime() {
      return (TCTTraceTime*)time;
    }
    
    virtual int getNumChild() {
      return children.size();
    }
    
    virtual TCTANode* getChild(int idx) {
      return children[idx];
    }
    
    virtual void addChild(TCTANode* node) {
      children.push_back(node);
    }
    
    virtual string toString(int maxDepth, Time minDuration);
    
  protected:
    vector<TCTANode*> children;
  };
  
  // Temporal Context Tree Function Trace Node
  class TCTFunctionTraceNode : public TCTATraceNode {
  public:
    TCTFunctionTraceNode(int id, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
      TCTATraceNode(Func, id, name, depth, cfgGraph, ra) {}
    TCTFunctionTraceNode(const TCTFunctionTraceNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTFunctionTraceNode() {}
    
    virtual TCTANode* duplicate() {
      return new TCTFunctionTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() {
      return new TCTFunctionTraceNode(id, name, depth, cfgGraph, ra);
    }
  };
  
  // Temporal Context Tree Root Node
  class TCTRootNode : public TCTATraceNode {
  public:
    TCTRootNode(int id, string name, int depth) : 
            TCTATraceNode(Root, id, name, depth, NULL, 0) {}
    TCTRootNode(const TCTRootNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTRootNode() {}
    
    virtual TCTANode* duplicate() {
      return new TCTRootNode(*this);
    }
    virtual TCTANode* voidDuplicate() {
      return new TCTRootNode(id, name, depth);
    }
  };
  
  // Temporal Context Tree Iteration Trace Node
  class TCTIterationTraceNode : public TCTATraceNode {
  public:
    TCTIterationTraceNode(int id, uint64_t iterNum, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Iter, id, "ITER_#" + std::to_string(iterNum), depth, 
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
    TCTIterationTraceNode(int id, string name, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Iter, id, name, depth, 
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
    TCTIterationTraceNode(const TCTIterationTraceNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTIterationTraceNode() {}
    
    virtual TCTANode* duplicate() {
      return new TCTIterationTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() {
      return new TCTIterationTraceNode(id, name, depth, cfgGraph);
    }
  };
  
  class TCTLoopTraceNode : public TCTATraceNode {
    public:
    TCTLoopTraceNode(int id, string name, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Loop, id, name, depth, 
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
    TCTLoopTraceNode(const TCTLoopTraceNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTLoopTraceNode() {}
    
    virtual TCTANode* duplicate() {
      return new TCTLoopTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() {
      return new TCTLoopTraceNode(id, name, depth, cfgGraph);
    }
  };
}

#endif /* TCT_NODE_HPP */

