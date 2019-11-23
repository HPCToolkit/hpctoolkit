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

namespace Analysis {

namespace CallPath {


void CudaAdvisor::init() {
  // Init already
  if (_metric_name_prof_maps.size() != 0) {
    return ;
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
  
  _inst_stall_metrics.insert(_invalid_stall_metric);
  _inst_stall_metrics.insert(_tex_stall_metric);
  _inst_stall_metrics.insert(_ifetch_stall_metric);
  _inst_stall_metrics.insert(_pipe_bsy_stall_metric);
  _inst_stall_metrics.insert(_mem_thr_stall_metric);
  _inst_stall_metrics.insert(_nosel_stall_metric);
  _inst_stall_metrics.insert(_other_stall_metric);
  _inst_stall_metrics.insert(_sleep_stall_metric);

  _exec_dep_stall_metric = "STALL:EXC_DEP";
  _mem_dep_stall_metric = "STALL:MEM_DEP";
  _cmem_dep_stall_metric = "STALL:CMEM_DEP";
  _sync_stall_metric = "STALL:SYNC";

  _dep_stall_metrics.insert(_exec_dep_stall_metric);
  _dep_stall_metrics.insert(_mem_dep_stall_metric);
  _dep_stall_metrics.insert(_cmem_dep_stall_metric);
  _dep_stall_metrics.insert(_sync_stall_metric);

  // Map metric names to index
  auto *mgr = _prof->metricMgr(); 
  std::map<std::string, std::pair<size_t, size_t> > metric_name_prof_map;

  for (size_t i = 0; i < mgr->size(); ++i) {
    auto prefix = mgr->metric(i)->namePfxBase();
    if (_inst_stall_metrics.find(prefix) == _inst_stall_metrics.end() &&
       _dep_stall_metrics.find(prefix) == _dep_stall_metrics.end() &&
      _issue_metric != prefix && _inst_metric != prefix) {
      continue;
    }

    if (mgr->metric(i)->type() == Prof::Metric::ADesc::TyIncl) {
      // Assume exclusive metrics index is i+1
      std::pair<size_t, size_t> metric_ids(i, i + 1);
      metric_name_prof_map[prefix] = metric_ids;
    }
    if (prefix == _sleep_stall_metric) {
      // The end of the current CPU thread metrics
      _metric_name_prof_maps.push_back(metric_name_prof_map);
      metric_name_prof_map.clear();
    }
  }

  // Create new metrics and mappings
  // TODO(Keren): fix multiple CPU threads
  for (auto &metric_name_prof_map : _metric_name_prof_maps) {
    std::map<std::string, std::pair<size_t, size_t> > new_metric_name_prof_map;

    for (auto &iter : metric_name_prof_map) {
      auto metric_name = "BLAME " + iter.first;
      auto *in_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
      auto *ex_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
      in_desc->type(Prof::Metric::ADesc::TyIncl);
      ex_desc->type(Prof::Metric::ADesc::TyExcl);
      in_desc->partner(ex_desc);
      ex_desc->partner(in_desc);

      if (mgr->insertIf(in_desc) && mgr->insertIf(ex_desc)) {
        auto in_id = in_desc->id();
        auto ex_id = ex_desc->id();
        new_metric_name_prof_map[metric_name] = std::pair<size_t, size_t>(in_id, ex_id);
      }
    }

    for (auto &iter : new_metric_name_prof_map) {
      auto &metric_name = iter.first;
      auto &metric_ids = iter.second;
      metric_name_prof_map[metric_name] = metric_ids;
    }
  }
}


void CudaAdvisor::constructVMAProfMap(Prof::LoadMap::LMId_t lm_id,
  std::vector<CudaParse::InstructionStat *> &inst_stats,
  std::map<VMA, Prof::CCT::ANode *> &vma_prof_map) {
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
      if (n_lm_id != lm_id) {
        continue;
      }

      vma_prof_map[n_lm_ip] = n;
    }
  }
}


void CudaAdvisor::constructVMAStructMap(std::vector<CudaParse::InstructionStat *> &inst_stats,
  VMAStructMap &vma_struct_map) {
  auto *struct_root = _prof->structure()->root();
  Prof::Struct::ANodeIterator struct_it(struct_root, NULL/*filter*/, true/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::Struct::ANode *n = NULL; (n = struct_it.current()); ++struct_it) {
    auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(n);
    auto vma = stmt->vmaSet().begin()->beg();
    vma_struct_map[vma] = n;
  }
}


void CudaAdvisor::initCCTGraph(std::vector<CudaParse::InstructionStat *> &inst_stats,
  CCTGraph<Prof::CCT::ANode *> &cct_dep_graph, VMAProfMap &vma_prof_map) {
  for (auto *inst_stat : inst_stats) {
    auto vma = inst_stat->pc;
    auto iter = vma_prof_map.find(vma);
    if (iter == vma_prof_map.end()) {
      continue;
    }
    cct_dep_graph.addNode(iter->second);
  }
}


void CudaAdvisor::initStructGraph(std::vector<CudaParse::InstructionStat *> &inst_stats,
  CCTGraph<Prof::Struct::ANode *> &struct_dep_graph, VMAStructMap &vma_struct_map) {
  for (auto *inst_stat : inst_stats) {
    auto vma = inst_stat->pc;
    auto *node = vma_struct_map[vma]; 
    if (node) {
      struct_dep_graph.addNode(node);
      for (auto &iter : inst_stat->assign_pcs) {
        for (auto pc : iter.second) {
          auto *dep_node = vma_struct_map[pc];
          struct_dep_graph.addEdge(node, dep_node);
        }
      }
    }
  }
}


void CudaAdvisor::updateCCTGraph(CCTGraph<Prof::CCT::ANode *> &cct_dep_graph,
  CCTGraph<Prof::Struct::ANode *> &struct_dep_graph, VMAProfMap &vma_prof_map,
  VMAStructMap &vma_struct_map) {
  // For each thread O(T)
  for (auto &metric_name_prof_map : _metric_name_prof_maps) {
    // Init queue
    std::queue<Prof::CCT::ANode *> nodes;
    for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
      auto *node = *iter;
      if (node->demandMetric(metric_name_prof_map[_inst_metric].first) != 0.0) {
        nodes.push(node);
      }
    }

    // Complexity O(N)
    while (nodes.empty() == false) {
      auto *node = nodes.front();
      nodes.pop();
      auto *parent = node->parent();

      bool find = false;
      auto *strct = node->structure();
      auto niter = struct_dep_graph.outgoing_nodes(strct);
      if (niter != struct_dep_graph.outgoing_nodes_end()) {
        for (auto *nstrct : niter->second) {
          auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(nstrct);
          auto vma = stmt->vmaSet().begin()->beg();

          auto iter = vma_prof_map.find(vma);
          if (iter != vma_prof_map.end()) {
            // Existed CCT node
            auto *neighbor_node = iter->second;
            cct_dep_graph.addEdge(node, neighbor_node);
            if (neighbor_node->demandMetric(metric_name_prof_map[_issue_metric].first) != 0.0) {
              neighbor_node->demandMetric(metric_name_prof_map[_issue_metric].first) = 1.0;
              neighbor_node->demandMetric(metric_name_prof_map[_issue_metric].second) = 1.0;
              neighbor_node->demandMetric(metric_name_prof_map[_inst_metric].first) += 1.0;
              neighbor_node->demandMetric(metric_name_prof_map[_inst_metric].second) += 1.0;
              find = true;
            }
          }
        }
      }

      if (!find) {
        auto niter = struct_dep_graph.outgoing_nodes(strct);
        if (niter != struct_dep_graph.outgoing_nodes_end()) {
          for (auto *nstrct : niter->second) {
            // New CCT nodes
            auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(nstrct);
            auto *neighbor_node = new Prof::CCT::Stmt(parent, 0);
            neighbor_node->structure(stmt);
            neighbor_node->demandMetric(metric_name_prof_map[_issue_metric].first) = 1.0;
            neighbor_node->demandMetric(metric_name_prof_map[_issue_metric].second) = 1.0;
            neighbor_node->demandMetric(metric_name_prof_map[_inst_metric].first) += 1.0;
            neighbor_node->demandMetric(metric_name_prof_map[_inst_metric].second) += 1.0;

            cct_dep_graph.addEdge(node, neighbor_node);
            auto vma = stmt->vmaSet().begin()->beg();
            vma_prof_map[vma] = neighbor_node;
            nodes.push(neighbor_node);
          }
        }
      } 
    }
  }
}


void CudaAdvisor::blameCCTGraph(CCTGraph<Prof::CCT::ANode *> &cct_dep_graph, VMAInstMap &vma_inst_map) {
  // for each thread
  for (auto &metric_name_prof_map : _metric_name_prof_maps) {
    for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
      auto *node = *iter;
      auto niter = cct_dep_graph.outgoing_nodes(node);

      std::map<std::string, double> sum;
      if (niter != cct_dep_graph.outgoing_nodes_end()) {
        for (auto *dep_node : niter->second) {
          auto *strct = dep_node->structure();
          auto vma = strct->vmaSet().begin()->beg();
          auto *inst_stat = vma_inst_map[vma];
          auto metrics_index = metric_name_prof_map[_issue_metric];

          // sum up all neighbor node's instructions
          if (inst_stat->op.find("MEMORY") != std::string::npos) {
            if (inst_stat->op.find("CONSTANT") != std::string::npos) {
              sum[_cmem_dep_stall_metric] += dep_node->demandMetric(metrics_index.first);
            } else if (inst_stat->op.find("GLOBAL") != std::string::npos) {
              sum[_mem_dep_stall_metric] += dep_node->demandMetric(metrics_index.first);
            } else if (inst_stat->op.find("LOCAL") != std::string::npos) {
              sum[_mem_dep_stall_metric] += dep_node->demandMetric(metrics_index.first);
            } else {
              sum[_exec_dep_stall_metric] += dep_node->demandMetric(metrics_index.first);
            }
          } else {
            sum[_exec_dep_stall_metric] += dep_node->demandMetric(metrics_index.first);
          }
        }
      }

      if (niter != cct_dep_graph.outgoing_nodes_end()) {
        for (auto *dep_node : niter->second) {
          auto *strct = dep_node->structure();
          auto vma = strct->vmaSet().begin()->beg();
          auto *inst_stat = vma_inst_map[vma];
          std::string latency_metric;
          if (inst_stat->op.find("MEMORY") != std::string::npos) {
            if (inst_stat->op.find("CONSTANT") != std::string::npos) {
              latency_metric = _cmem_dep_stall_metric;
            } else if (inst_stat->op.find("GLOBAL") != std::string::npos) {
              latency_metric = _mem_dep_stall_metric;
            } else if (inst_stat->op.find("LOCAL") != std::string::npos) {
              latency_metric = _mem_dep_stall_metric;
            } else {
              latency_metric = _exec_dep_stall_metric;
            }
          } else {
            latency_metric = _exec_dep_stall_metric;
          }

          if (latency_metric.size() != 0) {
            double div = sum[latency_metric];
            auto issue_metrics_index = metric_name_prof_map[_issue_metric];
            auto neighbor_ratio = dep_node->demandMetric(issue_metrics_index.first) / div;
            auto metric_index = metric_name_prof_map[latency_metric];
            auto latency = node->demandMetric(metric_index.first);
            auto blame_metrics_index = metric_name_prof_map["BLAME " + latency_metric];
            // inclusive and exclusive metrics have the same value
            // blame dep_node
            dep_node->demandMetric(blame_metrics_index.first) += latency * neighbor_ratio;
            dep_node->demandMetric(blame_metrics_index.second) += latency * neighbor_ratio;
          }

          for (auto &s : _inst_stall_metrics) {
            auto metrics_index = metric_name_prof_map[s];
            auto latency = node->demandMetric(metrics_index.first);
            auto blame_metrics_index = metric_name_prof_map["BLAME " + s];
            // inclusive and exclusive metrics have the same value
            // blame itself
            node->demandMetric(blame_metrics_index.first) += latency;
            node->demandMetric(blame_metrics_index.second) += latency;
          }
        }
      }
    }
  }
}


void CudaAdvisor::blame(Prof::LoadMap::LMId_t lm_id, std::vector<CudaParse::InstructionStat *> &inst_stats) {
  // 1. Map pc to CCT and structs
  VMAProfMap vma_prof_map;
  constructVMAProfMap(lm_id, inst_stats, vma_prof_map);

  VMAStructMap vma_struct_map;
  constructVMAStructMap(inst_stats, vma_struct_map);

  // 2. Init a dependency graph with struct nodes
  // a->b a depends on b
  CCTGraph<Prof::Struct::ANode *> struct_dep_graph;
  initStructGraph(inst_stats, struct_dep_graph, vma_struct_map);

  // 3.1 Init a graph with CCTs
  CCTGraph<Prof::CCT::ANode *> cct_dep_graph;
  initCCTGraph(inst_stats, cct_dep_graph, vma_prof_map);

  // 3.2 Iterative update CCT graph
  updateCCTGraph(cct_dep_graph, struct_dep_graph, vma_prof_map, vma_struct_map);

  // 4. accumulate blames
  VMAInstMap vma_inst_map;
  for (auto *inst_stat : inst_stats) {
    vma_inst_map[inst_stat->pc] = inst_stat;
  }
  blameCCTGraph(cct_dep_graph, vma_inst_map);

  // 5. find bottlenecks
}


void CudaAdvisor::advise(Prof::LoadMap::LMId_t lm_id) {
}


void CudaAdvisor::save(const std::string &file_name) {
}

}  // namespace CallPath

}  // namespace Analysis
