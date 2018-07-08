#ifndef _DOT_CFG_H_
#define _DOT_CFG_H_

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace CudaParse {

struct Inst {
  int offset;
  bool dual;
  std::string predicate;
  std::string opcode;
  std::string port;
  std::vector<std::string> operands;

  Inst(std::string &inst_str) : offset(0), dual(false) {
    if (inst_str.find("{") != std::string::npos) {  // Dual first
      auto pos = inst_str.find("{");
      inst_str.replace(pos, 1, " ");
      dual = true;
    }
    if (inst_str.find("}") != std::string::npos) {  // Dual second
      inst_str = inst_str.substr(2);
      auto pos = inst_str.find("*/");
      if (pos != std::string::npos) {  
        inst_str.replace(pos, 2, ":");
        dual = true;
      }
    }
    std::istringstream iss(inst_str);
    std::string s;
    if (std::getline(iss, s, ':')) {
      if (s.find("<") != std::string::npos) {
        auto pos = s.find(">");
        this->port = s.substr(1, pos - 1);
        s = s.substr(pos + 1); 
      }
      std::stringstream ss;
      ss << std::hex << s;
      ss >> offset;
      if (std::getline(iss, s, ':')) {
        s.erase(std::remove(s.begin(), s.end(), '{'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '}'), s.end());
        s.erase(std::remove(s.begin(), s.end(), ';'), s.end());
        s.erase(std::remove(s.begin(), s.end(), ','), s.end());
        s.erase(std::remove(s.begin(), s.end(), '('), s.end());
        s.erase(std::remove(s.begin(), s.end(), ')'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '`'), s.end());
        std::regex e("\\\\ ");
        iss = std::istringstream(std::regex_replace(s, e, "\n"));
        while (std::getline(iss, s)) {
          if (s != "") {
            if (opcode == "") {
              if (s.find("@") != std::string::npos) {
                predicate = s;
              } else {
                opcode = s;
              }
            } else {
              operands.push_back(s);
            }
          }
        }
      }
    }
  }
};


struct Block;

enum TargetType {
  CALL = 0,
  COND_TAKEN,
  COND_NOT_TAKEN,
  FALLTHROUGH,
  DIRECT
};

struct Target {
  Inst *inst;
  Block *block;
  TargetType type; 

  Target(Inst *inst, Block *block, TargetType type) : inst(inst), block(block), type(type) {}

  bool operator<(const Target &other) const {
    return this->inst->offset < other.inst->offset;
  }
};


struct Block {
  std::vector<Inst *> insts;
  std::vector<Target *> targets;
  size_t id;
  std::string name;

  Block(size_t id, std::string &name) : id(id), name(name) {}

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
  std::string name;

  Function(size_t id, const std::string &name) : id(id), name(name) {}

  ~Function() {
    for (auto *block : blocks) {
      delete block;
    }
  }
};


struct LoopEntry {
  Block *entry_block; 
  Block *back_edge_block;
  Inst *back_edge_inst;

  LoopEntry(Block *entry_block) : entry_block(entry_block) {}

  LoopEntry(Block *entry_block, Block *back_edge_block, Inst *back_edge_inst) :
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
  Inst *inst;
  Block *block; 
  Function *caller_function;
  Function *callee_function;

  Call(Inst *inst, Block *block, Function *caller_function, Function *callee_function) :
    inst(inst), block(block), caller_function(caller_function), callee_function(callee_function) {}
};

}

#endif
