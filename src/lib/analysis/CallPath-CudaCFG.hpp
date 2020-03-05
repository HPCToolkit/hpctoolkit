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
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef Analysis_CallPath_CallPath_CudaCFG_hpp 
#define Analysis_CallPath_CallPath_CudaCFG_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {

void
transformCudaCFGMain(Prof::CallPath::Profile &prof);


// Directed edge
struct CCTEdge {
  Prof::CCT::ANode *from;
  Prof::CCT::ANode *to;

  CCTEdge(Prof::CCT::ANode *from, Prof::CCT::ANode *to) : from(from), to(to) {}

  bool operator < (const CCTEdge &other) const {
    if (this->from < other.from) {
      return true; 
    } else if ((this->from == other.from) && (this->to < other.to)) {
      return true;
    }
    return false;
  }
};


class CCTGraph {
 public:
  typedef std::map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > NeighborNodeMap;

 public:
  CCTGraph() {}

  std::set<CCTEdge>::iterator edgeBegin() {
    return _edges.begin();
  }

  std::set<CCTEdge>::iterator edgeEnd() {
    return _edges.end();
  }

  std::set<Prof::CCT::ANode *>::iterator nodeBegin() {
    return _nodes.begin();
  }

  std::set<Prof::CCT::ANode *>::iterator nodeEnd() {
    return _nodes.end();
  }

  NeighborNodeMap::iterator incoming_nodes(Prof::CCT::ANode *node) {
    return _incoming_nodes.find(node);
  }

  NeighborNodeMap::iterator incoming_nodes_end() {
    return _incoming_nodes.end();
  }

  size_t outgoing_nodes_size(Prof::CCT::ANode *node) {
    auto iter = _outgoing_nodes.find(node); 
    if (iter == _outgoing_nodes.end()) {
      return 0;
    }
    return _outgoing_nodes[node].size();
  }

  size_t incoming_nodes_size(Prof::CCT::ANode *node) {
    auto iter = _incoming_nodes.find(node); 
    if (iter == _incoming_nodes.end()) {
      return 0;
    }
    return _incoming_nodes[node].size();
  }

  NeighborNodeMap::iterator outgoing_nodes(Prof::CCT::ANode *node) {
    return _outgoing_nodes.find(node);
  }
  
  NeighborNodeMap::iterator outgoing_nodes_end() {
    return _outgoing_nodes.end();
  }
 
  void addEdge(Prof::CCT::ANode *from, Prof::CCT::ANode *to) {
    if (_nodes.find(from) == _nodes.end()) {
      _nodes.insert(from);
    }

    if (_nodes.find(to) == _nodes.end()) {
      _nodes.insert(to);
    }

    CCTEdge edge(from, to);
    if (_edges.find(edge) == _edges.end()) {
      _edges.insert(std::move(edge));
      _incoming_nodes[to].push_back(from);
      _outgoing_nodes[from].push_back(to);
    }
  }

  void addNode(Prof::CCT::ANode *node) {
    if (_nodes.find(node) != _nodes.end()) {
      _nodes.insert(node);
    }
  }

  size_t size() {
    return _nodes.size();
  }  

 private:
  NeighborNodeMap _incoming_nodes;
  NeighborNodeMap _outgoing_nodes;
  std::set<CCTEdge> _edges;
  std::set<Prof::CCT::ANode *> _nodes;
};

}  // CallPath

}  // Analysis

#endif  // Analysis_CallPath_CallPath_CudaCFG_hpp
