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
// Copyright ((c)) 2002-2018, Rice University
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

#ifndef Analysis_CCTGraph_hpp
#define Analysis_CCTGraph_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof/Struct-Tree.hpp>


struct CCTNode {
  VMA vma;
  GPUParse::InstructionStat *inst;
  GPUParse::Function *function;
  GPUParse::Block *block;
  int latency;
  int frequency;
  int path_length;    // for debugging
  float path_length_inv;
  long latency_blame;

  CCTNode()
    : vma(0),
    inst(NULL),
    function(NULL),
    block(NULL),
    latency(0),
    frequency(0),
    path_length(1),
    path_length_inv(1),
    latency_blame(0) {}

  bool operator < (const CCTNode &other) const {
    if (this->inst->pc < other.inst->pc) {
      return true;
    }
    return false;
  }
};

// Directed edge
struct CCTEdge {
  int from;
  int to;

  CCTEdge(int from, int to) : from(from), to(to) {}

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
  typedef std::map<int, std::set<int> > NeighborNodeMap;

 public:
  CCTGraph() {}

  typename std::set<CCTEdge >::iterator edgeBegin() {
    return _edges.begin();
  }

  typename std::set<CCTEdge >::iterator edgeEnd() {
    return _edges.end();
  }

  std::map<int, CCTNode> nodes() {
    return _nodes;
  }

#if 0
  typename std::set<CCTNode>::iterator nodeBegin() {
    return _nodes.begin();
  }

  typename std::set<CCTNode>::iterator nodeEnd() {
    return _nodes.end();
  }
#endif

  typename NeighborNodeMap::iterator incoming_nodes_end() {
    return _incoming_nodes.end();
  }

  NeighborNodeMap incoming_nodes() {
    return _incoming_nodes;
  }

  typename NeighborNodeMap::const_iterator outgoing_nodes_end() const {
    return _outgoing_nodes.end();
  }

  typename NeighborNodeMap::iterator outgoing_nodes_end() {
    return _outgoing_nodes.end();
  }

  void
  updateChildPathLength
  (
   int from,
   int fromPathLength
  )
  {
    for (int to: _outgoing_nodes[from]) {
      int oldPathLength = _nodes[to].path_length;
      int newPathLength = std::max(oldPathLength, fromPathLength + 1); // incrementing the path length of the 'to' node
      if (newPathLength != oldPathLength) {
        _nodes[to].path_length = newPathLength;
        _nodes[to].path_length_inv = 1.0 / newPathLength;
        updateChildPathLength(to, newPathLength);
      }
    }
  }


  void addEdge(int from, int to) {
    CCTEdge edge(from, to);
    if (_edges.find(edge) == _edges.end()) {
      _edges.insert(std::move(edge));
      _incoming_nodes[to].insert(from);
      _outgoing_nodes[from].insert(to); // is outgoing edges needed?
      int oldPathLength = _nodes[to].path_length;
      int newPathLength= std::max(oldPathLength, _nodes[from].path_length + 1); // incrementing the path length of the 'to' node
      if (newPathLength != oldPathLength) {
        _nodes[to].path_length = newPathLength;
        _nodes[to].path_length_inv = 1.0 / newPathLength;
        // update path length of outgoing edges from to
        updateChildPathLength(to, newPathLength);
      }
    }
  }

  void removeEdge(typename std::set<CCTEdge >::iterator edge) {
    int from = edge->from;
    int to = edge->to;
    // Erase edges
    _incoming_nodes[to].erase(from);
    _outgoing_nodes[from].erase(to);
    _edges.erase(edge);
    // XXX(Keren): do not erase nodes
  }

  void addNode(CCTNode node) {
    std::map<int, CCTNode>::iterator it = _nodes.find(node.vma);
    if (it == _nodes.end()) {
      _nodes[node.vma] = node;
    }
  }

  size_t size() {
    return _nodes.size();
  }

  size_t edge_size() {
    return _edges.size();
  }

  void clear() {
    _nodes.clear();
    _edges.clear();
    _incoming_nodes.clear();
    _outgoing_nodes.clear();
  }

 private:
  NeighborNodeMap _incoming_nodes;
  NeighborNodeMap _outgoing_nodes;
  typename std::set<CCTEdge > _edges;
  std::map<int, CCTNode> _nodes;
};

#endif  // Analysis_CCTGraph_hpp
