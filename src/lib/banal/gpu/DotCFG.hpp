#ifndef BANAL_GPU_DOT_CFG_H
#define BANAL_GPU_DOT_CFG_H

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
// dyninst
#include <CFG.h>

#include <include/hpctoolkit-config.h>

namespace GPUParse {

typedef Dyninst::Architecture Arch;

struct Inst {
  int offset;
  int size;
  bool dual_first;
  bool dual_second;
  bool is_call;
  bool is_jump;
  bool is_sync;
  std::string predicate;
  std::string opcode;
  std::string port;
  std::string target;
  std::vector<std::string> operands;
  Arch arch;

  // Constructor for dummy inst
  Inst(int offset, int size, Arch arch) : offset(offset), size(size), dual_first(false), dual_second(false),
    is_call(false), is_jump(false), is_sync(false), arch(arch) {}

  Inst(int offset, int size) : Inst(offset, size, Dyninst::Arch_none) {}

  explicit Inst(int offset) : Inst(offset, 0) {}
};


#ifdef DYNINST_SUPPORTS_INTEL_GPU

struct IntelInst : public Inst {
  // Constructor for dummy inst
  IntelInst(int offset, int size) : Inst(offset, size, Dyninst::Arch_intelGen9) {}

  explicit IntelInst(int offset) : Inst(offset, 0, Dyninst::Arch_intelGen9) {}
};

#endif


struct CudaInst : public Inst {
  // Constructor for dummy inst
  CudaInst(int offset, int size) : Inst(offset, size, Dyninst::Arch_cuda) {}

  explicit CudaInst(int offset) : Inst(offset, 0, Dyninst::Arch_cuda) {}

  // Cuda instruction constructor
  CudaInst(std::string &inst_str) : CudaInst(0, 0) {
    if (inst_str.find("{") != std::string::npos) {  // Dual first
      auto pos = inst_str.find("{");
      inst_str.replace(pos, 1, " ");
      dual_first = true;
    }
    if (inst_str.find("}") != std::string::npos) {  // Dual second
      inst_str = inst_str.substr(2);
      auto pos = inst_str.find("*/");
      if (pos != std::string::npos) {  
        inst_str.replace(pos, 2, ":");
        dual_second = true;
      }
    }
    std::istringstream *iss = new std::istringstream(inst_str);
    std::string s;
    if (std::getline(*iss, s, ':')) {
      if (s.find("<") != std::string::npos) {
        // Port notation in dot graph to link basic blocks
        auto pos = s.find(">");
        this->port = s.substr(1, pos - 1);
        s = s.substr(pos + 1); 
      }
      std::stringstream ss;
      ss << std::hex << s;
      ss >> offset;
      if (std::getline(*iss, s, ':')) {
        s.erase(std::remove(s.begin(), s.end(), '{'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '}'), s.end());
        s.erase(std::remove(s.begin(), s.end(), ';'), s.end());
        s.erase(std::remove(s.begin(), s.end(), ','), s.end());
        s.erase(std::remove(s.begin(), s.end(), '('), s.end());
        s.erase(std::remove(s.begin(), s.end(), ')'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '`'), s.end());
        std::regex e("\\\\ ");
        std::string newline("\n");
        delete iss;
        iss = new std::istringstream(std::regex_replace(s, e, newline));
        while (std::getline(*iss, s)) {
          if (s != "") {
            if (opcode == "") {
              if (s.find("@") != std::string::npos) {
                predicate = s;
              } else {
                opcode = s;
                if (opcode.find("CALL") != std::string::npos || // sm_70
                  opcode.find("CAL") != std::string::npos) { // sm_60
                  this->is_call = true;
                } else if (opcode.find("BRA") != std::string::npos ||
                  opcode.find("BRX") != std::string::npos ||
                  opcode.find("JMP") != std::string::npos ||
                  opcode.find("JMX") != std::string::npos ||
                  opcode.find("BREAK") != std::string::npos ||
                  opcode.find("JMXU") != std::string::npos) {
                  this->is_jump = true;
                } else if (opcode.find("SYNC") != std::string::npos) {
                  // avoid Barrier Set Convergence Synchronization Point
                  //opcode.find("SSY") != std::string::npos ||
                  //opcode.find("BSSY") != std::string::npos)
                  // TODO(Keren): add more sync instructions
                  this->is_sync = true;
                }
              }
            } else {
              // Target
              operands.push_back(s);
              if (is_jump || is_sync) {
                auto pos = s.find(".L_");
                if (pos != std::string::npos) {
                  auto end_pos = pos + 3;
                  size_t len = 0;
                  while (end_pos != std::string::npos) {
                    if (!std::isdigit(s[end_pos])) {
                      break;
                    }
                    ++len;
                    ++end_pos;
                  }
                  this->target = s.substr(pos, len + 3);
                }
              }
            }
          }
        }
      }
    }
    delete iss;
  }
};


struct Block;

typedef Dyninst::ParseAPI::EdgeTypeEnum TargetType;

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
  size_t id;
  int address;
  int begin_offset;
  std::vector<Inst *> insts;
  std::vector<Target *> targets;
  std::string name;

  Block(size_t id, int address, const std::string &name) : id(id), address(address), name(name) {}

  Block(size_t id, const std::string &name) : Block(id, 0, name) {}

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
  int address;

  Function(size_t id, const std::string &name) : id(id), name(name),
    address(0) {}

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
