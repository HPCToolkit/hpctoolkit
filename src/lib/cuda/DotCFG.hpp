#ifndef _CUDA_DOT_CFG_H_
#define _CUDA_DOT_CFG_H_

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "Instruction.hpp"

// dyninst
#include <CFG.h>

namespace CudaParse {

struct Block;

// TODO(Keren): consistent with dyninst
typedef Dyninst::ParseAPI::EdgeTypeEnum TargetType;

struct Target {
  Instruction *inst;
  Block *block;
  TargetType type; 

  Target(Instruction *inst, Block *block, TargetType type) : inst(inst), block(block), type(type) {}

  bool operator<(const Target &other) const {
    return this->inst->offset < other.inst->offset;
  }
};


struct Block {
  int begin_offset;
  int address;
  std::vector<Instruction *> insts;
  std::vector<Target *> targets;
  size_t id;
  std::string name;

  Block(size_t id) : id(id) {}

  Block(size_t id, const std::string &name) : id(id), name(name) {}

  bool operator<(const Block &other) const {
    if (this->insts.size() == 0) {
      return true;
    } else if (other.insts.size() == 0) {
      return false;
    } else {
      return this->insts[0]->offset < other.insts[0]->offset;
    }
  }

  ~Block() {
    for (auto *inst : insts) {
      delete inst;
    }
    for (auto *target: targets) {
      delete target;
    }
  }
};


struct Function {
  std::vector<Block *> blocks;
  size_t id;
  // symbol index in cubins
  size_t index;
  std::string name;
  int address;
  bool unparsable;
  bool global;

  Function(size_t id, size_t index, const std::string &name, int address, bool unparsable, bool global) :
    id(id), index(index), name(name), address(address), unparsable(unparsable), global(global) {}

  Function(size_t id, size_t index, const std::string &name, int address) :
    Function(id, index, name, address, false, false) {}

  Function(size_t id, const std::string &name, int address) : Function(id, 0, name, address) {}

  Function(size_t id, const std::string &name) : Function(id, 0, name, 0) {}

  ~Function() {
    for (auto *block : blocks) {
      delete block;
    }
  }
};


struct LoopEntry {
  Block *entry_block; 
  Block *back_edge_block;
  Instruction *back_edge_inst;

  LoopEntry(Block *entry_block) : entry_block(entry_block) {}

  LoopEntry(Block *entry_block, Block *back_edge_block, Instruction *back_edge_inst) :
    entry_block(entry_block), back_edge_block(back_edge_block), back_edge_inst(back_edge_inst) {}
};


struct Loop {
  std::vector<LoopEntry *> entries;
  std::unordered_set<Loop *> child_loops;
  std::unordered_set<Block *> blocks;
  std::unordered_set<Block *> child_blocks;
  Function *function;

  Loop(Function *function) : function(function) {}
};


struct Call {
  Instruction *inst;
  Block *block; 
  Function *caller_function;
  Function *callee_function;

  Call(Instruction *inst, Block *block, Function *caller_function, Function *callee_function) :
    inst(inst), block(block), caller_function(caller_function), callee_function(callee_function) {}
};

}

#endif
