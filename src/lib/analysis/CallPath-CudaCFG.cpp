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

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>

#include <typeinfo>
#include <unordered_map>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <boost/graph/strong_components.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <include/uint.h>
#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>

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


#include <vector>
#include <queue>
#include <iostream>


#define DEBUG_CALLPATH_CUDACFG 0
#define OUTPUT_SCC_FRAME 1
#define SIMULATE_SCC_WITH_LOOP 1

#define WARP_SIZE 1

namespace Analysis {

namespace CallPath {

typedef std::map<VMA, std::set<Prof::Struct::ANode *> > StructCallMap;

typedef std::map<VMA, std::vector<Prof::CCT::ANode *> > ProfCallMap;

typedef std::map<Prof::Struct::ANode *, Prof::CCT::ANode *> StructProfMap;

typedef std::map<VMA, Prof::CCT::ANode *> ProfProcMap;

typedef std::map<Prof::CCT::ANode *, std::set<int> > ProfInstMap;

typedef std::map<Prof::CCT::ANode *, std::map<Prof::CCT::ANode *, std::map<int, double> > > IncomingInstMap;

typedef std::map<int, double> AdjustFactor;

static std::vector<size_t> gpu_inst_index;

static void
normalizeTraceNodes(std::set<Prof::CCT::ANode *> &gpu_roots);

// Static functions
static void
constructFrame(Prof::Struct::ANode *strct, Prof::CCT::ANode *frame, 
  Prof::CCT::ANode *proc, StructProfMap &struct_cct_map);

static void
constructStructCallMap(Prof::Struct::ANode *struct_root, StructCallMap &struct_call_map);

static void
constructCallGraph(Prof::CCT::ANode *prof_root, CCTGraph *cct_graph, StructCallMap &struct_call_map);

static void
findGPURoots(Prof::CCT::ANode *prof_root, Prof::Struct::ANode *struct_root, std::set<Prof::CCT::ANode *> &gpu_roots);

static bool
findRecursion(CCTGraph *cct_graph,
  std::unordered_map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > &cct_groups);

static void
mergeSCCNodes(CCTGraph *cct_graph, CCTGraph *old_cct_graph,
  std::unordered_map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > &cct_groups);

static void
gatherIncomingSamples(CCTGraph *cct_graph, IncomingInstMap &node_map, bool find_recursion);

static void
constructCallingContext(CCTGraph *cct_graph, IncomingInstMap &incoming_samples);

static void
copyPath(IncomingInstMap &incoming_samples,
  CCTGraph *cct_graph,
  Prof::CCT::ANode *cur, 
  Prof::CCT::ANode *prev,
  AdjustFactor adjust_factor);

static void
deleteTree(Prof::CCT::ANode *prof_root);


// Static inline functions
static inline bool
isSCCNode(Prof::CCT::ANode *node) {
  return node->type() == Prof::CCT::ANode::TySCC;
}


static inline bool
isCudaCallStmt(Prof::Struct::ANode *strct) {
  if (strct != NULL && strct->type() == Prof::Struct::ANode::TyStmt) {
    auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(strct);
    if (stmt->stmtType() == Prof::Struct::Stmt::STMT_CALL) {
      if (stmt->device().find("NVIDIA") != std::string::npos &&
        stmt->target() != 0) {
        // some library functions, e.g. free & malloc, might be included in a cubin but without an address
        return true;
      }
    }
  }
  return false;
}


static inline Prof::Struct::Stmt *
getCudaCallStmt(Prof::CCT::ANode *node) {
  auto *strct = node->structure();
  if (isCudaCallStmt(strct)) {
    return dynamic_cast<Prof::Struct::Stmt *>(strct);
  }
  return NULL;
}


static inline Prof::Struct::Proc *
getProcStmt(Prof::CCT::ANode *node) {
  auto *strct = node->structure();
  if (strct != NULL && strct->type() == Prof::Struct::ANode::TyProc) {
    return dynamic_cast<Prof::Struct::Proc *>(strct);
  }
  return NULL;
}


static inline void
debugGPUInst() {
  std::cout << "I: ";
  for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
    std::cout << "  " << gpu_inst_index[i];
  }
  std::cout << std::endl;
}


static inline void
debugCallGraph(CCTGraph *cct_graph) {
  for (auto it = cct_graph->edgeBegin(); it != cct_graph->edgeEnd(); ++it) {
    std::string from, to;
    if (getProcStmt(it->from) != NULL) {
      from = "(P)" + dynamic_cast<Prof::CCT::AProcNode *>(it->from)->procName();
    } else if (isSCCNode(it->from)) {
      from = "(S)";
    } else if (getCudaCallStmt(it->from)) {
      from = "(C)";
    }
    if (getProcStmt(it->to) != NULL) {
      to = "(P)" + dynamic_cast<Prof::CCT::AProcNode *>(it->to)->procName();
    } else if (isSCCNode(it->to)) {
      to = "(S)";
    } else if (getCudaCallStmt(it->to)) {
      to = "(C)";
    }
    std::cout << std::dec << it->from->id() << from << "->" << it->to->id() << to << std::endl;
  }
  
  debugGPUInst();
  for (auto it = cct_graph->nodeBegin(); it != cct_graph->nodeEnd(); ++it) {
    Prof::CCT::ANode *n = *it;
    if (getCudaCallStmt(n) != NULL) {
      std::cout << std::dec << n->id() << "(C)";
      for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
        std::cout << "  " << n->demandMetric(gpu_inst_index[i]);
      }
      std::cout << std::endl;
    }
  }
}


static inline void
debugProfTree(Prof::CCT::ANode *prof_root) {
  std::cout << "File names: " << std::endl;
  Prof::CCT::ANodeIterator it(prof_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = it.current()); ++it) {
    if (getProcStmt(n)) {
      auto *stmt = getProcStmt(n); 
      auto *p_stmt = n->parent()->structure();
      std::cout << stmt->ancestorFile()->name() << ": " << dynamic_cast<Prof::CCT::AProcNode *>(n)->procName() <<
        " (" << std::hex << stmt->vmaSet().begin()->beg() << std::dec << ") called from " <<
        dynamic_cast<Prof::Struct::ACodeNode *>(p_stmt)->begLine() <<
        " node " << n->parent() << std::endl;
    }
  }
}


void
transformCudaCFGMain(Prof::CallPath::Profile& prof) {
  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << std::endl;
    std::cout << "-------------------------------------------------" << std::endl;
    std::cout << "Step 0: Check if prof has GPU metrics" << std::endl;
    std::cout << "-------------------------------------------------" << std::endl;
  }
  // Check if prof contains gpu metrics
  auto *mgr = prof.metricMgr(); 
  for (size_t i = 0; i < mgr->size(); ++i) {
    if (mgr->metric(i)->namePfxBase() == GPU_INST_METRIC_NAME &&
      mgr->metric(i)->type() == Prof::Metric::ADesc::TyIncl) {
      // Assume exclusive metrics index is i+1
      gpu_inst_index.push_back(i);
    }
  }
  // Skip non-gpu prof
  if (gpu_inst_index.size() == 0) {
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << "Skip non-gpu prof" << std::endl;
    }
    return;
  }
  if (DEBUG_CALLPATH_CUDACFG) {
    debugGPUInst();
  }

  // Find the parents of gpu global functions
  std::set<Prof::CCT::ANode *> gpu_roots;
  findGPURoots(prof.cct()->root(), prof.structure()->root(), gpu_roots);

  // If the first prof node has no inst metrics, assign it one
  normalizeTraceNodes(gpu_roots);

  // Find <target_vma, <Struct::Stmt> > mappings
  StructCallMap struct_call_map; 
  constructStructCallMap(prof.structure()->root(), struct_call_map);

  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << "Number of gpu roots: " << gpu_roots.size() << std::endl;
  }

  for (auto *gpu_root : gpu_roots) {
    if (DEBUG_CALLPATH_CUDACFG) {
      auto *strct = gpu_root->structure();
      auto line = dynamic_cast<Prof::Struct::ACodeNode *>(strct)->begLine();
      auto *p_strct = strct->ancestor(Prof::Struct::ANode::TyProc);
      std::cout << "-------------------------------------------------" << std::endl;
      std::cout << "Current gpu root " << p_strct->name() << " line " << line << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
    }
    if (DEBUG_CALLPATH_CUDACFG) {
      debugProfTree(gpu_root);
    }

    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
      std::cout << "Step 1: Construct call graph" << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
    }
    // Construct a call map for a target
    CCTGraph *cct_graph = new CCTGraph();
    constructCallGraph(gpu_root, cct_graph, struct_call_map);

    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << std::endl;
      std::cout << "Call graph before merging SCCs" << std::endl;
      debugCallGraph(cct_graph);
    }

    // Handle recursive calls
    std::unordered_map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > cct_groups;
    bool find_recursion = false;
    if (findRecursion(cct_graph, cct_groups)) {
      find_recursion = true;
      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << std::endl;
        std::cout << "-------------------------------------------------" << std::endl;
        std::cout << "Step 1.1: Construct call graph for recursive calls" << std::endl;
        std::cout << "-------------------------------------------------" << std::endl;
      }
      CCTGraph *old_cct_graph = cct_graph;
      cct_graph = new CCTGraph();
      mergeSCCNodes(cct_graph, old_cct_graph, cct_groups);
      delete old_cct_graph;
      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << "Call graph after merging SCCs" << std::endl;
        debugCallGraph(cct_graph);
      }
    }

    // Record input samples for each node
    IncomingInstMap incoming_samples;
    gatherIncomingSamples(cct_graph, incoming_samples, find_recursion);

    // Copy from every gpu_root to leafs
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
      std::cout << "Step 2: Construct calling context tree" << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
    }
    constructCallingContext(cct_graph, incoming_samples);

    delete cct_graph;
  }
}


static void
normalizeTraceNodes(std::set<Prof::CCT::ANode *> &gpu_roots) {
  for (auto *gpu_root : gpu_roots) {
    Prof::CCT::ANodeIterator prof_it(gpu_root, NULL/*filter*/, false/*leavesOnly*/,
      IteratorStack::PreOrder);
    for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
      if (n->isLeaf()) {
        bool find = false;
        for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
          if (n->demandMetric(gpu_inst_index[i]) != 0.0) {
            find = true;
            break;
          }
        }
        if (!find) {
          // Find a pseudo instruction node for tracing
          n->demandMetric(gpu_inst_index[0]) = WARP_SIZE;
          n->demandMetric(gpu_inst_index[0] + 1) = WARP_SIZE;
        }
      }
    }
  }
}


static void
constructFrame(Prof::Struct::ANode *strct, Prof::CCT::ANode *frame,
  Prof::CCT::ANode *proc, StructProfMap &struct_cct_map) {
  for (Prof::Struct::ANodeChildIterator it(strct); it.Current(); ++it) {
    Prof::Struct::ANode* strct = it.current();

    // Done: if we reach the natural base case or embedded procedure
    if (strct->isLeaf() || typeid(*strct) == typeid(Prof::Struct::Proc)) {
      continue;
    }

    // Create frame, the frame node corresponding to strct
    Prof::CCT::ANode* frm = NULL;
    if (typeid(*strct) == typeid(Prof::Struct::Loop)) {
      frm = new Prof::CCT::Loop(frame, dynamic_cast<Prof::Struct::ACodeNode *>(strct));
    }
    else if (typeid(*strct) == typeid(Prof::Struct::Alien)) {
      frm = new Prof::CCT::Proc(frame, dynamic_cast<Prof::Struct::ACodeNode *>(strct));
    }
    
    if (frm) {
      struct_cct_map[strct] = frm;
      // Recur
      constructFrame(strct, frm, proc, struct_cct_map);
    }
  }
}


static void
constructStructCallMap(Prof::Struct::ANode *struct_root, StructCallMap &struct_call_map) {
  // Find call nodes where device = nvidia
  Prof::Struct::ANodeIterator struct_it(struct_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::Struct::ANode *n = NULL; (n = struct_it.current()); ++struct_it) {
    if (isCudaCallStmt(n)) {
      auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(n);
      struct_call_map[stmt->target()].insert(n);
    }
  }
}


static void
constructCallGraph(Prof::CCT::ANode *prof_root, CCTGraph *cct_graph, StructCallMap &struct_call_map) {
  // In this function, we only have flat samples without call stacks
  // struct_prof_map maps structs to unique ccts
  // prof_inst_map records samples withint the procedures exclusively 
  
  // Step1: Iterate over all proc nodes in the prof
  // TODO(Keren): multiple global function calls under a single target
  StructProfMap struct_prof_map;  // <Struct, CCT>
  ProfCallMap prof_call_map;  // <target_vma, [CCT::Call]>
  ProfProcMap prof_proc_map;  // <proc_vma, CCT::ProcFrm>
  Prof::CCT::ANodeIterator prof_it(prof_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << "Existing call nodes metrics" << std::endl;
    debugGPUInst();
  }
  for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
    if (getCudaCallStmt(n) != NULL) {  // call
      auto *stmt = getCudaCallStmt(n);
      prof_call_map[stmt->target()].push_back(n);

      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << dynamic_cast<Prof::Struct::ACodeNode *>(stmt)->begLine();
        for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
          std::cout << "  " << n->demandMetric(gpu_inst_index[i]);
        }
        std::cout << std::endl;
      }
    } else if (getProcStmt(n) != NULL) {  // proc
      auto *stmt = getProcStmt(n);
      auto vma = stmt->vmaSet().begin()->beg();
      prof_proc_map[vma] = n;
    }
    if (n->structure() != NULL) { 
      struct_prof_map[n->structure()] = n;
    }
  }

  // Step2: Count instructions for each kernel launched from different threads
  ProfInstMap prof_inst_map;
  for (auto &entry : prof_proc_map) {
    auto *proc = entry.second;
    // Iterate children
    Prof::CCT::ANodeChildIterator it(proc, NULL/*filter*/);
    for (Prof::CCT::ANode *n = NULL; (n = it.current()); ++it) {
      // Thread (Process) list
      for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
        if (n->hasMetricSlow(gpu_inst_index[i])) {
          prof_inst_map[proc].insert(i);
        }
      }
    }
  }

  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << std::endl;
    std::cout << "Proc inst before inst make up" << std::endl;
    for (auto &entry : prof_proc_map) {
      auto *proc = entry.second;
      std::cout << dynamic_cast<Prof::CCT::AProcNode *>(proc)->procName();
      for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
        if (prof_inst_map[proc].find(i) != prof_inst_map[proc].end()) {
          std::cout << " T ";
        } else {
          std::cout << " F ";
        }
      }
      std::cout << std::endl;
    }
  }

  // Step3: Add call->proc
  std::queue<std::pair<VMA, Prof::CCT::ANode *> > prof_procs;
  for (auto &entry : prof_proc_map) {
    prof_procs.push(entry);
  }

  // Complexity: O(TN), N procs, each time at least 1 metric gets updated, each node has T metric slots
  // Specific Complexity: O(TN * N), for each iteration, a node may be called from at most N places
  while (prof_procs.empty() == false) {
    auto &entry = prof_procs.front();
    prof_procs.pop();
    auto vma = entry.first;
    auto *proc = entry.second;
    // If there is at least one call node, do not bother.
    // If there is no call node for the procedure, we manually make up one
    if (prof_call_map.find(vma) == prof_call_map.end()) {
      for (auto *struct_call : struct_call_map[vma]) {
        // Create a scope frame
        Prof::Struct::ANode *scope = struct_call->ancestor(Prof::Struct::ANode::TyLoop,
          Prof::Struct::ANode::TyAlien,
          Prof::Struct::ANode::TyProc);
        // Ancester does not exist
        if (struct_prof_map.find(scope) == struct_prof_map.end()) {
          auto *struct_proc = struct_call->ancestorProc();
          // Update prof->proc queue and map
          auto frm_vma = struct_proc->vmaSet().begin()->beg();
          auto *frm_proc = new Prof::CCT::ProcFrm(NULL, struct_proc);
          // Add frm_vma->proc
          prof_proc_map[frm_vma] = frm_proc;
          // Add struct->proc
          struct_prof_map[struct_proc] = frm_proc;
          constructFrame(struct_proc, frm_proc, frm_proc, struct_prof_map); 
        }
        // Now the corresponding prof node exists
        Prof::CCT::ANode *frm_scope = struct_prof_map[scope];
        auto *prof_call = new Prof::CCT::Call(frm_scope, 0);
        auto *struct_code = dynamic_cast<Prof::Struct::ACodeNode *>(struct_call);
        prof_call->structure(struct_code);
        // Add to prof_call_map
        prof_call_map[vma].push_back(prof_call);
      }
    }
    for (auto *prof_call : prof_call_map[vma]) {
      // Add gpu call insts
      for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
        if (prof_inst_map[proc].find(i) != prof_inst_map[proc].end()) {
          auto *frm_proc = prof_call->ancestorProcFrm();
          auto *struct_proc = frm_proc->structure();
          auto frm_vma = struct_proc->vmaSet().begin()->beg();
          // Set gpu instruction to WARP SIZE if not sampled
          if (prof_call->demandMetric(gpu_inst_index[i]) == 0.0) {
            // Inclusive
            prof_call->demandMetric(gpu_inst_index[i]) = WARP_SIZE;
            // XXX(Keren): Is adding exclusive necessary here?
            prof_call->demandMetric(gpu_inst_index[i] + 1) = WARP_SIZE;
          }
          // The proc has not been added
          if (prof_inst_map[frm_proc].find(i) == prof_inst_map[frm_proc].end()) {
            prof_inst_map[prof_call->ancestorProcFrm()].insert(i);
            // Add a new entry
            std::pair<VMA, Prof::CCT::ANode *> new_entry(frm_vma, frm_proc);
            prof_procs.push(new_entry);
            if (DEBUG_CALLPATH_CUDACFG) {
              std::cout << frm_proc->procName() << " add "
                << dynamic_cast<Prof::CCT::AProcNode *>(proc)->procName() << " "
                << i << std::endl;
            }
          }
        }
      }
    }
  }

  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << "Proc inst after call inst make up" << std::endl;
    for (auto &entry : prof_proc_map) {
      auto *proc = entry.second;
      std::cout << dynamic_cast<Prof::CCT::AProcNode *>(proc)->procName();
      for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
        if (prof_inst_map[proc].find(i) != prof_inst_map[proc].end()) {
          std::cout << " T ";
        } else {
          std::cout << " F ";
        }
      }
      std::cout << std::endl;
    }
  }

  // Update call graph
  for (auto &entry : prof_proc_map) {
    auto vma = entry.first;
    auto *proc = entry.second;
    for (auto *call : prof_call_map[vma]) {
      // prof_call_map has been filled out
      cct_graph->addEdge(call, proc);
    }
  }

  // Step4: Add proc->call
  for (auto &entry : prof_call_map) {
    for (auto *call : entry.second) {
      // Prune call nodes that has been sampled but maps to a function that does not contain samples
      auto *parent = call->ancestorProcFrm();
      auto *stmt = getProcStmt(parent);
      auto vma = stmt->vmaSet().begin()->beg();
      if (prof_proc_map.find(vma) != prof_proc_map.end()) {
        cct_graph->addEdge(parent, call);
      } else {
        call->unlink();
      }
    }
  }
}


static bool
findRecursion(CCTGraph *cct_graph,
  std::unordered_map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > &cct_groups) {
  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << std::endl;
    std::cout << "Finding recursion in cct graphs" << std::endl;
  }
  std::unordered_map<int, int> graph_index_converter;
  std::unordered_map<int, Prof::CCT::ANode *> graph_index_reverse_converter;
  int start_index = 0;
  typedef boost::adjacency_list <boost::vecS, boost::vecS, boost::directedS> Graph;
  const int N = cct_graph->size();
  Graph G(N);

  // Find recursive calls
  for (auto it = cct_graph->edgeBegin(); it != cct_graph->edgeEnd(); ++it) {
    auto from_id = it->from->id();
    if (graph_index_converter.find(from_id) != graph_index_converter.end()) {
      from_id = graph_index_converter[from_id];
    } else {
      graph_index_converter[from_id] = start_index;
      graph_index_reverse_converter[start_index] = it->from;
      from_id = start_index++;
    }

    auto to_id = it->to->id();
    if (graph_index_converter.find(to_id) != graph_index_converter.end()) {
      to_id = graph_index_converter[to_id];
    } else {
      graph_index_converter[to_id] = start_index;
      graph_index_reverse_converter[start_index] = it->to;
      to_id = start_index++;
    }

    if (from_id == to_id) {
      std::cout << "Self recursion found" << std::endl;
    }
    add_edge(from_id, to_id, G);
  }

  // Find mutual recursive calls
  std::vector<int> c(num_vertices(G));
  int num = boost::strong_components(G,
    boost::make_iterator_property_map(c.begin(), boost::get(boost::vertex_index, G)));

  for (size_t i = 0; i < c.size(); ++i) {
    cct_groups[graph_index_reverse_converter[c[i]]].push_back(graph_index_reverse_converter[i]);
  }

  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << "CCT graph vertices " << cct_graph->size() << std::endl;
    std::cout << "Num vertices " << num_vertices(G) << std::endl;
    std::cout << "Find scc " << num << std::endl;
    for (size_t i = 0; i != c.size(); ++i) {
      std::cout << "Vertex " << i
        <<" is in component " << c[i] << std::endl;
    }
  }

  return num != static_cast<int>(cct_graph->size());
}


static void
mergeSCCNodes(CCTGraph *cct_graph, CCTGraph *old_cct_graph,
  std::unordered_map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > &cct_groups) {
  // Map node to group
  std::unordered_map<Prof::CCT::ANode *, Prof::CCT::ANode *> cct_group_reverse_map;
  for (auto it = cct_groups.begin(); it != cct_groups.end(); ++it) {
    auto &vec = it->second;
    auto *group = it->first;
    for (auto *n : vec) {
      cct_group_reverse_map[n] = group;
    }
  }

  // Add edges between SCCs
  for (auto it = cct_groups.begin(); it != cct_groups.end(); ++it) {
    Prof::CCT::ANode *scc_node = new Prof::CCT::SCC(NULL);
    auto &vec = it->second;
    for (auto *n : vec) {
      if (getProcStmt(n) != NULL) {
        // scc_node->proc
        cct_graph->addEdge(scc_node, n);
      } 
    }
    auto *group = it->first;
    for (auto eit = old_cct_graph->edgeBegin(); eit != old_cct_graph->edgeEnd(); ++eit) {
      // call->scc
      if (cct_group_reverse_map[eit->from] != group && getCudaCallStmt(eit->from) != NULL) {
        bool find = false;
        for (auto *n : vec) {
          if (eit->to == n) {
            find = true;
            break;
          }
        }
        if (find) {
          // from->scc_in_node
          cct_graph->addEdge(eit->from, scc_node);
        }
      } else if (cct_group_reverse_map[eit->to] != group && getCudaCallStmt(eit->to) != NULL) {
        // proc->call
        cct_graph->addEdge(eit->from, eit->to);
      }
    }
  }
}


static void
findGPURoots(Prof::CCT::ANode *prof_root, Prof::Struct::ANode *struct_root, std::set<Prof::CCT::ANode *> &gpu_roots) {
  std::set<Prof::Struct::ANode *> struct_procs;
  // 1. find all gpu procs in struct
  Prof::Struct::ANodeIterator struct_it(struct_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::Struct::ANode *n = NULL; (n = struct_it.current()); ++struct_it) {
    if (isCudaCallStmt(n)) {
      struct_procs.insert(n->ancestorProc());
    }
  }
  // 2. return CPU CCT nodes (GPU roots)
  Prof::CCT::ANodeIterator prof_it(prof_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
    if (struct_procs.find(n->structure()) != struct_procs.end()) {
      gpu_roots.insert(n->parent());
    }
  }
}


static void
gatherIncomingSamples(CCTGraph *cct_graph, IncomingInstMap &node_map, bool find_recursion) {
  // Gather call and scc samples
  for (auto it = cct_graph->nodeBegin(); it != cct_graph->nodeEnd(); ++it) {
    Prof::CCT::ANode *node = *it;
    if ((find_recursion && isSCCNode(node)) || (!find_recursion && getProcStmt(node) != NULL)) {
      if (cct_graph->incoming_nodes(node) != cct_graph->incoming_nodes_end()) {
        std::vector<Prof::CCT::ANode *> &vec = cct_graph->incoming_nodes(node)->second;
        for (auto *neighbor : vec) {
          for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
            node_map[node][neighbor][i] = neighbor->demandMetric(gpu_inst_index[i]);
          }
        }
      }
    }
  }
}


static void
constructCallingContext(CCTGraph *cct_graph, IncomingInstMap &incoming_samples) {
  // Init adjust factor for each thread
  AdjustFactor adjust_factor;
  for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
    adjust_factor[i] = 1.0;
  }

  for (auto it = cct_graph->nodeBegin(); it != cct_graph->nodeEnd(); ++it) {
    Prof::CCT::ANode *root = *it;
    // global functions, dynamic parallelism will invoke cpu function
    if (cct_graph->incoming_nodes_size(root) == 0) {
      if (isSCCNode(root)) {  // root is scc
        auto edge_iterator = cct_graph->outgoing_nodes(root);
        for (auto *n : edge_iterator->second) {
          if (cct_graph->outgoing_nodes_size(root) > 1 && OUTPUT_SCC_FRAME) {  // keep SCC frame
            auto *new_node = root->clone();
            auto *parent = n->parent();  
            new_node->link(parent);
            copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
          } else {
            auto *parent = n->parent();  
            copyPath(incoming_samples, cct_graph, n, parent, adjust_factor);
          }
        }
      } else {
        auto *parent = root->parent(); 
        copyPath(incoming_samples, cct_graph, root, parent, adjust_factor);
      }
      deleteTree(root);
    }
  }
  for (auto it = cct_graph->nodeBegin(); it != cct_graph->nodeEnd(); ++it) {
    Prof::CCT::ANode *node = *it;
    if (getProcStmt(node) != NULL) {
      node->unlink();
      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << "unlink: " << dynamic_cast<Prof::CCT::AProcNode *>(node)->procName() << std::endl;
      }
    }
  }
}


static void
copyPath(IncomingInstMap &incoming_samples,
  CCTGraph *cct_graph, 
  Prof::CCT::ANode *cur, Prof::CCT::ANode *prev,
  AdjustFactor adjust_factor) {
  bool isSCC = isSCCNode(cur);
  bool isCall = getCudaCallStmt(cur) == NULL ? false : true;
  // Copy node
  Prof::CCT::ANode *new_node;
  if (isSCC && SIMULATE_SCC_WITH_LOOP) {
    new_node = new Prof::CCT::Loop(prev);
  } else if (isCall) {
    new_node = new Prof::CCT::Call(prev, 0);
    new_node->structure(cur->structure());
    // A call node itself has metrics
    for (size_t i = 0; i < cur->numMetrics(); ++i) {
      new_node->demandMetric(i) = cur->demandMetric(i);
    }
  } else {
    new_node = cur->clone();
    new_node->link(prev);
  }
  
  // Adjust metrics
  for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
    size_t start = gpu_inst_index[i];
    size_t end = new_node->numMetrics();
    if (i != gpu_inst_index.size() - 1) {
      end = std::min(gpu_inst_index[i + 1], end);
    }
    for (size_t j = start; j < end; ++j) {
      new_node->demandMetric(j) *= adjust_factor[i];
    }
  }

  if (isCall) {
    // Case 1: Call node, skip to the procedure
    auto edge_iterator = cct_graph->outgoing_nodes(cur);
    if (edge_iterator != cct_graph->outgoing_nodes_end()) {  // some calls won't have outgoing edges
      for (auto *n : edge_iterator->second) { 
        if (DEBUG_CALLPATH_CUDACFG) {
          std::cout << std::endl;
          std::cout << "Before lay over a call: " << cur->id() << std::endl;
          debugGPUInst();
          std::vector<double> sum_samples(gpu_inst_index.size());
          for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
            for (auto &neighor : incoming_samples[n]) {
              sum_samples[i] += neighor.second[i];
            }
          }
          std::cout << "Adj: ";
          for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
            std::cout << adjust_factor[i] << " ";
          }
          std::cout << std::endl;
          std::cout << "Sum: ";
          for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
            std::cout << sum_samples[i] << " ";
          }
          std::cout << std::endl;
          std::cout << "Cur: ";
          for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
            std::cout << incoming_samples[n][cur][i] << " ";
          }
          std::cout << std::endl;
        }

        for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
          double sum_samples = 0.0;
          for (auto &neighor : incoming_samples[n]) {
            sum_samples += neighor.second[i];
          }
          double cur_samples = incoming_samples[n][cur][i];
          if (sum_samples != 0) {
            adjust_factor[i] *= cur_samples / sum_samples;
          }
        }

        if (DEBUG_CALLPATH_CUDACFG) {
          std::cout << "After lay over a call: " << cur->id() << std::endl;
          debugGPUInst();
          std::cout << "Adj: ";
          for (size_t i = 0; i < gpu_inst_index.size(); ++i) {
            std::cout << adjust_factor[i] << " ";
          }
          std::cout << std::endl;
        }
        copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
      }
    }
  } else if (isSCC) {
    // Case 2: SCC node
    auto edge_iterator = cct_graph->outgoing_nodes(cur);
    for (auto *n : edge_iterator->second) {
      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << std::endl;
        std::cout << "Lay over a SCC: " << cur->id() << std::endl;
      }
      // Keep scc in frame
      if (cct_graph->outgoing_nodes_size(cur) > 1 && OUTPUT_SCC_FRAME) {
        copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
      } else {
        copyPath(incoming_samples, cct_graph, n, prev, adjust_factor);
      }
    }
  } else if (!cur->isLeaf()) {
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << std::endl;
      std::cout << "Not a call or SCC: " << cur->id() << std::endl;
    }
    // Case 3: Iterate through children
    Prof::CCT::ANodeChildIterator it(cur, NULL/*filter*/);
    for (Prof::CCT::ANode *n = NULL; (n = it.current()); ++it) {
      copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
    }
  } else {
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << "Leaf node" << std::endl;
    }
  }
}


static void
deleteTree(Prof::CCT::ANode *prof_root) {
  // TODO(keren): delete the original structure to reduce memory consumption
}


} // namespace CallPath

} // namespace Analysis
