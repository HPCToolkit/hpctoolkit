//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <sstream>

#include <climits>
#include <cstring>
#include <cstdio>

#include <bitset>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <limits>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>

#include "GPUAdvisor.hpp"
#include "../MetricNameProfMap.hpp"
#include "../CCTGraph.hpp"

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

#define DEBUG_GPUADVISOR 0
#define DEBUG_GPUADVISOR_DETAILS 0

#define MAX2(x, y) (x > y ? x : y)

namespace Analysis {

/*
 * Debug methods
 */

// Define an offset in cubin
std::string GPUAdvisor::debugInstOffset(int vma) {
  auto iter = _function_offset.upper_bound(vma);
  if (iter != _function_offset.begin()) {
    --iter;
  }
  std::stringstream ss;
  ss << "(" << iter->second << ": " << std::hex << "0x" << vma - iter->first << ")" << std::dec;
  return ss.str();
}


void GPUAdvisor::debugCCTDepGraphSummary(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // <dep_stalls, <vmas> >
  std::map<std::string, double> lats;
  auto total = 0.0;

  for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
    auto *to_node = *it;

    for (auto &p : _inst_metrics) {
      auto metric_index = _metric_name_prof_map->metric_id(
        mpi_rank, thread_id, p.second);
      if (metric_index != -1) {
        double lat = to_node->demandMetric(metric_index);
        lats[p.second] += lat;
        total += lat;
      }
    }

    for (auto &p : _dep_metrics) {
      auto metric_index = _metric_name_prof_map->metric_id(
        mpi_rank, thread_id, p.second);
      if (metric_index != -1) {
        double lat = to_node->demandMetric(metric_index);
        lats[p.second] += lat;
        total += lat;
      }
    }
  }

  std::cout << "Total: " << total << std::endl;
  for (auto &lat_iter : lats) {
    std::cout << lat_iter.first << ": " << lat_iter.second << std::endl;
  }
}


void GPUAdvisor::debugCCTDepGraph(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // Only debug no stall latencies
  std::cout << "Nodes (" << cct_dep_graph.size() << ")" << std::endl;
  std::cout << "Edges (" << cct_dep_graph.edge_size() << ")" << std::endl;

  // <dep_stalls, <vmas> >
  std::map<int, std::vector<int> > exec_dep_vmas;
  std::map<int, std::vector<int> > mem_dep_vmas;
  double exec_deps = 0.0;
  double mem_deps = 0.0;

  for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
    auto *to_node = *it;
    auto to_vma = to_node->lmIP();

    auto exec_dep_metric_index = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, _exec_dep_lat_metric);
    auto exec_dep = to_node->demandMetric(exec_dep_metric_index);
    auto mem_dep_metric_index = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, _mem_dep_lat_metric);
    auto mem_dep = to_node->demandMetric(mem_dep_metric_index);

    exec_dep_vmas[exec_dep].push_back(to_vma);
    mem_dep_vmas[mem_dep].push_back(to_vma);
    exec_deps += exec_dep;
    mem_deps += mem_dep;

    auto innode_iter = cct_dep_graph.incoming_nodes(to_node);
    if (innode_iter != cct_dep_graph.incoming_nodes_end()) {
      for (auto *from_node : innode_iter->second) {
        auto from_vma = from_node->lmIP();
        std::cout << debugInstOffset(from_vma) << "," << std::dec;
      }
      if (innode_iter->second.size() != 0) {
        std::cout << " -> " << debugInstOffset(to_vma) << std::endl;
      }
    }
  }

  std::cout << std::endl << "Outstanding Latencies:" << std::endl;

  std::cout << "Exec_deps (" << exec_deps << ")" << std::endl;
  for (auto &iter : exec_dep_vmas) {
    if (iter.first == 0) {
      continue;
    }
    std::cout << iter.first << "(" <<
      static_cast<double>(iter.first) / exec_deps * 100 << "%): ";
    for (auto vma : iter.second) {
      std::cout << debugInstOffset(vma) << ", ";
    }
    std::cout << std::endl;
  }

  std::cout << "Mem_deps (" << mem_deps << ")" << std::endl;
  for (auto &iter : mem_dep_vmas) {
    if (iter.first == 0) {
      continue;
    }
    std::cout << iter.first << "(" <<
      static_cast<double>(iter.first) / mem_deps * 100 << "%): ";
    for (auto vma : iter.second) {
      std::cout << debugInstOffset(vma) << ", ";
    }
    std::cout << std::endl;
  }
}


void GPUAdvisor::debugCCTDepGraphNoPath(int mpi_rank, int thread_id,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // Only debug no stall latencies
  // <dep_stalls, <vmas> >
  std::map<int, std::vector<int> > exec_dep_vmas;
  std::map<int, std::vector<int> > mem_dep_vmas;
  double exec_deps = 0.0;
  double no_path_exec_dep = 0.0;
  double mem_deps = 0.0;
  double no_path_mem_dep = 0.0;

  for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
    auto *node = *it;
    auto node_vma = node->lmIP();
    auto exec_dep_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _exec_dep_lat_metric);
    auto exec_dep = node->demandMetric(exec_dep_metric_index);
    auto mem_dep_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _mem_dep_lat_metric);
    auto mem_dep = node->demandMetric(mem_dep_metric_index);

    if (cct_dep_graph.incoming_nodes_size(node) == 0 && exec_dep != 0) {
      exec_dep_vmas[exec_dep].push_back(node_vma);
      no_path_exec_dep += exec_dep;
    }

    if (cct_dep_graph.incoming_nodes_size(node) == 0 && mem_dep != 0) {
      mem_dep_vmas[mem_dep].push_back(node_vma);
      no_path_mem_dep += mem_dep;
    }

    exec_deps += exec_dep;
    mem_deps += mem_dep;
  }

  std::cout << "Exec_deps" << std::endl;
  for (auto &iter : exec_dep_vmas) {
    if (iter.first == 0) {
      continue;
    }
    std::cout << iter.first << "(" <<
      static_cast<double>(iter.first) / exec_deps * 100 << "%): ";
    for (auto vma : iter.second) {
      std::cout << debugInstOffset(vma) << ", ";
    }
    std::cout << std::endl;
  }
  std::cout << no_path_exec_dep << "(" <<
    no_path_exec_dep / exec_deps * 100 << "%)" << std::endl;

  std::cout << "Mem_deps" << std::endl;
  for (auto &iter : mem_dep_vmas) {
    if (iter.first == 0) {
      continue;
    }
    std::cout << iter.first << "(" <<
      static_cast<double>(iter.first) / mem_deps * 100 << "%): ";
    for (auto vma : iter.second) {
      std::cout << debugInstOffset(vma) << ", ";
    }
    std::cout << std::endl;
  }
  std::cout << no_path_mem_dep << "(" <<
    no_path_mem_dep / mem_deps * 100 << "%)" << std::endl;

  std::cout << no_path_mem_dep + no_path_exec_dep << "(" <<
    (no_path_mem_dep + no_path_exec_dep) / (exec_deps + mem_deps) * 100 <<
    "%): overall" << std::endl;
}


void GPUAdvisor::debugCCTDepGraphStallExec(int mpi_rank, int thread_id,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // <dep_stalls, <vmas> >
  std::map<int, std::vector<int> > stall_exec_dep_vmas;
  double stall_exec_dep = 0.0;
  double exec_deps = 0.0;

  for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
    auto *node = *it;
    auto node_vma = node->lmIP();
    auto *inst = _vma_prop_map.at(node_vma).inst;
    auto exec_dep_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _exec_dep_lat_metric);
    auto exec_dep = node->demandMetric(exec_dep_metric_index);

    if (inst->control.wait == 0) {
      stall_exec_dep += exec_dep;
      stall_exec_dep_vmas[exec_dep].push_back(node_vma);
    }

    exec_deps += exec_dep;
  }

  std::cout << "Exec_deps" << std::endl;
  for (auto &iter : stall_exec_dep_vmas) {
    if (iter.first == 0) {
      continue;
    }
    std::cout << iter.first << "(" <<
      static_cast<double>(iter.first) / exec_deps * 100 << "%): ";
    for (auto vma : iter.second) {
      std::cout << debugInstOffset(vma) << ", ";
    }
    std::cout << std::endl;
  }
  std::cout << stall_exec_dep << "(" <<
    stall_exec_dep / exec_deps * 100 << "%)" << std::endl;
}


void GPUAdvisor::debugCCTDepGraphSinglePath(CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  double total_path = 0.0;
  double single_path = 0.0;

  for (auto it = cct_dep_graph.nodeBegin(); it != cct_dep_graph.nodeEnd(); ++it) {
    auto *to_node = *it;
    auto innode_iter = cct_dep_graph.incoming_nodes(to_node);

    auto mem_dep_path = 0;
    auto exec_dep_path = 0;

    if (innode_iter != cct_dep_graph.incoming_nodes_end()) {
      for (auto *from_node : innode_iter->second) {
        auto from_vma = from_node->lmIP();
        auto *from_inst = _vma_prop_map.at(from_vma).inst;

        if (from_inst->op.find("MEMORY") != std::string::npos) {
          if (from_inst->op.find(".SHARED") != std::string::npos) {
            // Shared memory instructions only cause short_scoreboard wait
            ++exec_dep_path;
          } else {
            // L1TEX Memory instructions only cause memory dep
            ++mem_dep_path;
          }
        } else {
          // short_scoreboard or fixed dependency 
          ++exec_dep_path;
        }
      }
    }
    
    if ((exec_dep_path == 1 && mem_dep_path == 0) ||
        (exec_dep_path == 0 && mem_dep_path == 1) ||
        (exec_dep_path == 1 && mem_dep_path == 1) ||
        (exec_dep_path == 0 && mem_dep_path == 0)) {
      ++single_path;
    }

    ++total_path;
  }

  if (total_path == 0) {
    std::cout << "No path" << std::endl;
  } else {
    std::cout << std::setprecision(5) << single_path / total_path << std::endl;
  }
}


void GPUAdvisor::debugCCTDepPaths(CCTEdgePathMap &cct_edge_path_map) {
  for (auto &from_iter : cct_edge_path_map) {
    auto from_vma = from_iter.first;
    for (auto &to_iter : from_iter.second) {
      if (to_iter.second.size() == 0) {
        // Skip zero path entries
        continue;
      }
      auto to_vma = to_iter.first;
      std::cout << debugInstOffset(from_vma) << " -> " <<
        debugInstOffset(to_vma) << ": ";
      for (auto &path : to_iter.second) {
        std::cout << "[";
        for (auto *b : path) {
          auto front_vma = b->insts.front()->inst_stat->pc;
          std::cout << debugInstOffset(front_vma) << ",";
        }
        std::cout << "]" << std::endl;
      }
    }
  }
}


void GPUAdvisor::debugInstDepGraph() {
  std::cout << "Nodes (" << _inst_dep_graph.size() << ")" << std::endl;
  std::cout << "Edges (" << _inst_dep_graph.edge_size() << ")" << std::endl;

  for (auto it = _inst_dep_graph.nodeBegin(); it != _inst_dep_graph.nodeEnd(); ++it) {
    auto *to_node = *it;
    auto to_vma = to_node->pc;

    auto innode_iter = _inst_dep_graph.incoming_nodes(to_node);
    if (innode_iter != _inst_dep_graph.incoming_nodes_end()) {
      for (auto *from_node : innode_iter->second) {
        auto from_vma = from_node->pc;
        std::cout << debugInstOffset(from_vma) << "," << std::dec;
      }
      std::cout << " -> " << debugInstOffset(to_vma) << std::dec << std::endl;
    }
  }
}


void GPUAdvisor::debugInstBlames(InstBlames &inst_blames) {
  // <blame_id, blame_sum>
  double lat_blame_sum = 0.0;
  double stall_blame_sum = 0.0;
  for (auto &inst_blame : inst_blames) {
    lat_blame_sum += inst_blame.lat_blame;
    stall_blame_sum += inst_blame.stall_blame;
  }

  std::cout << "(" << lat_blame_sum << "," << stall_blame_sum << ")" << std::endl;

  for (auto &inst_blame : inst_blames) {
    std::cout << debugInstOffset(inst_blame.src_inst->pc) <<
      " -> " << debugInstOffset(inst_blame.dst_inst->pc) << ", ";
    std::cout << inst_blame.blame_name << ": " << inst_blame.lat_blame <<
      "(" << static_cast<double>(inst_blame.lat_blame) /
      lat_blame_sum * 100 << "%" 
      ", " << static_cast<double>(inst_blame.stall_blame) /
      stall_blame_sum * 100 << "%)" << std::endl;
  }
}


/*
 * Private methods
 */

int GPUAdvisor::demandNodeMetric(int mpi_rank, int thread_id, Prof::CCT::ADynNode *node) {
  auto in_issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric, true);
  auto ex_issue_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _issue_metric, false);
  auto in_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, true);
  auto ex_inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric, false);

  // If instruction issue is not sampled, assign it one issue
  int ret = node->demandMetric(in_issue_metric_index);
  if (ret == 0) {
    node->demandMetric(in_issue_metric_index) += _arch->warp_size();
    node->demandMetric(ex_issue_metric_index) += _arch->warp_size();
    node->demandMetric(in_inst_metric_index) += _arch->warp_size();
    node->demandMetric(ex_inst_metric_index) += _arch->warp_size();
    ret = 1;
  }
  return ret;
}


void GPUAdvisor::attributeBlameMetric(int mpi_rank, int thread_id,
  Prof::CCT::ANode *node, const std::string &blame_name, double blame) {
  // inclusive and exclusive metrics have the same value
  auto in_blame_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, blame_name, true);
  auto ex_blame_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, blame_name, false);

  node->demandMetric(in_blame_metric_index) += blame;
  node->demandMetric(ex_blame_metric_index) += blame;
}


void GPUAdvisor::initCCTDepGraph(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // Init nodes
  for (auto &iter : _vma_prop_map) {
    auto *prof_node = iter.second.prof_node;
    if (prof_node != NULL) {
      cct_dep_graph.addNode(prof_node);
    }
  }

  std::vector<std::pair<Prof::CCT::ADynNode *, Prof::CCT::ADynNode *> > edge_vec;
  // Insert all possible edges
  // If a node has samples, we find all possible causes.
  // Even for instructions that are not sampled, we assign them one issued sample, but no latency sample.
  // Since no latency sample is associated with these nodes, we do not need to propogate in the grpah.
  for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
    auto *node = *iter;
    auto node_vma = node->lmIP();

    // Find its dependent instructions
    auto *node_inst = _vma_prop_map.at(node_vma).inst;
    auto inst_iter = _inst_dep_graph.incoming_nodes(node_inst);

    if (inst_iter != _inst_dep_graph.incoming_nodes_end()) {
      for (auto *inst : inst_iter->second) {
        auto vma = inst->pc;
        auto vma_prop_iter = _vma_prop_map.find(vma);

        Prof::CCT::ADynNode *prof_node = NULL;
        if (vma_prop_iter->second.prof_node == NULL) {
          // Create a new prof node
          Prof::Metric::IData metric_data(_prof->metricMgr()->size());
          metric_data.clearMetrics();

          prof_node = new Prof::CCT::Stmt(node->parent(), HPCRUN_FMT_CCTNodeId_NULL,
            lush_assoc_info_NULL, _gpu_root->lmId(), vma, 0, NULL, metric_data);
          vma_prop_iter->second.prof_node = prof_node;
        } else {
          prof_node = vma_prop_iter->second.prof_node;
        }

        edge_vec.push_back(std::pair<Prof::CCT::ADynNode *, Prof::CCT::ADynNode *>(prof_node, node));
      }
    }
  }

  for (auto &edge : edge_vec) {
    auto *from_node = edge.first;
    auto *to_node = edge.second;
    
    cct_dep_graph.addEdge(from_node, to_node);
  }
}


void GPUAdvisor::pruneCCTDepGraphOpcode(int mpi_rank, int thread_id,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // Profile single path coverage
  if (DEBUG_GPUADVISOR) {
    std::cout << "Single path coverage before opcode constraints:" << std::endl;
    debugCCTDepGraphSinglePath(cct_dep_graph);
    std::cout << std::endl;
  }

  auto mem_dep_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _mem_dep_lat_metric);
  auto exec_dep_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _exec_dep_lat_metric);

  // Opcode constraints
  std::vector<std::set<CCTEdge<Prof::CCT::ADynNode *> >::iterator> remove_edges;
  for (auto iter = cct_dep_graph.edgeBegin(); iter != cct_dep_graph.edgeEnd(); ++iter) {
    auto edge = *iter;
    auto *from = edge.from;
    auto from_vma = from->lmIP();
    auto *from_inst = _vma_prop_map.at(from_vma).inst;
    auto *to = edge.to;

    if (from_inst->op.find("MEMORY") != std::string::npos) {
      if (from_inst->op.find(".SHARED") != std::string::npos) {
        // Shared memory instructions only cause short_scoreboard wait
        if (to->demandMetric(exec_dep_index) == 0) {
          remove_edges.push_back(iter);
        }
      } else {
        // L1TEX Memory instructions cause memory dep
        if (to->demandMetric(mem_dep_index) == 0) {
          remove_edges.push_back(iter);
        }
      }
    } else {
      // XXX(Keren): Other type instructions cause either
      // short_scoreboard or fixed dependency 
      if (to->demandMetric(exec_dep_index) == 0) {
        remove_edges.push_back(iter);
      }
    }
  }

  for (auto &iter : remove_edges) {
    if (DEBUG_GPUADVISOR_DETAILS) {
      auto edge = *iter;
      auto *from = edge.from;
      auto from_vma = from->lmIP();
      auto *to = edge.to;
      auto to_vma = to->lmIP();
      auto mem_deps = to->demandMetric(mem_dep_index);
      auto exec_deps = to->demandMetric(exec_dep_index);
      std::cout << "Remove " << debugInstOffset(from_vma) << " -> " <<
        debugInstOffset(to_vma) << ", Mem_deps: " << mem_deps <<
        ", Exec_deps: " << exec_deps << std::endl;
    }
    cct_dep_graph.removeEdge(iter);
  }

  // Profile single path coverage
  if (DEBUG_GPUADVISOR) {
    std::cout << "Single path coverage after opcode constraints:" << std::endl;
    debugCCTDepGraphSinglePath(cct_dep_graph);
    std::cout << std::endl;
  }
}


void GPUAdvisor::pruneCCTDepGraphBarrier(int mpi_rank, int thread_id,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph) {
  // Profile single path coverage
  if (DEBUG_GPUADVISOR) {
    std::cout << "Single path coverage before barrier constraints:" << std::endl;
    debugCCTDepGraphSinglePath(cct_dep_graph);
    std::cout << std::endl;
  }

  auto mem_dep_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _mem_dep_lat_metric);
  auto exec_dep_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _exec_dep_lat_metric);

  // Opcode constraints
  std::vector<std::set<CCTEdge<Prof::CCT::ADynNode *> >::iterator> remove_edges;
  for (auto iter = cct_dep_graph.edgeBegin(); iter != cct_dep_graph.edgeEnd(); ++iter) {
    auto edge = *iter;
    auto *from = edge.from;
    auto from_vma = from->lmIP();
    auto *from_inst = _vma_prop_map.at(from_vma).inst;
    auto *to = edge.to;
    auto to_vma = to->lmIP();
    auto *to_inst = _vma_prop_map.at(to_vma).inst;

    // No barrier, we prune it by path analysis
    if (from_inst->control.read == CudaParse::InstructionStat::BARRIER_NONE ||
      from_inst->control.write == CudaParse::InstructionStat::BARRIER_NONE) {
      continue;
    }

    // Barrier does not match
    if (((to_inst->control.wait & (1 << from_inst->control.read)) == 0) &&
      ((to_inst->control.wait & (1 << from_inst->control.write)) == 0)) {
      remove_edges.push_back(iter);
    }
  }

  for (auto &iter : remove_edges) {
    if (DEBUG_GPUADVISOR_DETAILS) {
      auto edge = *iter;
      auto *from = edge.from;
      auto from_vma = from->lmIP();
      auto *from_inst = _vma_prop_map.at(from_vma).inst;
      auto *to = edge.to;
      auto to_vma = to->lmIP();
      auto *to_inst = _vma_prop_map.at(to_vma).inst;
      auto mem_deps = to->demandMetric(mem_dep_index);
      auto exec_deps = to->demandMetric(exec_dep_index);
      std::cout << "Remove " << debugInstOffset(from_vma) << " -> " <<
        debugInstOffset(to_vma) << ", Barrier: b" <<
        std::bitset<6>(to_inst->control.wait) << ", Write: " <<
        static_cast<int>(from_inst->control.write) << ", Read: " <<
        static_cast<int>(from_inst->control.read) << 
        ", Mem_deps: " << mem_deps <<
        ", Exec_deps: " << exec_deps << std::endl;
    }
    cct_dep_graph.removeEdge(iter);
  }

  // Profile single path coverage
  if (DEBUG_GPUADVISOR) {
    std::cout << "Single path coverage after barrier constraints:" << std::endl;
    debugCCTDepGraphSinglePath(cct_dep_graph);
    std::cout << std::endl;
  }
}


// DFS procedure
void GPUAdvisor::trackDep(int from_vma, int to_vma, int id,
  CudaParse::Block *from_block, CudaParse::Block *to_block,
  int latency_issue, int latency,
  std::set<CudaParse::Block *> &visited_blocks,
  std::vector<CudaParse::Block *> &path,
  std::vector<std::vector<CudaParse::Block *>> &paths,
  TrackType track_type, bool fixed) {
  // The current block has been visited
  if (visited_blocks.find(from_block) != visited_blocks.end()) {
    return;
  }
  visited_blocks.insert(from_block);
  path.push_back(from_block);

  auto front_vma = from_block->insts.front()->inst_stat->pc;
  auto back_vma = from_block->insts.back()->inst_stat->pc;

  // Accumulate throughput
  bool find_def = false;
  bool hidden = false;
  auto start_vma = front_vma;
  auto end_vma = back_vma;

  // [front_vma, from_vma, back_vma]
  if (from_vma <= back_vma && from_vma >= front_vma) {
    start_vma = from_vma + _arch->inst_size();
  }

  // [front_vma, to_vma, back_vma]
  if (to_vma <= back_vma && to_vma >= front_vma) {
    end_vma = to_vma - _arch->inst_size();
  }

  // If from_vma and to_vma are in the same block but from_vma >= to_vma
  // It indicates we have a loop
  bool loop_block = false;
  if (from_vma <= back_vma && from_vma >= front_vma &&
    to_vma <= back_vma && to_vma >= front_vma &&
    from_vma >= to_vma) {
    // It only happens in the first block
    visited_blocks.erase(from_block);
    end_vma = back_vma;
    loop_block = true;
  }

  // Iterate inst until reaching the use inst
  while (start_vma <= end_vma) {
    auto *inst = _vma_prop_map.at(start_vma).inst;
    // 1: instruction issue
    if (fixed) {
      latency_issue += 1;
    } else {
      latency_issue += inst->control.stall + 1;
    }

    // 1. id on path constraint
    bool find = false;
    if (track_type == TRACK_REG) {
      find = inst->find_src_reg(id);
    } else if (track_type == TRACK_PRED_REG) {
      find = inst->find_src_pred_reg(id);
    } else if (track_type == TRACK_PREDICATE) {
      find = (inst->predicate == id || inst->find_src_pred_reg(id));
    } else {  // track_type == TRACK_BARRIER
      find = inst->find_src_barrier(id);
    }

    if (find && start_vma != (from_vma + _arch->inst_size())) {
      find_def = true;
      break;
    }

    // 2. Latency constraint
    if (latency_issue >= latency) {
      hidden = true;
      break;
    }

    start_vma += _arch->inst_size();
  }

  // If we find a valid path
  if (find_def == false && hidden == false) {
    // Reach the final block
    // [front_vma, to_vma, back_vma]
    if (to_vma <= back_vma && to_vma >= front_vma && loop_block == false) {
      paths.push_back(path);
    } else {
      for (auto *target : from_block->targets) {
        if (target->type != CudaParse::TargetType::CALL) {
          // We do not need from_vma anymore
          trackDep(0, to_vma, id, target->block, to_block, latency_issue, latency,
            visited_blocks, path, paths, track_type, fixed);
        }
      }
    }
  }

  visited_blocks.erase(from_block);
  path.pop_back();
}


void GPUAdvisor::trackDepInit(int to_vma, int from_vma,
  int dst, CCTEdgePathMap &cct_edge_path_map, TrackType track_type, bool fixed) {
  // Search for all possible paths from dst to src.
  // Since the single path rate here is expected to be high,
  // The following code section only executes few times.
  // Two constraints:
  // 1. A path is eliminated if an instruction on the way uses dst
  // 2. A path is eliminated if the throughput cycles is greater than latency
  auto *from_block = _vma_prop_map.at(from_vma).block;
  auto *to_block = _vma_prop_map.at(to_vma).block;
  std::set<CudaParse::Block *> visited_blocks;
  std::vector<CudaParse::Block *> path;
  std::vector<std::vector<CudaParse::Block *>> paths;
  auto latency = _vma_prop_map.at(from_vma).latency_upper;
  trackDep(from_vma, to_vma, dst, from_block, to_block,
    0, latency, visited_blocks, path, paths, track_type, fixed);

  // Merge paths
  for (auto &path : paths) {
    bool new_path = true;
    // Existing paths
    for (auto &p : cct_edge_path_map[from_vma][to_vma]) {
      if (path.size() != p.size()) {
        continue;
      }
      // Same size paths, comparing every element
      bool same_path = true;
      for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] != path[i]) {
          same_path = false;
          break;
        }
      }
      if (same_path == true) {
        new_path = false;
        break;
      }
    }
    if (new_path == true) {
      cct_edge_path_map[from_vma][to_vma].push_back(path);
    }
  }
}


void GPUAdvisor::pruneCCTDepGraphLatency(int mpi_rank, int thread_id,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
  CCTEdgePathMap &cct_edge_path_map) {
  // Profile single path coverage
  if (DEBUG_GPUADVISOR) {
    std::cout << "Single path coverage before latency constraints:" << std::endl;
    debugCCTDepGraphSinglePath(cct_dep_graph);
    std::cout << std::endl;
  }

  auto mem_dep_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _mem_dep_lat_metric);
  auto exec_dep_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _exec_dep_lat_metric);

  std::vector<std::set<CCTEdge<Prof::CCT::ADynNode *> >::iterator> remove_edges;
  for (auto iter = cct_dep_graph.edgeBegin(); iter != cct_dep_graph.edgeEnd(); ++iter) {
    auto edge = *iter;
    auto *from = edge.from;
    auto from_vma = from->lmIP();
    auto *to = edge.to;
    auto to_vma = to->lmIP();
    auto *to_inst = _vma_prop_map.at(to_vma).inst;
    auto *from_inst = _vma_prop_map.at(from_vma).inst;
    bool fixed = from_inst->control.read == 0 && from_inst->control.write == 0;

    // Regular regs
    for (auto dst : from_inst->dsts) {
      // Find the dst reg that causes dependency
      auto assign_iter = to_inst->assign_pcs.find(dst);
      if (assign_iter != to_inst->assign_pcs.end()) {
        trackDepInit(to_vma, from_vma, dst, cct_edge_path_map, TRACK_REG, fixed);
      }
    }

    // Predicate regs
    for (auto pdst : from_inst->pdsts) {
      auto passign_iter = to_inst->passign_pcs.find(pdst);
      if (passign_iter != to_inst->passign_pcs.end()) {
        trackDepInit(to_vma, from_vma, pdst, cct_edge_path_map, TRACK_PRED_REG, fixed);
      }
    }

    // Barrier
    for (auto bdst : from_inst->bdsts) {
      // Find the dst barrier that causes dependency
      auto bassign_iter = to_inst->bassign_pcs.find(bdst);
      if (bassign_iter != to_inst->bassign_pcs.end()) {
        trackDepInit(to_vma, from_vma, bdst, cct_edge_path_map, TRACK_BARRIER, fixed);
      }
    }

    // Predicate
    for (auto pred_dst : from_inst->pdsts) {
      // Find the dst barrier that causes dependency
      if (to_inst->predicate == pred_dst) {
        trackDepInit(to_vma, from_vma, pred_dst, cct_edge_path_map, TRACK_PREDICATE, fixed);
      }
    }

    // If there's no satisfied path
    if (cct_edge_path_map[from_vma][to_vma].size() == 0) {
      // This edge can be removed
      remove_edges.push_back(iter);
    }
  }

  for (auto &iter : remove_edges) {
    if (DEBUG_GPUADVISOR_DETAILS) {
      auto edge = *iter;
      auto *from = edge.from;
      auto from_vma = from->lmIP();
      auto *to = edge.to;
      auto to_vma = to->lmIP();
      auto mem_deps = to->demandMetric(mem_dep_index);
      auto exec_deps = to->demandMetric(exec_dep_index);
      std::cout << "Remove " << debugInstOffset(from_vma) << " -> " <<
        debugInstOffset(to_vma) << ", Mem_deps: " << mem_deps <<
        ", Exec_deps: " << exec_deps << std::endl;
    }
    cct_dep_graph.removeEdge(iter);
  }

  if (DEBUG_GPUADVISOR_DETAILS) {
    std::cout << "CCT dependency paths: " << std::endl;
    debugCCTDepPaths(cct_edge_path_map);
    std::cout << std::endl;
  }

  // Profile single path coverage
  if (DEBUG_GPUADVISOR) {
    std::cout << "Single path coverage after latency constraints:" << std::endl;
    debugCCTDepGraphSinglePath(cct_dep_graph);
    std::cout << std::endl;
  }
}


double GPUAdvisor::computePathInsts(
  int mpi_rank, int thread_id, int from_vma, int to_vma,
  std::vector<CudaParse::Block *> &path) {
  double insts = 0.0;

  auto inst_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, _inst_metric, true);

  for (auto i = 0; i < path.size(); ++i) {
    auto *block = path[i];
    auto front_vma = block->insts.front()->inst_stat->pc;
    auto back_vma = block->insts.back()->inst_stat->pc;
    auto start_vma = front_vma; 
    auto end_vma = back_vma;
    if (i == 0) {
      // Do not count the stall of the first inst
      start_vma = from_vma + _arch->inst_size();
    }
    if (i == path.size() - 1) {
      end_vma = to_vma;
    }
    insts += (end_vma - start_vma) / _arch->inst_size() + 1;
    // XXX(Keren): which is more reasonable?
    //for (auto j = start_vma; j <= end_vma; j += _arch->inst_size()) {
    //  auto *prof_node = _vma_prop_map[j].prof_node;
    //  if (prof_node != NULL) {
    //    insts += prof_node->demandMetric(inst_metric_index);
    //  }
    //}
  }

  return insts;
}


void GPUAdvisor::reverseDistance(std::map<Prof::CCT::ADynNode *, double> &distance, std::map<Prof::CCT::ADynNode *, double> &insts) {
  Prof::CCT::ADynNode *pivot = NULL;
  double pivot_inst = 0.0;

  std::map<Prof::CCT::ADynNode *, double> ratios;
  double ratio_sum = 0.0;

  // Fill one if zero
  for (auto &dis_iter : distance) {
    if (dis_iter.second == 0.0) {
      dis_iter.second = 1.0;
    } 
    double ratio = 1.0;
    if (pivot == NULL) {
      pivot = dis_iter.first;
      pivot_inst = dis_iter.second;
    } else {
      ratio = pivot_inst / dis_iter.second;
    }

    ratio_sum += ratio;
    ratios[dis_iter.first] = ratio;
  }

  // Normalize ratio
  for (auto &ratio_iter : ratios) {
    insts[ratio_iter.first] = 1.0 / ratio_sum * ratio_iter.second; 
  }
}


std::pair<std::string, std::string>
GPUAdvisor::detailizeExecBlame(CudaParse::InstructionStat *from_inst,
  CudaParse::InstructionStat *to_inst) {
  bool find_reg = false;
  for (auto dst : from_inst->dsts) {
    if (to_inst->find_src_reg(dst)) {
      find_reg = true;
      break;
    }
  }

  if (!find_reg) {
    for (auto pdst : from_inst->pdsts) {
      if (to_inst->find_src_pred_reg(pdst)) {
        find_reg = true;
        break;
      }
      if (to_inst->predicate == pdst) {
        find_reg = true;
        break;
      }
    }
  }

  if (find_reg) {
    if (from_inst->op.find(".SHARED") != std::string::npos) {
      // smem
      return std::make_pair(_exec_dep_smem_stall_metric, _exec_dep_smem_lat_metric);
    } else {
      // reg
      return std::make_pair(_exec_dep_dep_stall_metric, _exec_dep_dep_lat_metric);
    }
  } else {
    // war
    return std::make_pair(_exec_dep_war_stall_metric, _exec_dep_war_lat_metric);
  }
}


std::pair<std::string, std::string>
GPUAdvisor::detailizeMemBlame(CudaParse::InstructionStat *from_inst) {
  if (from_inst->op.find(".LOCAL") != std::string::npos) {
    // local mem
    return std::make_pair(_mem_dep_lmem_stall_metric, _mem_dep_lmem_lat_metric);
  } else if (from_inst->op.find(".CONSTANT") != std::string::npos) {
    // constant mem
    return std::make_pair(_mem_dep_cmem_stall_metric, _mem_dep_cmem_lat_metric);
  } else {
    // global mem
    return std::make_pair(_mem_dep_gmem_stall_metric, _mem_dep_gmem_lat_metric);
  }
}


void GPUAdvisor::blameCCTDepGraph(int mpi_rank, int thread_id,
  CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph, CCTEdgePathMap &cct_edge_path_map,
  InstBlames &inst_blames) {
  auto mem_stall_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, _mem_dep_stall_metric);
  auto exec_stall_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, _exec_dep_stall_metric);

  auto mem_lat_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, _mem_dep_lat_metric);
  auto exec_lat_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, _exec_dep_lat_metric);

  auto issue_metric_index = _metric_name_prof_map->metric_id(
    mpi_rank, thread_id, _issue_metric);

  for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
    auto *to_node = *iter;
    auto to_vma = to_node->lmIP();
    auto *to_inst = _vma_prop_map.at(to_vma).inst;
    auto to_struct_iter = _vma_struct_map.upper_bound(to_vma);
    Prof::Struct::ACodeNode *to_struct = NULL;
    if (to_struct_iter != _vma_struct_map.begin()) {
      --to_struct_iter;
      to_struct = to_struct_iter->second;
    }
    auto innode_iter = cct_dep_graph.incoming_nodes(to_node);

    std::map<int, std::vector<Prof::CCT::ADynNode *> > prof_nodes;
    if (innode_iter != cct_dep_graph.incoming_nodes_end()) {
      for (auto *from_node : innode_iter->second) {
        // Ensure from node is issued at least once
        demandNodeMetric(mpi_rank, thread_id, from_node);

        auto from_vma = from_node->lmIP();
        auto *from_inst = _vma_prop_map.at(from_vma).inst;

        // Sum up all neighbors' issue count
        if (from_inst->op.find("MEMORY") != std::string::npos) {
          if (from_inst->op.find(".SHARED") != std::string::npos) {
            // Shared memory
            prof_nodes[exec_lat_metric_index].push_back(from_node);
          } else { 
            prof_nodes[mem_lat_metric_index].push_back(from_node);

            // Global and local memory store instructions can result in raw dependencies
            if (from_inst->op.find(".STORE") != std::string::npos) {
              prof_nodes[exec_lat_metric_index].push_back(from_node);
            }

            // We must have found a correponding stall, otherwise the edge is removed by pruning
            auto mem = to_node->demandMetric(mem_lat_metric_index);
            auto exec = to_node->demandMetric(exec_lat_metric_index);
            assert(mem != 0 || exec != 0);
          }
        } else {
          prof_nodes[exec_lat_metric_index].push_back(from_node);
        }
      }
    }
    
    if (prof_nodes.size() == 0 || (prof_nodes.size() == 1 && prof_nodes.begin()->first == mem_lat_metric_index)) {
      // Scheduler lat
      auto lat_blame = to_node->demandMetric(exec_lat_metric_index);
      auto lat_blame_name = "BLAME " + _exec_dep_sche_lat_metric;
      attributeBlameMetric(mpi_rank, thread_id, to_node, lat_blame_name, lat_blame);

      // Scheduler stall
      auto stall_blame = to_node->demandMetric(exec_stall_metric_index);
      auto stall_blame_name = "BLAME " + _exec_dep_sche_stall_metric;
      attributeBlameMetric(mpi_rank, thread_id, to_node, stall_blame_name, stall_blame);

      inst_blames.emplace_back(InstructionBlame(to_inst, to_inst, to_struct, to_struct, 0,
                                                stall_blame, lat_blame, lat_blame_name));
    }


    // Dependent latencies
    for (auto &prof_iter : prof_nodes) {
      auto lat_metric_index = prof_iter.first;
      auto lat = to_node->demandMetric(lat_metric_index);

      auto stall_metric_index = lat_metric_index == mem_lat_metric_index ?
        mem_stall_metric_index : exec_stall_metric_index;
      auto stall = to_node->demandMetric(stall_metric_index);

      auto &from_nodes = prof_iter.second;

      if (lat == 0) {
        // Checked by the previous assertion, must have at least one none-zero latency
        continue;
      }

      std::map<Prof::CCT::ADynNode *, double> distance;
      std::map<Prof::CCT::ADynNode *, double> insts;
      std::map<Prof::CCT::ADynNode *, double> issues;
      double issue_sum = 0.0;

        // Give bias to barrier instructions?
        // TODO(Keren): could be removed when CUPTIv2 is available
#if 0
        std::vector<Prof::CCT::ADynNode *> barrier_from_nodes;
        for (auto *from_node : from_nodes) {
          auto from_vma = from_node->lmIP();
          auto *from_inst = _vma_prop_map[from_vma].inst;

          if ((to_inst->control.wait & (1 << from_inst->control.write)) == 1) {
            barrier_from_nodes.push_back(from_node);
          }
        }

        // If we have barrier instructions, only consider barrier instructions
        auto &biased_from_nodes = barrier_from_nodes.size() > 0 ?
          barrier_from_nodes : from_nodes;
#endif
        
      for (auto *from_node : from_nodes) {
        auto from_vma = from_node->lmIP();
        auto ps = cct_edge_path_map[from_vma][to_vma];

        for (auto &path : ps) {
          auto path_inst = computePathInsts(mpi_rank, thread_id, from_vma, to_vma, path);
          distance[from_node] = MAX2(distance[from_node], path_inst);
        }
        auto issue = from_node->demandMetric(issue_metric_index);
        // Guarantee that issue is not zero
        issue = issue > 0 ? issue : _arch->warp_size();
        issues[from_node] = issue;
        issue_sum += issue;
      }

      // More insts, less hiding possibility
      reverseDistance(distance, insts);

      auto adjust_sum = 0.0;
      // Apportion based on issue and stalls
      for (auto *from_node : from_nodes) {
        auto inst_ratio = insts[from_node];
        auto issue_ratio = issues[from_node] / issue_sum;
        adjust_sum += inst_ratio * issue_ratio;
      }

      for (auto *from_node : from_nodes) {
        auto from_vma = from_node->lmIP();
        auto *from_inst = _vma_prop_map.at(from_vma).inst;
        auto from_struct_iter = _vma_struct_map.upper_bound(from_vma);
        Prof::Struct::ACodeNode *from_struct = NULL;
        if (from_struct_iter != _vma_struct_map.begin()) {
          --from_struct_iter;
          from_struct = from_struct_iter->second;
        }

        auto inst_ratio = insts[from_node];
        auto issue_ratio = issues[from_node] / issue_sum;
        auto stall_blame = stall * inst_ratio * issue_ratio / adjust_sum;
        auto lat_blame = lat * inst_ratio * issue_ratio / adjust_sum;

        std::pair<std::string, std::string> metric_name;
        if (stall_metric_index == exec_stall_metric_index) {
          // detailization: smem, reg_dep, or war
          metric_name = detailizeExecBlame(from_inst, to_inst);
        } else {
          metric_name = detailizeMemBlame(from_inst);
        }

        auto stall_blame_name = "BLAME " + metric_name.first;
        attributeBlameMetric(mpi_rank, thread_id, from_node, stall_blame_name, stall_blame);

        auto lat_blame_name = "BLAME " + metric_name.second;
        attributeBlameMetric(mpi_rank, thread_id, from_node, lat_blame_name, lat_blame);

        // One metric id is enough for inst blame analysis
        inst_blames.emplace_back(InstructionBlame(from_inst, to_inst, from_struct, to_struct,
                                                  distance[from_node], stall_blame,
                                                  lat_blame, lat_blame_name));
      }
    }

    // Inst stall and latencies
    for (auto &s : _inst_metrics) {
      auto stall_metric = s.first;
      auto lat_metric = s.second;

      auto stall_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, stall_metric);
      auto lat_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, lat_metric);

      auto stall = to_node->demandMetric(stall_metric_index);
      auto lat = to_node->demandMetric(lat_metric_index);

      if (lat == 0) {
        continue;
      }

      // blame itself
      auto stall_blame_name = "BLAME " + stall_metric;
      attributeBlameMetric(mpi_rank, thread_id, to_node, stall_blame_name, stall);

      auto lat_blame_name = "BLAME " + lat_metric;
      attributeBlameMetric(mpi_rank, thread_id, to_node, lat_blame_name, lat);

      if (to_struct == NULL) {
        std::cout << "here3" << std::endl;
      }

      // one metric id is enough for inst blame analysis
      inst_blames.emplace_back(
          InstructionBlame(to_inst, to_inst, to_struct, to_struct, 0, stall, lat, lat_blame_name));
    }

    // Sync stall
    auto sync_stall_metric_index = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, _sync_stall_metric);
    auto sync_lat_metric_index = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, _sync_lat_metric);
    auto sync_stall = to_node->demandMetric(sync_stall_metric_index);
    auto sync_lat = to_node->demandMetric(sync_lat_metric_index);

    if (sync_lat == 0) {
      continue;
    }

    // inclusive and exclusive metrics have the same value
    auto sync_vma = to_vma;
    auto *sync_inst = _vma_prop_map.at(sync_vma).inst;

    double distance = 0;
    if (sync_inst->op.find(".SYNC") == std::string::npos) {
      // blame the previous node
      distance = 1;
      sync_vma = sync_vma - _arch->inst_size();
      sync_inst = _vma_prop_map.at(sync_vma).inst;
    }

    auto *sync_node = _vma_prop_map.at(sync_vma).prof_node;
    auto sync_struct_iter = _vma_struct_map.upper_bound(sync_vma);
    Prof::Struct::ACodeNode *sync_struct = NULL;
    if (sync_struct_iter != _vma_struct_map.begin()) {
      --sync_struct_iter;
      sync_struct = sync_struct_iter->second;
    }

    if (sync_node == NULL) {
      // Create a new prof node
      Prof::Metric::IData metric_data(_prof->metricMgr()->size());
      metric_data.clearMetrics();

      sync_node = new Prof::CCT::Stmt(to_node->parent(), HPCRUN_FMT_CCTNodeId_NULL,
        lush_assoc_info_NULL, _gpu_root->lmId(), sync_vma, 0, NULL, metric_data);
    }

    auto stall_blame_name = "BLAME " + _sync_stall_metric;
    attributeBlameMetric(mpi_rank, thread_id, to_node, stall_blame_name, sync_stall);

    auto lat_blame_name = "BLAME " + _sync_lat_metric;
    attributeBlameMetric(mpi_rank, thread_id, to_node, lat_blame_name, sync_lat);

    // one metric id is enough for inst blame analysis
    inst_blames.emplace_back(InstructionBlame(sync_inst, to_inst, sync_struct, to_struct,
        distance, sync_stall, sync_lat, lat_blame_name));
  }
}


void GPUAdvisor::overlayInstBlames(InstBlames &inst_blames, KernelBlame &kernel_blame) {
  for (auto &inst_blame : inst_blames) {
    auto *from_inst = inst_blame.src_inst;
    auto *from_block = _vma_prop_map.at(from_inst->pc).block;
    auto *from_function = _vma_prop_map.at(from_inst->pc).function;
    auto *to_inst = inst_blame.dst_inst;
    auto *to_block = _vma_prop_map.at(to_inst->pc).block;
    auto *to_function = _vma_prop_map.at(to_inst->pc).function;

    // Update block and function
    inst_blame.src_block = from_block;
    inst_blame.src_function = from_function;
    inst_blame.dst_block = to_block;
    inst_blame.dst_function = to_function;

    // Update kernel blame
    kernel_blame.stall_blames[inst_blame.blame_name] += inst_blame.stall_blame;
    kernel_blame.lat_blames[inst_blame.blame_name] += inst_blame.lat_blame;
    kernel_blame.stall_blame += inst_blame.stall_blame;
    kernel_blame.lat_blame += inst_blame.lat_blame;

    kernel_blame.inst_blames.push_back(inst_blame);
  }

  for (auto &inst_blame : kernel_blame.inst_blames) {
    kernel_blame.lat_inst_blame_ptrs.push_back(&inst_blame);
    kernel_blame.stall_inst_blame_ptrs.push_back(&inst_blame);
  }

  std::sort(kernel_blame.lat_inst_blame_ptrs.begin(), kernel_blame.lat_inst_blame_ptrs.end(),
    InstructionBlameLatComparator());
  std::sort(kernel_blame.stall_inst_blame_ptrs.begin(), kernel_blame.stall_inst_blame_ptrs.end(),
    InstructionBlameStallComparator());
}


void GPUAdvisor::blame(CCTBlames &cct_blames) {
  // For each MPI process
  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks();
    ++mpi_rank) {
    // For each CPU thread
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank);
      ++thread_id) {
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) == -1) {
        // Skip tracing threads
        continue;
      }

      bool gpu_metric = false;
      auto inst_metric_index = _metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric);
      for (auto &iter : _vma_prop_map) {
        auto *prof_node = iter.second.prof_node;
        if (prof_node != NULL && prof_node->demandMetric(inst_metric_index) != 0.0) {
          gpu_metric = true;
          break;
        }
      }
      if (gpu_metric == false) {
        // Skip CPU threads without GPU kernels
        continue;
      }

      if (DEBUG_GPUADVISOR) {
        std::cout << "[" << mpi_rank << "," << thread_id << "]" << std::endl << std::endl;
      }

      // 1. Init a CCT dependency graph
      CCTGraph<Prof::CCT::ADynNode *> cct_dep_graph;
      initCCTDepGraph(mpi_rank, thread_id, cct_dep_graph);

      if (DEBUG_GPUADVISOR) {
        std::cout << "CCT dependency graph summary: " << std::endl;
        debugCCTDepGraphSummary(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      if (DEBUG_GPUADVISOR_DETAILS) {
        std::cout << "CCT dependency graph after propgation: " << std::endl;
        debugCCTDepGraph(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      // 2. Prune cold paths in CCT graph
      // 2.1 Opcode constraints
      pruneCCTDepGraphOpcode(mpi_rank, thread_id, cct_dep_graph);

      if (DEBUG_GPUADVISOR_DETAILS) {
        std::cout << "CCT dependency graph after opcode pruning: " << std::endl;
        debugCCTDepGraph(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      // 2.2 Barrier constraints
      pruneCCTDepGraphBarrier(mpi_rank, thread_id, cct_dep_graph);

      if (DEBUG_GPUADVISOR_DETAILS) {
        std::cout << "CCT dependency graph after barrier pruning: " << std::endl;
        debugCCTDepGraph(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      // 2.3 Issue constraints
      CCTEdgePathMap cct_edge_path_map;
      pruneCCTDepGraphLatency(mpi_rank, thread_id, cct_dep_graph, cct_edge_path_map);

      if (DEBUG_GPUADVISOR_DETAILS) {
        std::cout << "CCT dependency graph after latency pruning: " << std::endl;
        debugCCTDepGraph(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      if (DEBUG_GPUADVISOR_DETAILS) {
        std::cout << "CCT pure stall: " << std::endl;
        debugCCTDepGraphStallExec(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      if (DEBUG_GPUADVISOR_DETAILS) {
        std::cout << "CCT no path: " << std::endl;
        debugCCTDepGraphNoPath(mpi_rank, thread_id, cct_dep_graph);
        std::cout << std::endl;
      }

      // 3. Accumulate blames and record significant pairs and paths
      // Apportion based on block latency coverage and def inst issue count
      InstBlames inst_blames;
      blameCCTDepGraph(mpi_rank, thread_id, cct_dep_graph, cct_edge_path_map, inst_blames);

      if (DEBUG_GPUADVISOR) {
        std::cout << "Inst blames: " << std::endl;
        debugInstBlames(inst_blames);
        std::cout << std::endl;
      }

      // 5. Overlay blames
      auto &kernel_blame = cct_blames[mpi_rank][thread_id];
      overlayInstBlames(inst_blames, kernel_blame);
    }
  }
}


}  // namespace Analysis
