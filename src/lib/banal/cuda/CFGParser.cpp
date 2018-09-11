#include "CFGParser.hpp"
#include <cctype>
#include <iostream>

#define DEBUG_CUDA_CFGPARSER 1


namespace CudaParse {

static void debug_blocks(const std::vector<Block *> &blocks) {
  for (auto *block : blocks) {
    std::cout << "Block id: " << block->id <<
      " , name: " << block->name << std::endl;
    std::cout << "Range: [" << block->insts[0]->offset <<
      ", " << block->insts.back()->offset << "]" << std::endl;
  }
}


TargetType CFGParser::get_target_type(const Block *source_block, Inst *inst) {
  TargetType type;
  if (inst->predicate.find("!@") != std::string::npos) {
    type = COND_TAKEN;
  } else if (inst->predicate.find("@") != std::string::npos) {
    type = COND_NOT_TAKEN;
  } else if (inst == source_block->insts.back()) {
    type = FALLTHROUGH;
  } else {
    type = DIRECT;
  }
  return type;
}


void CFGParser::parse_inst_strings(
  const std::string &label,
  std::deque<std::string> &inst_strings) {
  std::regex e("\\\\l([|]*)");
  std::istringstream ss(std::regex_replace(label, e, "\n"));
  std::string s;
  while (std::getline(ss, s)) {
    inst_strings.push_back(s);
  }
  while (inst_strings.size() > 0) {
    if (isdigit(inst_strings.front()[0]) || inst_strings.front()[0] == '<') {
      break;
    }
    inst_strings.pop_front();
  }
  inst_strings.pop_back();
}


void CFGParser::parse_calls(std::vector<Function *> &functions) {
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->is_call()) {
          std::string &operand = inst->operands[0];
          Function *callee_function = 0;
          for (auto *ff : functions) {
            if (ff->name == operand) {
              callee_function = ff;
              break;
            }
          }
          if (callee_function != 0) 
            block->targets.push_back(new Target(inst, callee_function->blocks[0], CALL));
          else {
            std::cout << "warning: CUBIN function " << operand << " not found" << std::endl; 
          }
        }
      }
    }
  }
}


size_t CFGParser::find_block_parent(size_t node) {
  size_t parent = _block_parent[node];
  size_t block_size = _block_parent.size();
  if (parent == block_size) {
    return _block_parent[node] = node;
  } else if (parent == node) {
    return node;
  } else {
    return _block_parent[node] = find_block_parent(parent);
  }
}


void CFGParser::unite_blocks(size_t l, size_t r) {
  _block_parent[l] = find_block_parent(r);
}


static bool compare_block_ptr(Block *l, Block *r) {
  return *l < *r;
}


static bool compare_target_ptr(Target *l, Target *r) {
  return *l < *r;
}


void CFGParser::parse(const Graph &graph, std::vector<Function *> &functions) {
  std::unordered_map<size_t, Block *> block_map;
  std::vector<Block *> blocks;
  std::vector<std::pair<size_t, size_t> > edges;

  // Parse every vertex to build blocks
  for (auto *vertex : graph.vertices) {
    Block *block = new Block(vertex->id, vertex->name);

    std::deque<std::string> inst_strings;
    parse_inst_strings(vertex->label, inst_strings);
    for (auto &inst_string : inst_strings) {
      block->insts.push_back(new Inst(inst_string));
    }

    blocks.push_back(block);
    block_map[block->id] = block;
  }

  // Parse every edge to build block relations
  for (auto *edge : graph.edges) {
    edges.push_back(std::pair<size_t, size_t>(edge->source_id, edge->target_id));
    Block *target_block = block_map[edge->target_id];
    Block *source_block = block_map[edge->source_id];
    
    // Link blocks
    for (size_t i = 0; i < source_block->insts.size(); ++i) {
      Inst *inst = source_block->insts[i];
      if (inst->port == edge->source_port[0]) {
        if (inst->dual_first == true || inst->dual_second == true) {
          Inst *dual_inst = NULL;
          if (inst->dual_first) {
            dual_inst = source_block->insts[i + 1];
          } else if (inst->dual_second) {
            dual_inst = source_block->insts[i - 1];
          }
          TargetType type = get_target_type(source_block, inst);
          TargetType dual_type = get_target_type(source_block, dual_inst);
          if (type == COND_TAKEN || type == COND_NOT_TAKEN) {
            source_block->targets.push_back(new Target(inst, target_block, type));
          } else if (dual_type == COND_TAKEN || dual_type == COND_NOT_TAKEN) {
            source_block->targets.push_back(new Target(dual_inst, target_block, dual_type));
          } else {
            source_block->targets.push_back(new Target(inst, target_block, type));
          }
        } else {
          TargetType type = get_target_type(source_block, inst);
          source_block->targets.push_back(new Target(inst, target_block, type));
        }
      }
    }
    // Some edges may not have port information
    if (source_block->targets.size() == 0) {
      Inst *inst = source_block->insts.back();
      TargetType type;
      if (inst->is_call()) {
        type = CALL_FT;
      } else {
        type = FALLTHROUGH;
      }
      source_block->targets.push_back(new Target(inst, target_block, type));
    }
  }

#ifdef DEBUG_CUDA_CFGPARSER
  std::cout << "Before split" << std::endl;
  debug_blocks(blocks);
#endif

  // Split blocks for loop analysis ans CALL instructions
  // TODO(keren): identify RET edges?
  split_blocks(edges, blocks, block_map);

#ifdef DEBUG_CUDA_CFGPARSER
  std::cout << "After split" << std::endl;
  debug_blocks(blocks);
#endif

  // Resize before union find
  _block_parent.resize(blocks.size());
  for (size_t i = 0; i < blocks.size(); ++i) {
    _block_parent[i] = blocks.size();
  }

  // Find toppest block
  for (size_t i = 0; i < edges.size(); ++i) {
    auto source_id = edges[i].first;
    auto target_id = edges[i].second;
    unite_blocks(target_id, source_id);
  }

  // Build functions
  size_t function_id = 0;
  for (auto *block : blocks) {
    // Sort block targets according to inst offset
    std::sort(block->targets.begin(), block->targets.end(), compare_target_ptr);
    if (find_block_parent(block->id) == block->id) {
      // Filter out self contained useless loops. A normal function will not start with "."
      if (block_map[block->id]->name[0] == '.') {
        continue;
      }
      Function *function = new Function(function_id, block_map[block->id]->name);
      ++function_id;
      for (auto *bb : blocks) {
        if (find_block_parent(bb->id) == block->id) {
          function->blocks.push_back(bb);
        }
      }
      std::sort(function->blocks.begin(), function->blocks.end(), compare_block_ptr);
      int begin_offset = function->blocks[0]->insts[0]->offset;
      function->begin_offset = begin_offset == 8 ? 0 : begin_offset;
      functions.push_back(function);
    }
  }

  parse_calls(functions);
}


void CFGParser::split_blocks(
  std::vector<std::pair<size_t, size_t> > &edges,
  std::vector<Block *> &blocks,
  std::unordered_map<size_t, Block *> &block_map) {
  size_t extra_block_id = blocks.size();
  std::vector<Block *> candidate_blocks;
  for (auto *block : blocks) {
    std::vector<size_t> split_inst_index;
    // Step 1: filter out all branch instructions
    for (size_t i = 0; i < block->insts.size(); ++i) {
      Inst *inst = block->insts[i];
      if (inst->is_call()) {
        split_inst_index.push_back(i);
        continue;
      }
      for (size_t j = 0; j < block->targets.size(); ++j) {
        if (block->insts[i] == block->targets[j]->inst) {
          split_inst_index.push_back(i);
          break;
        }
      }
    }

#ifdef DEBUG_CUDA_CFGPARSER
    std::cout << "Split index:" << std::endl;
    for (size_t i = 0; i < split_inst_index.size(); ++i) {
      std::cout << "Block: " << block->name <<
        ", offset: " << block->insts[i]->offset << std::endl;
    }
#endif

    if (split_inst_index.size() != 0) {
      size_t cur_block_id = 1;
      size_t prev_inst_index = 0;
      std::vector<std::pair<size_t, size_t> > intervals;
      // Step 2: for each branch instrution, create a block except for the last inst
      for (size_t i = 0; i < split_inst_index.size(); ++i) {
        // Update interval
        auto index = split_inst_index[i];
        intervals.push_back(std::pair<size_t, size_t>(prev_inst_index, index));
        prev_inst_index = index + 1;
      }
      std::vector<Inst *> insts = block->insts;
      if (prev_inst_index < insts.size()) {
        intervals.push_back(std::pair<size_t, size_t>(prev_inst_index, insts.size() - 1));
      }
#ifdef DEBUG_CUDA_CFGPARSER
      std::cout << "Intervals:" << std::endl;
      for (size_t i = 0; i < intervals.size(); ++i) {
        std::cout << "[" << intervals[i].first << ", " << intervals[i].second << "]" << std::endl;
      }
#endif
      // Step 3: update block instructions
      block->insts.clear();
      std::vector<Block *> new_blocks;
      for (size_t i = 0; i < intervals.size(); ++i) {
        Block *new_block;
        if (i == 0) {
          new_block = block;
        } else {
          std::string block_name = block->name + "_" + std::to_string(cur_block_id++);
          new_block = new Block(extra_block_id++, block_name);
          // Update block records
          candidate_blocks.push_back(new_block);
          block_map[new_block->id] = new_block;
        }
        new_blocks.push_back(new_block);
        // Update instructions
        size_t start_index = intervals[i].first;
        size_t end_index = intervals[i].second;
        for (size_t st = start_index; st < end_index + 1; ++st) {
          new_block->insts.push_back(insts[st]);
        }
      }
      // Step 4: update block targets
      std::vector<Target *> targets = block->targets;
      block->targets.clear();
      for (size_t i = 0; i < new_blocks.size(); ++i) {
        auto *new_block = new_blocks[i];
        if (i == new_blocks.size() - 1) {
          for (auto *target : targets) {
            if (target->inst == new_block->insts.back()) {
              new_block->targets.push_back(target);
            }
          }
        } else {
          // A conditional call is treated by branch split, not call split
          bool call_split = true;
          Target *target = NULL;
          for (size_t j = 0; j < targets.size(); ++j) {
            if (targets[j]->inst == new_block->insts.back()) {
              target = targets[j];
              call_split = false;
              break;
            }
          }
          if (call_split) {
            TargetType next_block_type = CALL_FT;
            Block *next_block = new_blocks[i + 1];
            new_block->targets.push_back(new Target(
                new_block->insts.back(), next_block, next_block_type));
            // Push back new edge
            edges.push_back(std::pair<size_t, size_t>(new_block->id, next_block->id));
          } else {
            TargetType next_block_type;
            TargetType target_block_type;
            Block *next_block = new_blocks[i + 1];

            if (target->type == COND_TAKEN) {
              target_block_type = COND_TAKEN;
              next_block_type = COND_NOT_TAKEN;
            } else if (target->type == COND_NOT_TAKEN) {
              target_block_type = COND_NOT_TAKEN;
              next_block_type = COND_TAKEN;
            } else {
              target_block_type = DIRECT;
              next_block_type = INDIRECT;
            }

            new_block->targets.push_back(new Target(
                target->inst, next_block, next_block_type));
            new_block->targets.push_back(new Target(
                target->inst, target->block, target_block_type));
            // Push back new edge
            edges.push_back(std::pair<size_t, size_t>(new_block->id, next_block->id));
          }
        }
      }
    }
  }
  // Add created blocks into block list
  for (auto *block : candidate_blocks) {
    blocks.push_back(block);
  }
}


}
