#include "GPUAdvisor.hpp"
#include <lib/prof/CCT-Tree.hpp>                    // ADynNode
#include <lib/analysis/advisor/intel/CCTGraph.hpp>  // CCTGraph


void
GPUAdvisor::initCCTDepGraph
(
 Analysis::CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph
)
{
  // Init nodes
  for (auto &iter : _vma_prop_map) {
    auto *prof_node = iter.second.prof_node;
    if (prof_node != NULL) {
      cct_dep_graph.addNode(prof_node);
    }
  }

  auto inst_exe_pred_index =
    _metric_name_prof_map->metric_id(0, 0, _inst_exe_pred_metric);  // mpi rank and thread id set to 0

  std::vector<std::pair<Prof::CCT::ADynNode *, Prof::CCT::ADynNode *>> edge_vec;
  // Insert all possible edges
  // If a node has samples, we find all possible causes.
  // Even for instructions that are not sampled, we assign them one issued sample, but no latency
  // sample. Since no latency sample is associated with these nodes, we do not need to propogate in
  // the grpah.
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

        if (inst_exe_pred_index != -1) {
          // Accurate mode
          if (vma_prop_iter->second.prof_node != NULL &&
              vma_prop_iter->second.prof_node->hasMetricSlow(inst_exe_pred_index)) {
            // Only add prof node it is executed
            prof_node = vma_prop_iter->second.prof_node;
          }
        } else {
          // Fast mode
          if (vma_prop_iter->second.prof_node == NULL) {
            // Create a new prof node
            Prof::Metric::IData metric_data(_prof->metricMgr()->size());
            metric_data.clearMetrics();
            prof_node =
              new Prof::CCT::Stmt(node->parent(), HPCRUN_FMT_CCTNodeId_NULL, lush_assoc_info_NULL,
                  _gpu_root->lmId(), vma, 0, NULL, metric_data);
            vma_prop_iter->second.prof_node = prof_node;
          } else {
            prof_node = vma_prop_iter->second.prof_node;
          }
        }

        if (prof_node != NULL) {
          edge_vec.push_back(
              std::pair<Prof::CCT::ADynNode *, Prof::CCT::ADynNode *>(prof_node, node));
        }
      }
    }
  }

  for (auto &edge : edge_vec) {
    auto *from_node = edge.first;
    auto *to_node = edge.second;

    cct_dep_graph.addEdge(from_node, to_node);
  }

  auto lat_index = _metric_name_prof_map->metric_id(0, 0, _lat_metric);  // mpi rank and thread id set to 0

  // Special handling for depbar
  for (auto iter = cct_dep_graph.nodeBegin(); iter != cct_dep_graph.nodeEnd(); ++iter) {
    auto *node = *iter;
    auto node_vma = node->lmIP();
    auto *prof_node = _vma_prop_map.at(node_vma).prof_node;

    if (prof_node != NULL && cct_dep_graph.incoming_nodes_size(node) == 0 &&
        prof_node->hasMetricSlow(lat_index) != 0.0) {
      auto bar_vma = node_vma - _arch->inst_size();
      auto bar_vma_prop_iter = _vma_prop_map.find(bar_vma);
      auto *bar_inst = bar_vma_prop_iter->second.inst;

      if (bar_inst->op.find("BAR") == std::string::npos) {
        continue;
      }

      decltype(prof_node) bar_node = bar_vma_prop_iter->second.prof_node;
      if (bar_node == NULL) {
        // Create a node
        Prof::Metric::IData metric_data(_prof->metricMgr()->size());
        metric_data.clearMetrics();
        bar_node = new Prof::CCT::Stmt(node->parent(), HPCRUN_FMT_CCTNodeId_NULL, lush_assoc_info_NULL,
            _gpu_root->lmId(), bar_vma, 0, NULL, metric_data);
        bar_vma_prop_iter->second.prof_node = bar_node;
      }

      // Move mem dep metrics
      bar_node->demandMetric(lat_index) = prof_node->demandMetric(lat_index);
      prof_node->demandMetric(lat_index) = 0;
    }
  }
}


void
GPUAdvisor::trackDep
(
 int from_vma,
 int to_vma,
 int id,
 GPUParse::Block *from_block,
 GPUParse::Block *to_block,
 int latency_issue,
 int latency,
 std::set<GPUParse::Block *> &visited_blocks,
 std::vector<GPUParse::Block *> &path,
 std::vector<std::vector<GPUParse::Block *>> &paths,
 TrackType track_type,
 bool fixed,
 int barrier_threshold
)
{
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
  bool skip = false;
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
  if (from_vma <= back_vma && from_vma >= front_vma && to_vma <= back_vma && to_vma >= front_vma &&
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
      latency_issue += inst->control.stall;
    }

    // 1. id on path constraint
    bool find = false;
    if (track_type == TRACK_REG) {
      find = inst->find_src_reg(id);
    } else if (track_type == TRACK_PRED_REG) {
      find = inst->find_src_pred_reg(id);
    } else if (track_type == TRACK_PREDICATE) {
      find = (inst->predicate == id || inst->find_src_pred_reg(id));
    } else if (track_type == TRACK_UNIFORM) {
      find = inst->find_src_ureg(id);
    } else {  // track_type == TRACK_BARRIER
      find = inst->find_src_barrier(id);
    }

    if (find) {
      if (barrier_threshold == -1) {
        find_def = true;
        break;
      }
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
    if (to_vma <= back_vma && to_vma >= front_vma && loop_block == false && skip == false) {
      paths.push_back(path);
    } else {
      for (auto *target : from_block->targets) {
        if (target->type != GPUParse::TargetType::CALL) {
          // We do not need from_vma anymore
          trackDep(0, to_vma, id, target->block, to_block, latency_issue, latency, visited_blocks,
              path, paths, track_type, fixed, barrier_threshold);
        }
      }
    }
  }

  visited_blocks.erase(from_block);
  path.pop_back();
}


void GPUAdvisor::trackDepInit(int to_vma, int from_vma, int dst, CCTEdgePathMap &cct_edge_path_map,
                              TrackType track_type, bool fixed, int barrier_threshold) {
  // Search for all possible paths from dst to src.
  // Since the single path rate here is expected to be high,
  // The following code section only executes few times.
  // Two constraints:
  // 1. A path is eliminated if an instruction on the way uses dst
  // 2. A path is eliminated if the throughput cycles is greater than latency
  auto *from_block = _vma_prop_map.at(from_vma).block;
  auto *to_block = _vma_prop_map.at(to_vma).block;
  std::set<GPUParse::Block *> visited_blocks;
  std::vector<GPUParse::Block *> path;
  std::vector<std::vector<GPUParse::Block *>> paths;
  auto latency = fixed ? _vma_prop_map.at(from_vma).latency_lower : _vma_prop_map.at(from_vma).latency_upper;
  trackDep(from_vma, to_vma, dst, from_block, to_block, 0, latency, visited_blocks, path, paths,
      track_type, fixed, barrier_threshold);

  std::vector<bool> new_paths;
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
    new_paths.push_back(new_path);
  }

  for (size_t i = 0; i < paths.size(); ++i) {
    if (new_paths[i]) {
      cct_edge_path_map[from_vma][to_vma].push_back(paths[i]);
    }
  }
}


void
GPUAdvisor::pruneCCTDepGraphLatency
(
 Analysis::CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
 CCTEdgePathMap &cct_edge_path_map
)
{
  std::vector<std::set<Analysis::CCTEdge<Prof::CCT::ADynNode *>>::iterator> remove_edges;
  for (auto iter = cct_dep_graph.edgeBegin(); iter != cct_dep_graph.edgeEnd(); ++iter) {
    auto edge = *iter;
    auto *from = edge.from;
    auto from_vma = from->lmIP();
    auto *to = edge.to;
    auto to_vma = to->lmIP();
    auto *to_inst = _vma_prop_map.at(to_vma).inst;
    auto *from_inst = _vma_prop_map.at(from_vma).inst;
    auto barrier_threshold = to_inst->barrier_threshold;
    bool fixed = from_inst->control.read == GPUParse::InstructionStat::BARRIER_NONE &&
      from_inst->control.write == GPUParse::InstructionStat::BARRIER_NONE;

    // Regular regs
    for (auto dst : from_inst->dsts) {
      // Find the dst reg that causes dependency
      auto assign_iter = to_inst->assign_pcs.find(dst);
      if (assign_iter != to_inst->assign_pcs.end()) {
        trackDepInit(to_vma, from_vma, dst, cct_edge_path_map, TRACK_REG, fixed, -1);
      }
    }

    // Predicate regs
    for (auto pdst : from_inst->pdsts) {
      auto passign_iter = to_inst->passign_pcs.find(pdst);
      if (passign_iter != to_inst->passign_pcs.end()) {
        trackDepInit(to_vma, from_vma, pdst, cct_edge_path_map, TRACK_PRED_REG, fixed, -1);
      }
    }

    // Barrier
    for (auto bdst : from_inst->bdsts) {
      // Find the dst barrier that causes dependency
      auto bassign_iter = to_inst->bassign_pcs.find(bdst);
      if (bassign_iter != to_inst->bassign_pcs.end()) {
        trackDepInit(to_vma, from_vma, bdst, cct_edge_path_map, TRACK_BARRIER, fixed, barrier_threshold);
      }    
    }    

    // Uniform registers
    for (auto udst : from_inst->udsts) {
      // Find the dst that causes dependency
      auto uassign_iter = to_inst->uassign_pcs.find(udst);
      if (uassign_iter != to_inst->uassign_pcs.end()) {
        trackDepInit(to_vma, from_vma, udst, cct_edge_path_map, TRACK_UNIFORM, fixed, -1); 
      }    
    }    

    // Predicate
    for (auto pred_dst : from_inst->pdsts) {
      // Find the dst barrier that causes dependency
      if (to_inst->predicate == pred_dst) {
        trackDepInit(to_vma, from_vma, pred_dst, cct_edge_path_map, TRACK_PREDICATE, fixed, -1); 
      }    
    }    

    // If there's no satisfied path
    if (cct_edge_path_map[from_vma][to_vma].size() == 0) { 
      // This edge can be removed
      remove_edges.push_back(iter);
    }    
  }

  for (auto &iter : remove_edges) {
    cct_dep_graph.removeEdge(iter);
  }
}


void
GPUAdvisor::blameCCTDepGraph
(
 Analysis::CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
 CCTEdgePathMap &cct_edge_path_map,
 InstBlames &inst_blames
)
{
  // to be filled
}


void
GPUAdvisor::overlayInstBlames
(
 InstBlames &inst_blames,
 KernelBlame &kernel_blame
)
{
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
    kernel_blame.lat_blames[inst_blame.blame_name] += inst_blame.lat_blame;
    kernel_blame.lat_blame += inst_blame.lat_blame;

    kernel_blame.inst_blames.push_back(inst_blame);
  }

  for (auto &inst_blame : kernel_blame.inst_blames) {
    kernel_blame.lat_inst_blame_ptrs.push_back(&inst_blame);
  }

  std::sort(kernel_blame.lat_inst_blame_ptrs.begin(), kernel_blame.lat_inst_blame_ptrs.end(),
            InstructionBlameLatComparator());
}


void
GPUAdvisor::blame
(
 KernelBlame &kernel_blame
)
{
  // 1. Init a CCT dependency graph
  Analysis::CCTGraph<Prof::CCT::ADynNode *> cct_dep_graph;
  initCCTDepGraph(cct_dep_graph);

  // 2.3 Issue constraints
  CCTEdgePathMap cct_edge_path_map;
  pruneCCTDepGraphLatency(cct_dep_graph, cct_edge_path_map);

  // 3. Accumulate blames and record significant pairs and paths
  // Apportion based on block latency coverage and def inst issue count
  InstBlames inst_blames;
  blameCCTDepGraph(cct_dep_graph, cct_edge_path_map, inst_blames);

  // 5. Overlay blames
  //auto &kernel_blame = cct_blames[mpi_rank][thread_id];
  overlayInstBlames(inst_blames, kernel_blame);
}
