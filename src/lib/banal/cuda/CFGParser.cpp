#include "CFGParser.hpp"
#include <cctype>
#include <iostream>

namespace CudaParse {

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
        if (inst->opcode.find("CALL") != std::string::npos || // sm_70
          inst->opcode.find("CAL") != std::string::npos) { // sm_60
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
  size_t graph_size = _block_parent.size();
  if (parent == graph_size) {
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
  size_t graph_size = graph.vertices.size();
  _block_parent.resize(graph_size);
  for (size_t i = 0; i < graph_size; ++i) {
    _block_parent[i] = graph_size;
  }

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
    // Find toppest block
    unite_blocks(edge->target_id, edge->source_id);
    Block *target_block = block_map[edge->target_id];
    Block *source_block = block_map[edge->source_id];
    
    TargetType type = DIRECT;
    // Link blocks
    Inst *target_inst;
    for (auto *inst : source_block->insts) {
      if (inst->port == edge->source_port[0]) {
        if (inst->predicate.find("!@") != std::string::npos) {
          type = COND_TAKEN;
        } else if (inst->predicate.find("@") != std::string::npos) {
          type = COND_NOT_TAKEN;
        } else if (inst == source_block->insts.back()) {
          type = FALLTHROUGH;
        }
        source_block->targets.push_back(new Target(inst, target_block, type));
      }
    }
    // Some edge may not have port information
    if (source_block->targets.size() == 0) {
      Inst *inst = source_block->insts.back();
      type = FALLTHROUGH;
      source_block->targets.push_back(new Target(inst, target_block, type));
    }
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
      functions.push_back(function);
    }
  }

  parse_calls(functions);
}

}
