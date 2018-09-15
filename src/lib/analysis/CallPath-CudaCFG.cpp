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

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>

#include <typeinfo>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>

#include "CallPath-CudaCFG.hpp"

using std::string;

#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Metric-ADesc.hpp>

#include <lib/profxml/XercesUtil.hpp>
#include <lib/profxml/PGMReader.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>


#define DEBUG_CALLPATH_CUDACFG 1

namespace Analysis {

namespace CallPath {

typedef std::map<VMA, std::vector<Prof::CCT::ANode *> > CallMap;

typedef std::map<Prof::CCT::ANode *, std::map<Prof::CCT::ANode *, double> > IncomingSamplesMap;

// Static functions
static void
constructCallMap(Prof::CCT::ANode *root, CallMap &call_map);

static void
constructCCTGraph(Prof::CCT::ANode *root, CallMap &call_map, CCTGraph &cct_graph);

static void
findGPURoots(CCTGraph &cct_graph, std::vector<Prof::CCT::ANode *> &gpu_roots);

static void
gatherIncomingSamples(CCTGraph &cct_graph,
  Prof::Metric::Mgr &mgr, IncomingSamplesMap &node_map);

static void
constructCallingContext(IncomingSamplesMap &incoming_samples,
  CCTGraph &cct_graph, std::vector<Prof::CCT::ANode *> &gpu_roots);

static void
copyPath(IncomingSamplesMap &incoming_samples,
  CCTGraph &cct_graph,
  Prof::CCT::ANode *cur, 
  Prof::CCT::ANode *prev,
  double adjust_factor);

static void
deleteTree(Prof::CCT::ANode *root);


static inline bool
isNVIDIADevice(const std::string &device) {
  return device.find("NVIDIA") != std::string::npos;
}


// Static inline functions
static inline Prof::Struct::Stmt *
getCudaCallStmt(Prof::CCT::ANode *node) {
  auto *strct = node->structure();
  if (strct != NULL && strct->type() == Prof::Struct::ANode::TyStmt) {
    auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(strct);
    if (stmt->stmtType() == Prof::Struct::Stmt::STMT_CALL) {
      if (isNVIDIADevice(stmt->device())) {
        return stmt;
      }
    }
  }
  return NULL;
}


static inline void
debugCallGraph(CCTGraph &cct_graph) {
  for (auto it = cct_graph.edgeBegin(); it != cct_graph.edgeEnd(); ++it) {
    std::cout << it->from->id() << "->" << it->to->id() << std::endl;
  }
}


void
transformCudaCFGMain(Prof::CallPath::Profile& prof) {
  // Construct a map to pair vmas and calls
  Prof::CCT::ANode *root = prof.cct()->root();
  CallMap call_map;
  constructCallMap(root, call_map);

  // Construct a cct call graph on GPU
  CCTGraph cct_graph;
  constructCCTGraph(root, call_map, cct_graph);

  if (DEBUG_CALLPATH_CUDACFG) {
    debugCallGraph(cct_graph);
  }

  // Find gpu global functions
  std::vector<Prof::CCT::ANode *> gpu_roots;
  findGPURoots(cct_graph, gpu_roots);

  // Record input samples for each node
  IncomingSamplesMap incoming_samples;
  gatherIncomingSamples(cct_graph, *(prof.metricMgr()), incoming_samples);

  // TODO(keren): Test scc

  // Copy from every gpu_root to leafs
  constructCallingContext(incoming_samples, cct_graph, gpu_roots);
}


static void
constructCallMap(Prof::CCT::ANode *root, CallMap &call_map) {
  Prof::CCT::ANodeIterator it(root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  // Find call nodes where device = nvidia and construct a graph
  for (Prof::CCT::ANode *n = NULL; (n = it.current()); ++it) {
    auto *stmt = getCudaCallStmt(n);
    if (stmt != NULL) {
      call_map[stmt->target()].push_back(n);
    }
  }
}


static void
constructCCTGraph(Prof::CCT::ANode *root, CallMap &call_map,
  CCTGraph &cct_graph) {
  // Step1: Add proc->call
  for (auto &entry : call_map) {
    auto &vec = entry.second;
    for (auto *call_node : vec) {
      auto *parent = call_node->ancestorProcFrm();
      cct_graph.addEdge(parent, call_node);
    }
  }

  // Step2: Add call->proc if statement and vma matches, 
  Prof::CCT::ANodeIterator it(root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = it.current()); ++it) {
    auto *strct = n->structure();
    if (strct != NULL && strct->type() == Prof::Struct::ANode::TyProc) {
      auto *proc = dynamic_cast<Prof::Struct::Proc *>(strct);
      auto vma_set = *(proc->vmaSet().begin());
      VMA vma = vma_set.beg();
      if (call_map.find(vma) != call_map.end()) {
        auto call_nodes = call_map[vma];
        for (auto *call_node : call_nodes) {
          cct_graph.addEdge(call_node, n);
        }
      }
    }
  }
}


static void
findGPURoots(CCTGraph &cct_graph, std::vector<Prof::CCT::ANode *> &gpu_roots) {
  for (auto it = cct_graph.nodeBegin(); it != cct_graph.nodeEnd(); ++it) {
    if (cct_graph.incoming_nodes(*it) == cct_graph.incoming_nodes_end()) {
      gpu_roots.push_back(*it);
    }
  }
}


static void
gatherIncomingSamples(CCTGraph &cct_graph,
  Prof::Metric::Mgr &mgr, IncomingSamplesMap &node_map) {
  int sample_metric_idx = -1;
  for (size_t i = 0; i < mgr.size(); ++i) {
    if (mgr.metric(i)->namePfxBase() == "GPU_ISAMP") {
      sample_metric_idx = i;
      break;
    }
  }
  for (auto it = cct_graph.nodeBegin(); it != cct_graph.nodeEnd(); ++it) {
    Prof::CCT::ANode *node = *it;
    auto *strct = node->structure();
    if (strct != NULL && strct->type() == Prof::Struct::ANode::TyProc) {
      if (cct_graph.incoming_nodes(node) != cct_graph.incoming_nodes_end()) {
        std::vector<Prof::CCT::ANode *> &vec = cct_graph.incoming_nodes(node)->second;
        if (sample_metric_idx == -1) {
          for (auto *neighbor : vec) {
            // By default, set it to one
            node_map[node][neighbor] = 1.0;
          }
        } else {
          for (auto *neighbor : vec) {
            node_map[node][neighbor] = std::max(neighbor->metric(sample_metric_idx), 1.0);
          }
        }
      }
    }
  }
}


static void
constructCallingContext(IncomingSamplesMap &incoming_samples,
  CCTGraph &cct_graph, std::vector<Prof::CCT::ANode *> &gpu_roots) {
  for (auto *gpu_root : gpu_roots) {
    auto *parent = gpu_root->parent();
    gpu_root->unlink();
    copyPath(incoming_samples, cct_graph, gpu_root, parent, 1.0);
    deleteTree(gpu_root);
  }
}


static void
copyPath(IncomingSamplesMap &incoming_samples,
  CCTGraph &cct_graph, 
  Prof::CCT::ANode *cur, Prof::CCT::ANode *prev,
  double adjust_factor) {
  // Copy node
  auto *new_node = cur->clone();
  new_node->link(prev);
  // Adjust metrics
  for (size_t i = 0; i < new_node->numMetrics(); ++i) {
    new_node->metric(i) *= adjust_factor;
  }
  
  auto *stmt = getCudaCallStmt(cur);
  if (stmt != NULL) {
    // Case 1: Call node, skip to the procedure
    auto *n = cct_graph.outgoing_nodes(cur)->second[0];
    n->unlink();
    double sum_samples = 0.0;
    for (auto &neighor : incoming_samples[n]) {
      sum_samples += neighor.second;
    }
    double cur_samples = incoming_samples[n][cur];
    adjust_factor *= cur_samples / sum_samples;
    copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
  } else if (!cur->isLeaf()) {
    // Case 2: Iterate through children
    Prof::CCT::ANodeChildIterator it(cur, NULL/*filter*/);
    for (Prof::CCT::ANode *n = NULL; (n = it.current()); ++it) {
      copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
    }
  }
}


static void
deleteTree(Prof::CCT::ANode *root) {
}


} // namespace CallPath

} // namespace Analysis
