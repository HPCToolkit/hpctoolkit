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

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>


namespace Analysis {

// Directed edge
template<class T>
struct CCTEdge {
  T from;
  T to;

  CCTEdge(T from, T to) : from(from), to(to) {}

  bool operator < (const CCTEdge &other) const {
    if (this->from < other.from) {
      return true;
    } else if ((this->from == other.from) && (this->to < other.to)) {
      return true;
    }
    return false;
  }
};


template<class T>
class CCTGraph {
 public:
  typedef std::map<T, std::set<T> > NeighborNodeMap;

 public:
  CCTGraph() {}

  typename std::set<CCTEdge<T> >::iterator edgeBegin() {
    return _edges.begin();
  }

  typename std::set<CCTEdge<T> >::iterator edgeEnd() {
    return _edges.end();
  }

  std::set<T> nodes() {
    return _nodes;
  }

  typename std::set<T>::iterator nodeBegin() {
    return _nodes.begin();
  }

  typename std::set<T>::iterator nodeEnd() {
    return _nodes.end();
  }

  typename NeighborNodeMap::iterator incoming_nodes(T node) {
    return _incoming_nodes.find(node);
  }

  typename NeighborNodeMap::iterator incoming_nodes_end() {
    return _incoming_nodes.end();
  }

  size_t outgoing_nodes_size(T node) const {
    auto iter = _outgoing_nodes.find(node);
    if (iter == _outgoing_nodes.end()) {
      return 0;
    }
    return iter->second.size();
  }

  size_t incoming_nodes_size(T node) const {
    auto iter = _incoming_nodes.find(node);
    if (iter == _incoming_nodes.end()) {
      return 0;
    }
    return iter->second.size();
  }

  typename NeighborNodeMap::iterator outgoing_nodes(T node) const {
    return _outgoing_nodes.find(node);
  }

  typename NeighborNodeMap::iterator outgoing_nodes_end() const {
    return _outgoing_nodes.end();
  }

  typename NeighborNodeMap::iterator outgoing_nodes(T node) {
    return _outgoing_nodes.find(node);
  }

  typename NeighborNodeMap::iterator outgoing_nodes_end() {
    return _outgoing_nodes.end();
  }

  void addEdge(T from, T to) {
    if (_nodes.find(from) == _nodes.end()) {
      _nodes.insert(from);
    }

    if (_nodes.find(to) == _nodes.end()) {
      _nodes.insert(to);
    }

    CCTEdge<T> edge(from, to);
    if (_edges.find(edge) == _edges.end()) {
      _edges.insert(std::move(edge));
      _incoming_nodes[to].insert(from);
      _outgoing_nodes[from].insert(to);
    }
  }

  void removeEdge(typename std::set<CCTEdge<T> >::iterator edge) {
    T from = edge->from;
    T to = edge->to;
    // Erase edges
    _incoming_nodes[to].erase(from);
    _outgoing_nodes[from].erase(to);
    _edges.erase(edge);
    // XXX(Keren): do not erase nodes
  }

  void addNode(T node) {
    if (_nodes.find(node) == _nodes.end()) {
      _nodes.insert(node);
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
  typename std::set<CCTEdge<T> > _edges;
  std::set<T> _nodes;
};

}  // Analysis

#endif  // Analysis_CCTGraph_hpp
