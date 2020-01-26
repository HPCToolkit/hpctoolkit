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
#include <limits>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>

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

#define DEBUG_CALLPATH_CUDAADVISOR 0

namespace Analysis {

namespace CallPath {

void CudaAdvisor::init() {
  if (_inst_stall_metrics.size() != 0) {
    // Init already
    return;
  }

  // init individual metrics
  _issue_metric = GPU_INST_METRIC_NAME":STL_NONE";
  _inst_metric = GPU_INST_METRIC_NAME;

  _invalid_stall_metric = GPU_INST_METRIC_NAME":STL_INV";
  _tex_stall_metric = GPU_INST_METRIC_NAME":STL_TMEM";
  _ifetch_stall_metric = GPU_INST_METRIC_NAME":STL_IFET";
  _pipe_bsy_stall_metric = GPU_INST_METRIC_NAME":STL_PIPE";
  _mem_thr_stall_metric = GPU_INST_METRIC_NAME":STL_MTHR";
  _nosel_stall_metric = GPU_INST_METRIC_NAME":STL_NSEL";
  _other_stall_metric = GPU_INST_METRIC_NAME":STL_OTHR";
  _sleep_stall_metric = GPU_INST_METRIC_NAME":STL_SLP";
  _cmem_stall_metric = GPU_INST_METRIC_NAME":STL_CMEM";
  
  _inst_stall_metrics.insert(_invalid_stall_metric);
  _inst_stall_metrics.insert(_tex_stall_metric);
  _inst_stall_metrics.insert(_ifetch_stall_metric);
  _inst_stall_metrics.insert(_pipe_bsy_stall_metric);
  _inst_stall_metrics.insert(_mem_thr_stall_metric);
  _inst_stall_metrics.insert(_nosel_stall_metric);
  _inst_stall_metrics.insert(_other_stall_metric);
  _inst_stall_metrics.insert(_sleep_stall_metric);
  _inst_stall_metrics.insert(_cmem_stall_metric);

  _exec_dep_stall_metric = GPU_INST_METRIC_NAME":STL_IDEP";
  _mem_dep_stall_metric = GPU_INST_METRIC_NAME":STL_GMEM";
  _sync_stall_metric = GPU_INST_METRIC_NAME":STL_SYNC";

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
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks(); ++mpi_rank) {
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank); ++thread_id) {
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) == -1) {
        // skip tracing threads
        continue;
      }

      // For each thread O(T)
      // Init queue
      std::queue<Prof::CCT::ADynNode *> nodes;
      for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
        auto *node = *iter;
        // Every stalled instruction must be issued at least once
        demandNodeMetrics(node);
        nodes.push(node);
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
}


void CudaAdvisor::demandNodeMetrics(Prof::CCT::ADynNode *node) {
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks(); ++mpi_rank) {
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank); ++thread_id) {
      auto ex_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, false);
      if (ex_inst_metric_index == -1) {
        // skip tracing threads
        continue;
      }
      int inst = node->demandMetric(ex_inst_metric_index);
      if (inst != 0) {
        demandNodeMetric(mpi_rank, thread_id, node);
      }
    }
  }
}


int CudaAdvisor::demandNodeMetric(int mpi_rank, int thread_id, Prof::CCT::ADynNode *node) {
  auto in_issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric, true);
  auto ex_issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric, false);
  auto in_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, true);
  auto ex_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, false);

  if (ex_inst_metric_index == -1) {
    // skip tracing threads
    return 0;
  }

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


void CudaAdvisor::blameCCTGraph(CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph, VMAInstMap &vma_inst_map,
  InstBlames &inst_blames) {
  // for each thread
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks(); ++mpi_rank) {
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank); ++thread_id) {
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) == -1) {
        // skip tracing threads
        continue;
      }

      std::vector<InstructionBlame> inst_blame;
      for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
        auto *node = *iter;
        auto src_vma = node->lmIP();
        auto niter = cct_dep_graph.outgoing_nodes(node);

        std::map<std::string, double> sum;
        if (niter != cct_dep_graph.outgoing_nodes_end()) {
          for (auto *dep_node : niter->second) {
            auto vma = dep_node->lmIP();
            auto *inst_stat = vma_inst_map[vma];

            // sum up all neighbor node's instructions
            if (inst_stat->op.find("MEMORY") != std::string::npos) {
              if (inst_stat->op.find("GLOBAL") != std::string::npos) {
                sum[_mem_dep_stall_metric] += demandNodeMetric(mpi_rank, thread_id, dep_node);
              } else if (inst_stat->op.find("LOCAL") != std::string::npos) {
                sum[_mem_dep_stall_metric] += demandNodeMetric(mpi_rank, thread_id, dep_node);
              } else {
                sum[_exec_dep_stall_metric] += demandNodeMetric(mpi_rank, thread_id, dep_node);
              }
            } else {
              sum[_exec_dep_stall_metric] += demandNodeMetric(mpi_rank, thread_id, dep_node);
            }
          }
        }

        // dependent latencies
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
              // one metric id is enough for inst blame analysis
              inst_blame.emplace_back(
                InstructionBlame(src_vma, vma, ex_blame_metric_index, latency * neighbor_ratio));
            }
          }
        }

        // stall latencies
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
          // one metric id is enough for inst blame analysis
          inst_blame.emplace_back(
            InstructionBlame(src_vma, src_vma, ex_blame_metric_index, stall));
        }
      }

      std::sort(inst_blame.begin(), inst_blame.end());
      inst_blames[mpi_rank][thread_id] = inst_blame;
    }
  }
}


void CudaAdvisor::overlayInstructionBlames(std::vector<CudaParse::Function *> &functions,
  InstBlames &inst_blames) {
  // Overlay instructions
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks();
    ++mpi_rank) {
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank);
      ++thread_id) {
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) == -1) {
        // Skip tracing threads
        continue;
      }

      auto thread_inst_blame_start = 0;
      for (auto *function : functions) {
        // Construct function boundaries
        VMA function_start_pc = std::numeric_limits<VMA>::max();
        VMA function_end_pc = std::numeric_limits<VMA>::min();
        FunctionBlame function_blame;
        for (auto *block : function->blocks) {
          VMA block_start_pc = block->insts.front()->inst_stat->pc;
          VMA block_end_pc = block->insts.back()->inst_stat->pc; 
          function_start_pc = std::min(block_start_pc, function_start_pc);
          function_end_pc = std::max(block_end_pc, function_end_pc);
          BlockBlame block_blame(block_start_pc, block_end_pc);
          function_blame.block_blames.push_back(block_blame);
        }
        function_blame.start = function_start_pc;
        function_blame.end = function_end_pc;

        auto block_blame_index = 0;
        auto &thread_inst_blames = inst_blames[mpi_rank][thread_id];
        for (; thread_inst_blame_start < thread_inst_blames.size(); ++thread_inst_blame_start) {
          auto &block_blame = function_blame.block_blames[block_blame_index];
          auto &inst_blame = thread_inst_blames[thread_inst_blame_start];

          if (inst_blame.src > function_blame.end) {
            // Go to next function
            break;
          }

          // Find the correponding block
          while (block_blame.end < inst_blame.src) {
            ++block_blame_index;
            block_blame = function_blame.block_blames[block_blame_index];
          }
          if (inst_blame.src >= block_blame.start) {
            block_blame.inst_blames.push_back(inst_blame);
            block_blame.blames[inst_blame.metric_id] += inst_blame.value;
            block_blame.blame_sum += inst_blame.value;
            function_blame.blames[inst_blame.metric_id] += inst_blame.value;
            function_blame.blame_sum += inst_blame.value;
          }
        }
        _function_blames[mpi_rank][thread_id].push_back(function_blame);
      }
    }
  }
}


void CudaAdvisor::debugOutstandingCCTs(CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks(); ++mpi_rank) {
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank); ++thread_id) {
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) == -1) {
        // skip tracing threads
        continue;
      }

      // <dep_stalls, <vmas> >
      std::map<int, std::vector<int> > exec_dep_vmas;
      std::map<int, std::vector<int> > mem_dep_vmas;

      for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
        auto *node = *it;
        auto node_vma = node->lmIP();
        auto exec_dep_stall_metric_id = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _exec_dep_stall_metric);
        auto exec_dep_stall_metric = node->demandMetric(exec_dep_stall_metric_id);
        auto mem_dep_stall_metric_id = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _mem_dep_stall_metric);
        auto mem_dep_stall_metric = node->demandMetric(mem_dep_stall_metric_id);

        exec_dep_vmas[exec_dep_stall_metric].push_back(node_vma);
        mem_dep_vmas[mem_dep_stall_metric].push_back(node_vma);
      }

      std::cout << "[ " << mpi_rank << ", " << thread_id << "]" << std::endl;

      std::cout << "exec_deps" << std::endl;
      for (auto &iter : exec_dep_vmas) {
        std::cout << iter.first << ": ";
        for (auto vma : iter.second) {
          std::cout << vma << ", ";
        }
        std::cout << std::endl;
      }

      std::cout << "mem_deps" << std::endl;
      for (auto &iter : mem_dep_vmas) {
        std::cout << iter.first << ": ";
        for (auto vma : iter.second) {
          std::cout << vma << ", ";
        }
        std::cout << std::endl;
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


void CudaAdvisor::debugInstBlames(InstBlames &inst_blames) {
  for (auto &mpi_inst_blames : inst_blames) {
    for (auto &thread_inst_blames : mpi_inst_blames.second) {
      std::cout << "[" << mpi_inst_blames.first << "," <<
        thread_inst_blames.first << "]:" << std::endl;
      for (auto &inst_blame : thread_inst_blames.second) {
        std::cout << std::hex << inst_blame.dst << "->";
        if (inst_blame.src != inst_blame.dst) {
          std::cout << inst_blame.src << ", ";
        } else {
          std::cout << "-1, ";
        }
        std::cout << std::dec << _metric_name_prof_map->name(inst_blame.metric_id) << ": " <<
          inst_blame.value << std::endl;
      }
    }
  }
}


void CudaAdvisor::blame(Prof::LoadMap::LMId_t lm_id, std::vector<CudaParse::Function *> &functions,
  std::vector<CudaParse::InstructionStat *> &inst_stats) {
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
    std::cout << "Outstanding CCTs before propgation: " << std::endl;
    debugOutstandingCCTs(cct_dep_graph);
    std::cout << std::endl;
  }

  if (DEBUG_CALLPATH_CUDAADVISOR) {
    std::cout << "CCT dependency graph before propgation: " << std::endl;
    debugCCTDepGraph(cct_dep_graph);
    std::cout << std::endl;
  }

  // 3.2 Iterative update CCT graph
  updateCCTGraph(lm_id, cct_dep_graph, inst_dep_graph,
    vma_prof_map, vma_struct_map, vma_inst_map);

  if (DEBUG_CALLPATH_CUDAADVISOR) {
    std::cout << "CCT dependency graph after propgation: " << std::endl;
    debugCCTDepGraph(cct_dep_graph);
    std::cout << std::endl;
  }

  //// 4. accumulate blames
  InstBlames inst_blames;
  blameCCTGraph(cct_dep_graph, vma_inst_map, inst_blames);

  if (DEBUG_CALLPATH_CUDAADVISOR) {
    std::cout << "Debug inst blames: " << std::endl;
    debugInstBlames(inst_blames);
    std::cout << std::endl;
  }

  // 4. overlay blames
  overlayInstructionBlames(functions, inst_blames);
}


void CudaAdvisor::advise(Prof::LoadMap::LMId_t lm_id) {
  // 1. construct function blames, block blames, and instruction blames

}


void CudaAdvisor::save(const std::string &file_name) {
}

}  // namespace CallPath

}  // namespace Analysis
