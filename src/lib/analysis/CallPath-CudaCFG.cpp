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
#include <unordered_map>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <boost/graph/strong_components.hpp>
#include <boost/graph/adjacency_list.hpp>

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

#include <vector>
#include <queue>
#include <iostream>


#define DEBUG_CALLPATH_CUDACFG 0
#define OUTPUT_SCC_FRAME 1
#define SIMULATE_SCC_WITH_LOOP 1


namespace Analysis {

namespace CallPath {

typedef std::map<VMA, std::set<Prof::Struct::ANode *> > StructCallMap;

typedef std::map<VMA, std::vector<Prof::CCT::ANode *> > ProfCallMap;

typedef std::map<Prof::Struct::ANode *, Prof::CCT::ANode *> StructProfMap;

typedef std::map<VMA, Prof::CCT::ANode *> ProfProcMap;

typedef std::map<Prof::CCT::ANode *, std::map<Prof::CCT::ANode *, double> > IncomingSamplesMap;

static std::vector<size_t> gpu_inst_index;

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
gatherIncomingSamples(CCTGraph *cct_graph, IncomingSamplesMap &node_map, bool find_recursion);

static void
constructCallingContext(CCTGraph *cct_graph, IncomingSamplesMap &incoming_samples);

static void
copyPath(IncomingSamplesMap &incoming_samples,
  CCTGraph *cct_graph,
  Prof::CCT::ANode *cur, 
  Prof::CCT::ANode *prev,
  double adjust_factor);

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
    std::cout << std::dec << it->from->id()  << from << "->" << it->to->id() << to << std::endl;
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
        " (" << stmt->vmaSet().begin()->beg() << ") called from " <<
        dynamic_cast<Prof::Struct::ACodeNode *>(p_stmt)->begLine() <<
        " node " << n->parent() <<
      std::endl;
    }
  }
}


void
transformCudaCFGMain(Prof::CallPath::Profile& prof) {
  // Check if prof contains gpu metrics
  auto *mgr = prof.metricMgr(); 
  for (size_t i = 0; i < mgr->size(); ++i) {
    if (mgr->metric(i)->namePfxBase() == "GPU INST") {
      gpu_inst_index.push_back(i);
    }
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << "Metric " << i << ":" << mgr->metric(i)->name() << std::endl;
    }
  }
  // Skip non-gpu prof
  if (gpu_inst_index.size() == 0) {
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << "Skip non-gpu prof" << std::endl;
    }
    return;
  }

  // Find the parents of gpu global functions
  std::set<Prof::CCT::ANode *> gpu_roots;
  findGPURoots(prof.cct()->root(), prof.structure()->root(), gpu_roots);

  // find <target_vma, <Struct::Stmt> > mappings
  StructCallMap struct_call_map; 
  constructStructCallMap(prof.structure()->root(), struct_call_map);

  if (DEBUG_CALLPATH_CUDACFG) {
    std::cout << "Number of gpu roots: " << gpu_roots.size() << std::endl;
  }

  for (auto *gpu_root : gpu_roots) {
    if (DEBUG_CALLPATH_CUDACFG) {
      debugProfTree(gpu_root);
    }

    // Construct a call map for a target
    CCTGraph *cct_graph = new CCTGraph();
    constructCallGraph(gpu_root, cct_graph, struct_call_map);

    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << "Call graph before merging SCCs" << std::endl;
      debugCallGraph(cct_graph);
    }

    // Handle recursive calls
    std::unordered_map<Prof::CCT::ANode *, std::vector<Prof::CCT::ANode *> > cct_groups;
    bool find_recursion = false;
    if (findRecursion(cct_graph, cct_groups)) {
      find_recursion = true;
      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << "Find recursive calls" << std::endl;
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
    IncomingSamplesMap incoming_samples;
    gatherIncomingSamples(cct_graph, incoming_samples, find_recursion);

    // Copy from every gpu_root to leafs
    if (DEBUG_CALLPATH_CUDACFG) {
      std::cout << "Construct calling context tree" << std::endl;
    }
    constructCallingContext(cct_graph, incoming_samples);

    delete cct_graph;
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
  // Step1: Iterate over all proc nodes in the prof
  // TODO(Keren): multiple global function calls under a single target
  StructProfMap struct_prof_map;  // <Struct, CCT>
  ProfCallMap prof_call_map;  // <target_vma, [CCT::Call]>
  ProfProcMap prof_proc_map;  // <proc_vma, CCT::ProcFrm>
  Prof::CCT::ANodeIterator prof_it(prof_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
    if (getCudaCallStmt(n) != NULL) {  // call
      auto *stmt = getCudaCallStmt(n);
      prof_call_map[stmt->target()].push_back(n);
    } else if (getProcStmt(n) != NULL) {  // proc
      auto *stmt = getProcStmt(n);
      auto vma = stmt->vmaSet().begin()->beg();
      prof_proc_map[vma] = n;
    }
    if (n->structure() != NULL) { 
      struct_prof_map[n->structure()] = n;
    }
  }

  // Step2: Add call->proc
  std::queue<std::pair<VMA, Prof::CCT::ANode *> > prof_procs;
  for (auto &entry : prof_proc_map) {
    prof_procs.push(entry);
  }

  while (prof_procs.empty() == false) {
    auto &entry = prof_procs.front();
    prof_procs.pop();
    auto vma = entry.first;
    for (auto *struct_call : struct_call_map[vma]) {
      Prof::CCT::ANode *prof_call = NULL;
      if (prof_call_map.find(vma) != prof_call_map.end()) {
        for (auto *call : prof_call_map[vma]) {
          if (call->structure() == struct_call) {
            prof_call = call;
            break;
          }
        }
      }
      // If the call node has not been sampled, we manually assign it sample one
      if (prof_call == NULL) {
        // Create a scope frame
        Prof::Struct::ANode *scope = struct_call->ancestor(Prof::Struct::ANode::TyLoop,
          Prof::Struct::ANode::TyAlien,
          Prof::Struct::ANode::TyProc);
        // Ancester does not exist
        if (struct_prof_map.find(scope) == struct_prof_map.end()) {
          auto *struct_proc = struct_call->ancestorProc();
          // Update prof->proc queue and map
          auto vma = struct_proc->vmaSet().begin()->beg();
          auto *frm_proc = new Prof::CCT::ProcFrm(NULL, struct_proc);
          std::pair<VMA, Prof::CCT::ANode *> new_entry(vma, frm_proc);
          // Add new entry
          prof_procs.push(new_entry);
          // Add vma->proc
          prof_proc_map[vma] = frm_proc;
          // Add struct->proc
          struct_prof_map[struct_proc] = frm_proc;
          constructFrame(struct_proc, frm_proc, frm_proc, struct_prof_map); 
        }
        // Now the corresponding prof node exists
        Prof::CCT::ANode *frm_scope = struct_prof_map[scope];
        prof_call = new Prof::CCT::Call(frm_scope, 0);
        auto *struct_code = dynamic_cast<Prof::Struct::ACodeNode *>(struct_call);
        prof_call->structure(struct_code);
        // Add a gpu_isample
        prof_call->demandMetric(gpu_inst_index[0]) = 1.0;
        // Add to prof_call_map
        prof_call_map[vma].push_back(prof_call);
      }
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

  // Step3: Add proc->call
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
      std::cout << "Self recursion" << std::endl;
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


void
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
gatherIncomingSamples(CCTGraph *cct_graph, IncomingSamplesMap &node_map, bool find_recursion) {
  // Gather call and scc samples
  for (auto it = cct_graph->nodeBegin(); it != cct_graph->nodeEnd(); ++it) {
    Prof::CCT::ANode *node = *it;
    if ((find_recursion && isSCCNode(node)) || (!find_recursion && getProcStmt(node) != NULL)) {
      if (cct_graph->incoming_nodes(node) != cct_graph->incoming_nodes_end()) {
        std::vector<Prof::CCT::ANode *> &vec = cct_graph->incoming_nodes(node)->second;
        if (gpu_inst_index.size() == 0) {
          for (auto *neighbor : vec) {
            // By default, set it to one
            node_map[node][neighbor] = 1.0;
          }
        } else {
          for (auto *neighbor : vec) {
            node_map[node][neighbor] = std::max(neighbor->demandMetric(gpu_inst_index[0]), 1.0);
          }
        }
      }
    }
  }
}


static void
constructCallingContext(CCTGraph *cct_graph, IncomingSamplesMap &incoming_samples) {
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
            copyPath(incoming_samples, cct_graph, n, new_node, 1.0);
          } else {
            auto *parent = n->parent();  
            copyPath(incoming_samples, cct_graph, n, parent, 1.0);
          }
        }
      } else {
        auto *parent = root->parent(); 
        copyPath(incoming_samples, cct_graph, root, parent, 1.0);
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
copyPath(IncomingSamplesMap &incoming_samples,
  CCTGraph *cct_graph, 
  Prof::CCT::ANode *cur, Prof::CCT::ANode *prev,
  double adjust_factor) {
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
  for (size_t i = 0; i < new_node->numMetrics(); ++i) {
    new_node->metric(i) *= adjust_factor;
  }

  if (isCall) {
    // Case 1: Call node, skip to the procedure
    auto edge_iterator = cct_graph->outgoing_nodes(cur);
    if (edge_iterator != cct_graph->outgoing_nodes_end()) {  // some calls won't have outgoing edges
      for (auto *n : edge_iterator->second) {
        double sum_samples = 0.0;
        for (auto &neighor : incoming_samples[n]) {
          sum_samples += neighor.second;
        }
        double cur_samples = incoming_samples[n][cur];
        adjust_factor *= cur_samples / sum_samples;
        if (DEBUG_CALLPATH_CUDACFG) {
          std::cout << "Lay over a call" << std::endl;
        }
        copyPath(incoming_samples, cct_graph, n, new_node, adjust_factor);
      }
    }
  } else if (isSCC) {
    // Case 2: SCC node
    auto edge_iterator = cct_graph->outgoing_nodes(cur);
    for (auto *n : edge_iterator->second) {
      if (DEBUG_CALLPATH_CUDACFG) {
        std::cout << "Lay over a SCC" << std::endl;
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
      std::cout << "Not a call or SCC" << std::endl;
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
