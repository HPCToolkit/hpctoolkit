#include "CFGParser.hpp"
#include <cctype>
#include <iostream>

#define DEBUG_CUDA_CFGPARSER 0


namespace CudaParse {

static void debug_blocks(const std::vector<Block *> &blocks) {
  for (auto *block : blocks) {
    std::cout << "Block id: " << block->id <<
      " , name: " << block->name << std::endl;
    std::cout << "Range: [" << block->insts[0]->offset <<
      ", " << block->insts.back()->offset << "]" << std::endl;
    for (auto *target : block->targets) {
      std::cout << "Target block id: " << target->block->id <<
        " , name: " << target->block->name << std::endl;
    }
    std::cout << std::endl;
  }
}


TargetType CFGParser::get_target_type(const Inst *inst) {
  TargetType type;
  if (inst->predicate.find("@") != std::string::npos) {
    type = TargetType::COND_TAKEN;
  } else if (inst->is_call) {
    type = TargetType::CALL;
  } else {
    type = TargetType::DIRECT;
  }
  return type;
}


TargetType CFGParser::get_fallthrough_type(const Inst *inst) {
  TargetType type;
  if (inst->is_call) {
    type = TargetType::CALL_FT;
  } else {
    type = TargetType::DIRECT;
  }
  return type;
}


void CFGParser::parse_inst_strings(
  const std::string &label,
  std::deque<std::string> &inst_strings) {
  std::regex e("\\\\l([|]*)");
  std::string newline("\n");
  std::istringstream ss(std::regex_replace(label, e, newline));
  std::string s;
  while (std::getline(ss, s)) {
    inst_strings.push_back(s);
  }

  // The first and the last string must start with '{' and '}' accordingly
  inst_strings.pop_front();
  inst_strings.pop_back();

  int last_invalid_string_index = -1;
  for (size_t i = 0; i < inst_strings.size(); ++i) {
    if (inst_strings[i].size() == 0) {
      last_invalid_string_index = i;
    } else if (inst_strings[i][0] == '<') {
      // sth. like <exit>00f0
      break;
    } else {
      // Validate if the offset is a hex number, we can do sth. smarter, but we don't have to right now
      std::istringstream iss(inst_strings[i]);
      if (std::getline(iss, s, ':')) {
        if (std::all_of(s.begin(), s.end(), ::isxdigit) == false) {
          last_invalid_string_index = i;
        } else {
          // Must have a instruction followed
          if (std::getline(iss, s, ':')) {
            break;
          }
          // Might be a funny function name like abcd
          last_invalid_string_index = i;
        }
      }
    }
  }
  for (int i = 0; i <= last_invalid_string_index; ++i) {
    inst_strings.pop_front();
  }
}


void CFGParser::link_dangling_blocks(
  std::set<Block *> &dangling_blocks,
  std::vector<Function *> &functions) {
  for (auto *function : functions) {
    bool find = true;
    // Find a matched dangling_block and insert it
    // A while loop is for the case when we have a chain of dangling blocks not in sorted order
    while (find) {
      find = false;
      for (auto iter = dangling_blocks.begin(); iter != dangling_blocks.end(); ++iter) {
        auto *dangling_block = *iter;
        for (auto *block : function->blocks) {
          auto next_offset1 = block->insts.back()->offset + 8;
          auto next_offset2 = block->insts.back()->offset + 16;
          auto prev_offset1 = block->insts.front()->offset - 8;
          auto prev_offset2 = block->insts.front()->offset - 16;
          if (dangling_block->insts.front()->offset == next_offset1 ||
            dangling_block->insts.front()->offset == next_offset2 ||
            dangling_block->insts.back()->offset == prev_offset1 ||
            dangling_block->insts.back()->offset == prev_offset2) {
            // block->dangling_block
            // Either block.address > dangling_block.address or dangling_block.address > block.address
            bool duplicate = false;
            for (auto *b : function->blocks) {
              if (dangling_block->insts.back()->offset >= b->insts.front()->offset &&
                dangling_block->insts.front()->offset <= b->insts.back()->offset) {
                // Find existing inst, skip
                duplicate = true;
                break;
              }
            }
            if (!duplicate) {
              find = true;
              block->targets.push_back(
                new Target(block->insts.back(), dangling_block, TargetType::DIRECT));
            }
            // Either we inserted the target or we found a duplicate
            break;
          }
        }
        if (find) {
          function->blocks.push_back(dangling_block);
          dangling_blocks.erase(iter);
          break;
        }
      }
    }
  }
}


void CFGParser::parse_calls(std::vector<Function *> &functions) {
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->is_call) {
          std::string &operand = inst->operands[0];
          Function *callee_function = 0;
          for (auto *ff : functions) {
            if (ff->name == operand) {
              callee_function = ff;
              break;
            }
          }
          if (callee_function != 0) {
            bool find_target = false;
            Target *t = NULL;
            for (auto *target : block->targets) {
              if (target->inst == inst && target->type == TargetType::CALL) {
                // If a target already exists, an inst cannot point to multiple targets
                find_target = true;
                t = target;
                break;
              }
            }
            if (!find_target) {
              if (DEBUG_CUDA_CFGPARSER) {
                std::cout << "Function (" << function->name << ") call (" << callee_function->name << ") at " << inst->offset << std::endl;
              }
              block->targets.push_back(new Target(inst, callee_function->blocks[0], TargetType::CALL));
            } else {
              if (DEBUG_CUDA_CFGPARSER) {
                std::cout << "Call link exists (" << function->name << ") call (" << callee_function->name << ") at " << inst->offset << " " << t->type << std::endl;
              }
            }
          } else {
            if (DEBUG_CUDA_CFGPARSER) {
              std::cout << "warning: CUBIN function " << operand << " not found" << std::endl; 
            }
          }
        }
      }
    }
  }
}


void CFGParser::find_block_parent(const std::vector<Block *> &blocks) {
  bool incoming_nodes[blocks.size()];
  std::fill(incoming_nodes, incoming_nodes + blocks.size(), false);
  for (auto *block : blocks) {
    for (auto *target : block->targets) {
      incoming_nodes[target->block->id] = true;
    }
  }
  bool visited[blocks.size()];
  std::fill(visited, visited + blocks.size(), false);
  for (auto *block : blocks) {
    if (visited[block->id] == false) {
      if (incoming_nodes[block->id] == false) {
        visited[block->id] = true;
        _block_parent[block->id] = block->id;
        unite_blocks(block, visited, block->id);
      }
    }
  }
}


void CFGParser::unite_blocks(const Block *block, bool *visited, size_t parent) {
  for (auto *target : block->targets) {
    if (visited[target->block->id] == false) {
      visited[target->block->id] = true;
      _block_parent[target->block->id] = parent;
      unite_blocks(target->block, visited, parent);
    }
  }
}


static bool compare_block_ptr(Block *l, Block *r) {
  return *l < *r;
}


static bool compare_target_ptr(Target *l, Target *r) {
  return *l < *r;
}


void CFGParser::parse(const Graph &graph, std::vector<Function *> &functions) {
  std::unordered_map<size_t, Block *> block_id_map;
  std::unordered_map<std::string, Block *> block_name_map;
  std::vector<Block *> blocks;

  // Parse every vertex to build blocks
  for (auto *vertex : graph.vertices) {
    Block *block = new Block(vertex->id, vertex->name);

    std::deque<std::string> inst_strings;
    parse_inst_strings(vertex->label, inst_strings);
    for (auto &inst_string : inst_strings) {
      block->insts.push_back(new Inst(inst_string));
    }

    blocks.push_back(block);
    block_id_map[block->id] = block;
    block_name_map[block->name] = block;
  }

  // Find target block labels by checking every instruction
  for (auto *block : blocks) {
    for (auto *inst : block->insts) {
      if (inst->target.size() != 0) {
        auto *target_block = block_name_map[inst->target];
        TargetType type = get_target_type(inst);
        auto *target = new Target(inst, target_block, type);
        block->targets.push_back(target);
      }
    }
  }

  // Link fallthrough edges at the end of blocks
  if (DEBUG_CUDA_CFGPARSER) {
    std::cout << "Before linking" << std::endl;
    debug_blocks(blocks);
  }

  link_fallthrough_edges(graph, blocks, block_id_map);

  if (DEBUG_CUDA_CFGPARSER) {
    std::cout << "Before split" << std::endl;
    debug_blocks(blocks);
  }

  // Split blocks for loop analysis ans CALL instructions
  // TODO(keren): identify RET edges?
  split_blocks(blocks, block_id_map);

  if (DEBUG_CUDA_CFGPARSER) {
    std::cout << "After split" << std::endl;
    debug_blocks(blocks);
  }

  // Find toppest block
  _block_parent.clear();
  _block_parent.resize(blocks.size());
  for (size_t i = 0; i < _block_parent.size(); ++i) {
    _block_parent[i] = i;
  }
  find_block_parent(blocks);

  // Build functions
  // Find dangling blocks
  std::set<Block *> dangling_blocks;
  for (auto *block : blocks) {
    dangling_blocks.insert(block);
  }

  size_t function_id = 0;
  for (auto *block : blocks) {
    // Sort block targets according to inst offset
    std::sort(block->targets.begin(), block->targets.end(), compare_target_ptr);
    if (_block_parent[block->id] == block->id) {
      // Filter out self contained useless loops. A normal function will not start with "."
      if (block_id_map[block->id]->name[0] == '.') {
        continue;
      }
      Function *function = new Function(function_id, block_id_map[block->id]->name);
      ++function_id;
      for (auto *bb : blocks) {
        if (_block_parent[bb->id] == block->id) {
          if (DEBUG_CUDA_CFGPARSER) {
            std::cout << "Link " << bb->name << " with " << block_id_map[_block_parent[block->id]]->name << std::endl;
          }
          function->blocks.push_back(bb);

          auto iter = dangling_blocks.find(bb);
          if (iter != dangling_blocks.end()) {
            dangling_blocks.erase(iter);
          }
        }
      }
      std::sort(function->blocks.begin(), function->blocks.end(), compare_block_ptr);
      // For sm_30 <= cuda arch < sm_70, the first 8 byte of outter functions are control codes.
      // For sm_70, byte 0-16 will be the first instruction
      // Inner (local) functions will not start with a control code
      // So when we find a block starts with 8, just enforce the begin offset to be 0
      int first_inst_offset = function->blocks[0]->insts[0]->offset;
      function->blocks[0]->begin_offset = first_inst_offset == 8 ? 0 : first_inst_offset;
      functions.push_back(function);
    }
  }

  link_dangling_blocks(dangling_blocks, functions);
  
  // Sort blocks after linking dangling blocks
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      std::sort(block->targets.begin(), block->targets.end(), compare_target_ptr);
    }
    std::sort(function->blocks.begin(), function->blocks.end(), compare_block_ptr);
  }

  parse_calls(functions);
}


void CFGParser::link_fallthrough_edges(
  const Graph &graph,
  const std::vector<Block *> &blocks,
  std::unordered_map<size_t, Block *> &block_id_map) {
  std::map<std::pair<size_t, size_t>, size_t> edges;
  for (auto *edge : graph.edges) {
    edges[std::pair<size_t, size_t>(edge->source_id, edge->target_id)]++;
  }

  for (auto *block : blocks) {
    for (auto *target : block->targets) {
      auto source_id = block->id;
      auto target_id = target->block->id;
      edges[std::pair<size_t, size_t>(source_id, target_id)]--;
    }
  }

  for (auto edge : edges) {
    if (edge.second != 0) {
      auto source_id = edge.first.first;
      auto target_id = edge.first.second;
      auto *block = block_id_map[source_id];
      auto *target_block = block_id_map[target_id];
      auto *inst = block->insts.back();
      auto type = get_fallthrough_type(inst);
      auto *target = new Target(inst, target_block, type);
      block->targets.push_back(target);
    }
  }
}


void CFGParser::split_blocks(
  std::vector<Block *> &blocks,
  std::unordered_map<size_t, Block *> &block_id_map) {
  size_t extra_block_id = blocks.size();
  std::vector<Block *> candidate_blocks;
  for (auto *block : blocks) {
    std::vector<size_t> split_inst_index;
    // Step 1: filter out all branch instructions
    for (size_t i = 0; i < block->insts.size(); ++i) {
      Inst *inst = block->insts[i];
      if (inst->is_call) {
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

    if (DEBUG_CUDA_CFGPARSER) {
      std::cout << "Split index:" << std::endl;
      for (size_t i = 0; i < split_inst_index.size(); ++i) {
        std::cout << "Block: " << block->name <<
          ", offset: " << block->insts[split_inst_index[i]]->offset <<
          " " << block->insts[split_inst_index[i]]->opcode << std::endl;
      }
    }

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
      if (DEBUG_CUDA_CFGPARSER) {
        std::cout << "Intervals:" << std::endl;
        for (size_t i = 0; i < intervals.size(); ++i) {
          std::cout << "[" << intervals[i].first << ", " << intervals[i].second << "]" << std::endl;
        }
      }
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
          block_id_map[new_block->id] = new_block;
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
          // A conditional call is treated as a branch split, not call split
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
            TargetType next_block_type = TargetType::CALL_FT;
            Block *next_block = new_blocks[i + 1];
            new_block->targets.push_back(new Target(
              new_block->insts.back(), next_block, next_block_type));
          } else {
            Block *next_block = new_blocks[i + 1];
            TargetType next_block_type = get_fallthrough_type(target->inst);
            TargetType target_block_type = get_target_type(target->inst);

            new_block->targets.push_back(new Target(
              target->inst, next_block, next_block_type));
            new_block->targets.push_back(new Target(
              target->inst, target->block, target_block_type));
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
