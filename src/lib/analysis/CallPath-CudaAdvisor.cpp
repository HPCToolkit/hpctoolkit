//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>
#include <cstdio>

#include <typeinfo>
#include <unordered_map>
#include <algorithm>
#include <queue>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>

#include "CallPath-CudaAdvisor.hpp"
#include "MetricNameProfMap.hpp"
#include "CCTGraph.hpp"

using std::string;

#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Struct-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Metric-ADesc.hpp>

#include <lib/profxml/XercesUtil.hpp>
#include <lib/profxml/PGMReader.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/cuda/DotCFG.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>

#define DEBUG_CALLPATH_CUDAADVISOR 1

namespace Analysis {

namespace CallPath {

void CudaAdvisor::init() {
  if (_inst_stall_metrics.size() != 0) {
    // Init already
    return;
  }

  // init individual metrics
  _issue_metric = "STALL:NONE";
  _inst_metric = "GPU INST";

  _invalid_stall_metric = "STALL:INVALID";
  _tex_stall_metric = "STALL:TEX";
  _ifetch_stall_metric = "STALL:IFETCH";
  _pipe_bsy_stall_metric = "STALL:PIPE_BSY";
  _mem_thr_stall_metric = "STALL:MEM_THR";
  _nosel_stall_metric = "STALL:NOSEL";
  _other_stall_metric = "STALL:OTHER";
  _sleep_stall_metric = "STALL:SLEEP";
  _cmem_stall_metric = "STALL:CMEM_DEP";
  
  _inst_stall_metrics.insert(_invalid_stall_metric);
  _inst_stall_metrics.insert(_tex_stall_metric);
  _inst_stall_metrics.insert(_ifetch_stall_metric);
  _inst_stall_metrics.insert(_pipe_bsy_stall_metric);
  _inst_stall_metrics.insert(_mem_thr_stall_metric);
  _inst_stall_metrics.insert(_nosel_stall_metric);
  _inst_stall_metrics.insert(_other_stall_metric);
  _inst_stall_metrics.insert(_sleep_stall_metric);
  _dep_stall_metrics.insert(_cmem_stall_metric);

  _exec_dep_stall_metric = "STALL:EXC_DEP";
  _mem_dep_stall_metric = "STALL:MEM_DEP";
  _sync_stall_metric = "STALL:SYNC";

  _dep_stall_metrics.insert(_exec_dep_stall_metric);
  _dep_stall_metrics.insert(_mem_dep_stall_metric);
  _dep_stall_metrics.insert(_sync_stall_metric);

  for (auto &s : _inst_stall_metrics) {
    _metric_name_prof_map->add("BLAME " + s);
  }

  for (auto &s : _dep_stall_metrics) {
    _metric_name_prof_map->add("BLAME " + s);
  }
}


void CudaAdvisor::constructVMAProfMap(Prof::LoadMap::LMId_t lm_id, VMAProfMap &vma_prof_map,
  VMAInstMap &vma_inst_map) {
  auto *prof_root = _prof->cct()->root();
  // Only iterate instruction nodes
  Prof::CCT::ANodeIterator prof_it(prof_root, NULL/*filter*/, true/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
    Prof::CCT::ADynNode* n_dyn = dynamic_cast<Prof::CCT::ADynNode*>(n);
    if (n_dyn) {
      Prof::LoadMap::LMId_t n_lm_id = n_dyn->lmId(); // ok if LoadMap::LMId_NULL
      VMA n_lm_ip = n_dyn->lmIP();
      // filter out
      if (n_lm_id != lm_id || vma_inst_map.find(n_lm_ip) == vma_inst_map.end()) {
        continue;
      }

      vma_prof_map[n_lm_ip] = n_dyn;

      if (DEBUG_CALLPATH_CUDAADVISOR) {
        std::cout << "Find CCT node vma: 0x" << std::hex << n_lm_ip << std::dec << std::endl;
      }
    }
  }
}


void CudaAdvisor::constructVMAStructMap(VMAStructMap &vma_struct_map) {
  auto *struct_root = _prof->structure()->root();
  Prof::Struct::ANodeIterator struct_it(struct_root, NULL/*filter*/, true/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::Struct::ANode *n = NULL; (n = struct_it.current()); ++struct_it) {
    if (n->type() == Prof::Struct::ANode::TyStmt) {
      auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(n);
      for (auto &vma_interval : stmt->vmaSet()) {
        vma_struct_map[vma_interval] = n;

        if (DEBUG_CALLPATH_CUDAADVISOR) {
          std::cout << "Find Struct node vma: [0x" << std::hex << vma_interval.beg() << ", 0x" <<
           vma_interval.end() << "]" << std::dec << std::endl;
        }
      }
    }
  }
}


void CudaAdvisor::constructVMAInstMap(std::vector<CudaParse::InstructionStat *> &inst_stats,
  VMAInstMap &vma_inst_map) {
  for (auto *inst_stat : inst_stats) {
    auto vma = inst_stat->pc;
    vma_inst_map[vma] = inst_stat;

    if (DEBUG_CALLPATH_CUDAADVISOR) {
      std::cout << "Find Inst vma: 0x" << std::hex << vma << std::dec << std::endl;
    }
  }
}


void CudaAdvisor::initCCTGraph(std::vector<CudaParse::InstructionStat *> &inst_stats,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph, VMAProfMap &vma_prof_map) {
  for (auto *inst_stat : inst_stats) {
    auto vma = inst_stat->pc;
    auto iter = vma_prof_map.find(vma);
    if (iter == vma_prof_map.end()) {
      continue;
    }
    cct_dep_graph.addNode(iter->second);
  }
}


void CudaAdvisor::initInstGraph(std::vector<CudaParse::InstructionStat *> &inst_stats,
  CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph, VMAInstMap &vma_inst_map) {
  for (auto *inst_stat : inst_stats) {
    inst_dep_graph.addNode(inst_stat);
    for (auto &iter : inst_stat->assign_pcs) {
      for (auto pc : iter.second) {
        auto *dep_inst = vma_inst_map[pc];
        inst_dep_graph.addEdge(inst_stat, dep_inst);
      }
    }
  }
}


void CudaAdvisor::updateCCTGraph(Prof::LoadMap::LMId_t lm_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
  CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph, VMAProfMap &vma_prof_map,
  VMAStructMap &vma_struct_map, VMAInstMap &vma_inst_map) {
  auto in_inst_metric_ids = _metric_name_prof_map->metric_ids(_inst_metric, true);
  auto ex_inst_metric_ids = _metric_name_prof_map->metric_ids(_inst_metric, false);
  auto in_issue_metric_ids = _metric_name_prof_map->metric_ids(_issue_metric, true);
  auto ex_issue_metric_ids = _metric_name_prof_map->metric_ids(_issue_metric, false);

  // For each thread O(T)
  for (size_t i = 0; i < in_issue_metric_ids.size(); ++i) {
    // Init queue
    std::queue<Prof::CCT::ADynNode *> nodes;
    for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
      auto *node = *iter;
      if (node->demandMetric(in_issue_metric_ids[i]) != 0.0) {
        nodes.push(node);
      }
    }

    // Complexity O(N)
    while (nodes.empty() == false) {
      auto *node = nodes.front();
      nodes.pop();
      auto *parent = node->parent();

      auto node_vma = node->lmIP();
      // Must have a correponding instruction
      auto *node_inst = vma_inst_map[node_vma];
      auto inst_iter = inst_dep_graph.outgoing_nodes(node_inst);

      if (inst_iter != inst_dep_graph.outgoing_nodes_end()) {
        for (auto *inst_stat : inst_iter->second) {
          auto vma = inst_stat->pc;
          auto iter = vma_prof_map.find(vma);
          if (iter != vma_prof_map.end()) {
            // Existed CCT node
            auto *neighbor_node = iter->second;
            cct_dep_graph.addEdge(node, neighbor_node);

          } else {
            Prof::Metric::IData metric_data(_prof->metricMgr()->size());
            metric_data.clearMetrics();
            auto *neighbor_node = new Prof::CCT::Stmt(parent,
              HPCRUN_FMT_CCTNodeId_NULL, lush_assoc_info_NULL, lm_id, vma, 0, NULL, metric_data);

            cct_dep_graph.addEdge(node, neighbor_node);
            vma_prof_map[vma] = neighbor_node;
            nodes.push(neighbor_node);
          }
        }
      }
    }
  }
}


int CudaAdvisor::demandNodeMetrics(int mpi_rank, int thread_id, Prof::CCT::ADynNode *node) {
  auto in_issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric, true);
  auto ex_issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric, false);
  auto in_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, true);
  auto ex_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, false);

  int ret = node->demandMetric(in_issue_metric_index);
  if (ret == 0) {
    node->demandMetric(in_issue_metric_index) += 1;
    node->demandMetric(ex_issue_metric_index) += 1;
    node->demandMetric(in_inst_metric_index) += 1;
    node->demandMetric(ex_inst_metric_index) += 1;
    ret = 1;
  }
  return ret;
}


void CudaAdvisor::blameCCTGraph(CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph, VMAInstMap &vma_inst_map) {
  // for each thread
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks(); ++mpi_rank) {
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank); ++thread_id) {
      for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
        auto *node = *iter;
        auto niter = cct_dep_graph.outgoing_nodes(node);

        std::map<std::string, double> sum;
        if (niter != cct_dep_graph.outgoing_nodes_end()) {
          for (auto *dep_node : niter->second) {
            auto vma = dep_node->lmIP();
            auto *inst_stat = vma_inst_map[vma];

            // sum up all neighbor node's instructions
            if (inst_stat->op.find("MEMORY") != std::string::npos) {
              if (inst_stat->op.find("GLOBAL") != std::string::npos) {
                sum[_mem_dep_stall_metric] += demandNodeMetrics(mpi_rank, thread_id, dep_node);
              } else if (inst_stat->op.find("LOCAL") != std::string::npos) {
                sum[_mem_dep_stall_metric] += demandNodeMetrics(mpi_rank, thread_id, dep_node);
              } else {
                sum[_exec_dep_stall_metric] += demandNodeMetrics(mpi_rank, thread_id, dep_node);
              }
            } else {
              sum[_exec_dep_stall_metric] += demandNodeMetrics(mpi_rank, thread_id, dep_node);
            }
          }
        }

        if (niter != cct_dep_graph.outgoing_nodes_end()) {
          for (auto *dep_node : niter->second) {
            auto vma = dep_node->lmIP();
            auto *inst_stat = vma_inst_map[vma];
            std::string latency_metric;
            if (inst_stat->op.find("MEMORY") != std::string::npos) {
              if (inst_stat->op.find("GLOBAL") != std::string::npos) {
                latency_metric = _mem_dep_stall_metric;
              } else if (inst_stat->op.find("LOCAL") != std::string::npos) {
                latency_metric = _mem_dep_stall_metric;
              } else {
                latency_metric = _exec_dep_stall_metric;
              }
            } else {
              latency_metric = _exec_dep_stall_metric;
            }

            auto latency_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, latency_metric);
            auto latency = node->demandMetric(latency_metric_index);
            if (latency_metric.size() != 0 && latency != 0) {
              double div = sum[latency_metric];
              auto issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric);
              auto neighbor_ratio = dep_node->demandMetric(issue_metric_index) / div;
              // inclusive and exclusive metrics have the same value
              auto in_blame_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, "BLAME " + latency_metric, true);
              auto ex_blame_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, "BLAME " + latency_metric, false);
              // blame dep_node
              dep_node->demandMetric(in_blame_metric_index) += latency * neighbor_ratio;
              dep_node->demandMetric(ex_blame_metric_index) += latency * neighbor_ratio;
            }
          }
        }

        for (auto &s : _inst_stall_metrics) {
          auto stall_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, s);
          auto stall = node->demandMetric(stall_metric_index);
          if (stall == 0) {
            continue;
          }
          auto in_blame_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, "BLAME " + s, true);
          auto ex_blame_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, "BLAME " + s, false);
          // inclusive and exclusive metrics have the same value
          // blame itself
          node->demandMetric(in_blame_metric_index) += stall;
          node->demandMetric(ex_blame_metric_index) += stall;
        }
      }
    }
  }
}


void CudaAdvisor::debugCCTDepGraph(CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  std::cout << "Nodes (" << cct_dep_graph.size() << "):" << std::endl;
  for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
    auto *node = *it;
    auto node_vma = node->lmIP();

    std::cout << std::hex << "0x" << node_vma << std::dec << std::endl;
  }

  std::cout << "Edges:" << std::endl;
  for (auto it = cct_dep_graph.edgeBegin(); it != cct_dep_graph.edgeEnd(); ++it) {
    auto *from_node = it->from;
    auto *to_node = it->to;

    auto from_vma = from_node->lmIP();
    auto to_vma = to_node->lmIP();

    std::cout << std::hex << "0x" << from_vma << "-> 0x" << to_vma << std::dec << std::endl;
  }
}


void CudaAdvisor::blame(Prof::LoadMap::LMId_t lm_id, std::vector<CudaParse::InstructionStat *> &inst_stats) {
  // 1. Map pc to CCT and structs
  VMAInstMap vma_inst_map;
  constructVMAInstMap(inst_stats, vma_inst_map);

  VMAProfMap vma_prof_map;
  constructVMAProfMap(lm_id, vma_prof_map, vma_inst_map);

  VMAStructMap vma_struct_map;
  constructVMAStructMap(vma_struct_map);

  // 2. Init a dependency graph with struct nodes
  // a->b a depends on b
  CCTGraph<CudaParse::InstructionStat *> inst_dep_graph;
  initInstGraph(inst_stats, inst_dep_graph, vma_inst_map);

  // 3.1 Init a graph with CCTs
  CCTGraph<Prof::CCT::ADynNode *> cct_dep_graph;
  initCCTGraph(inst_stats, cct_dep_graph, vma_prof_map);

  if (DEBUG_CALLPATH_CUDAADVISOR) {
    std::cout << "CCT dependency graph before propgate: " << std::endl;
    debugCCTDepGraph(cct_dep_graph);
    std::cout << std::endl;
  }

  // 3.2 Iterative update CCT graph
  updateCCTGraph(lm_id, cct_dep_graph, inst_dep_graph,
    vma_prof_map, vma_struct_map, vma_inst_map);

  if (DEBUG_CALLPATH_CUDAADVISOR) {
    std::cout << "CCT dependency graph after propgate: " << std::endl;
    debugCCTDepGraph(cct_dep_graph);
    std::cout << std::endl;
  }

  // 4. accumulate blames
  blameCCTGraph(cct_dep_graph, vma_inst_map);

  // 5. find bottlenecks
}


void CudaAdvisor::advise(Prof::LoadMap::LMId_t lm_id) {
}


void CudaAdvisor::save(const std::string &file_name) {
}

}  // namespace CallPath

}  // namespace Analysis
