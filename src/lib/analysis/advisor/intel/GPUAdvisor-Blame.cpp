#include "GPUAdvisor.hpp"
//#include <lib/prof/CCT-Tree.hpp>                    // ADynNode
#include <lib/analysis/CCTGraph.hpp>                // CCTGraph


void
GPUAdvisor::initCCTDepGraph
(
 CCTGraph &cct_dep_graph
)
{
  std::vector<std::pair<int, int>> edge_vec;
  // Insert all possible edges
  // If a node has samples, we find all possible causes.
  for (auto &iter : _node_map) {
    auto *inst = iter.second.inst;
    int to = inst->pc;
    cct_dep_graph.addNode(iter.second);
    for (auto reg_vector: inst->assign_pcs) {
      for (int from: reg_vector.second) {
        edge_vec.push_back(std::pair<int,int>(from, to));
        cct_dep_graph.addNode(_node_map[from]);
      }
    }
  }

  for (auto &edge : edge_vec) {
    int from_node = edge.first;
    int to_node = edge.second;

    cct_dep_graph.addEdge(from_node, to_node);
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
    auto *inst = _node_map.at(start_vma).inst;
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
  auto *from_block = _node_map.at(from_vma).block;
  auto *to_block = _node_map.at(to_vma).block;
  std::set<GPUParse::Block *> visited_blocks;
  std::vector<GPUParse::Block *> path;
  std::vector<std::vector<GPUParse::Block *>> paths;
  auto latency = _node_map.at(from_vma).latency;
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
 CCTGraph &cct_dep_graph,
 CCTEdgePathMap &cct_edge_path_map
)
{
  // commented for now
#if 0
  std::vector<std::set<Analysis::CCTEdge<Prof::CCT::ADynNode *>>::iterator> remove_edges;
  for (auto iter = cct_dep_graph.edgeBegin(); iter != cct_dep_graph.edgeEnd(); ++iter) {
    auto edge = *iter;
    auto *from = edge.from;
    auto from_vma = from->lmIP();
    auto *to = edge.to;
    auto to_vma = to->lmIP();
    auto *to_inst = _node_map.at(to_vma).inst;
    auto *from_inst = _node_map.at(from_vma).inst;
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
#endif
}


void
GPUAdvisor::blameCCTDepGraph
(
 CCTGraph &cct_dep_graph,
 CCTEdgePathMap &cct_edge_path_map,
 InstBlames &inst_blames
)
{
  // latency blaming

  // nodes = sorted list of def-use vertices
  std::map<int, CCTNode> nodes = cct_dep_graph.nodes();
  std::map<int, std::set<int> > incoming_nodes = cct_dep_graph.incoming_nodes();
  for (auto iter = nodes.rbegin(); iter != nodes.rend(); ++iter) {  
    CCTNode n = iter->second;
    float sum_freq_path_inv = 0;
    for (auto &i: incoming_nodes[n.vma]) {
      CCTNode i_node = nodes[i];
      sum_freq_path_inv += i_node.frequency * i_node.path_length_inv;
    }
    for (auto &i: incoming_nodes[n.vma]) {
      // latency_blame calculations for incoming node i
      CCTNode &i_node = nodes[i];
      i_node.latency_blame += (i_node.frequency * i_node.path_length_inv * n.latency / sum_freq_path_inv);
      // for debugging
      _node_map[i].latency_blame = i_node.latency_blame;
    }
  }
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
    auto *from_block = _node_map.at(from_inst->pc).block;
    auto *from_function = _node_map.at(from_inst->pc).function;
    auto *to_inst = inst_blame.dst_inst;
    auto *to_block = _node_map.at(to_inst->pc).block;
    auto *to_function = _node_map.at(to_inst->pc).function;

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
GPUAdvisor::printNodes
(
 CCTGraph cct_dep_graph
)
{
  std::map<int, std::set<int> > incoming_nodes = cct_dep_graph.incoming_nodes();
  std::map<int, CCTNode > nodes = cct_dep_graph.nodes();
  for (auto &iter : _node_map) {
    std::set<int> iter_incoming_nodes = incoming_nodes[iter.second.vma];
    std::cout << "offset: " << iter.second.vma <<
                 ", latency: " << iter.second.latency <<
                 ", latency_blame: " << iter.second.latency_blame <<
                 ", path_length: " << nodes[iter.second.vma].path_length <<
                 ", incoming edges: [";
    for (int i_node : iter_incoming_nodes) {
      std::cout << i_node << ", ";
    }
    std::cout << "]\n";
  }
}


void
GPUAdvisor::blame
(
 KernelBlame &kernel_blame
)
{
  // 1. Init a CCT dependency graph
  CCTGraph cct_dep_graph;
  initCCTDepGraph(cct_dep_graph);

  // 2.3 Issue constraints
  CCTEdgePathMap cct_edge_path_map;
  //pruneCCTDepGraphLatency(cct_dep_graph, cct_edge_path_map);

  // 3. Accumulate blames and record significant pairs and paths
  // Apportion based on block latency coverage and def inst issue count
  InstBlames inst_blames;
  blameCCTDepGraph(cct_dep_graph, cct_edge_path_map, inst_blames);

  printNodes(cct_dep_graph);
  // 5. Overlay blames
  //auto &kernel_blame = cct_blames[mpi_rank][thread_id];
  //overlayInstBlames(inst_blames, kernel_blame);
}
