// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *


//***************************************************************************

#ifndef GPUCFG_hpp
#define GPUCFG_hpp

//***************************************************************************
// system includes
//***************************************************************************

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>



//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <CFG.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************




//***************************************************************************
// begin namespace
//***************************************************************************

namespace GPUParse {



//***************************************************************************
// type declarations
//***************************************************************************

typedef Dyninst::Architecture Arch;
typedef Dyninst::Address Address;


struct InstructionStat {
  enum PredicateFlag {
    PREDICATE_NONE = 0,
    PREDICATE_TRUE = 1,
    PREDICATE_FALSE = 2,
  };

  static int const WAIT_BITS = 6;
  static int const BARRIER_NONE = 7;

  struct Control {
    uint8_t reuse;
    uint8_t wait;
    uint8_t read;
    uint8_t write;
    uint8_t yield;
    uint8_t stall;

    Control() : reuse(0), wait(0), read(7), write(7), yield(0), stall(1) {}
  };

  std::string op;
  int pc;
  int predicate;  // P0-P6
  int barrier_threshold = -1;
  bool indirect = false; // indirect memory addressing
  PredicateFlag predicate_flag;
  std::vector<int> predicate_assign_pcs;
  std::vector<int> dsts;    // R0-R255: only records normal registers
  std::vector<int> srcs;    // R0-R255: only records normal registers
  std::vector<int> pdsts;   // P0-P6: only records predicate registers
  std::vector<int> psrcs;   // P0-P6: only records predicate registers
  std::vector<int> bdsts;   // B1-B6: only records barriers
  std::vector<int> bsrcs;   // B1-B6: only records barriers
  std::vector<int> udsts;   // UR0-UR63: only records uniform registers
  std::vector<int> usrcs;   // UR0-UR63: only records uniform registers
  std::vector<int> updsts;  // UP0-UP?: only records uniform predicate registers
  std::vector<int> upsrcs;  // UP0-UP?: only records uniform predicate registers
  std::map<int, std::vector<int> > assign_pcs;
  std::map<int, std::vector<int> > passign_pcs;
  std::map<int, std::vector<int> > bassign_pcs;
  std::map<int, std::vector<int> > uassign_pcs;
  std::map<int, std::vector<int> > upassign_pcs;
  Control control;

  InstructionStat() {}

  //explicit InstructionStat(Instruction *inst);

  InstructionStat(const std::string &op, int pc, int predicate, PredicateFlag predicate_flag,
      std::vector<int> &predicate_assign_pcs, std::vector<int> &dsts,
      std::vector<int> &srcs, std::vector<int> &pdsts, std::vector<int> &psrcs,
      std::vector<int> &bdsts, std::vector<int> &bsrcs)
    : op(op),
    pc(pc),
    predicate(predicate),
    predicate_flag(predicate_flag),
    predicate_assign_pcs(predicate_assign_pcs),
    dsts(dsts),
    srcs(srcs),
    pdsts(pdsts),
    psrcs(psrcs),
    bdsts(bdsts),
    bsrcs(bsrcs) {}

  InstructionStat(const std::string &op, int pc, int predicate, PredicateFlag predicate_flag,
      std::vector<int> &predicate_assign_pcs, std::vector<int> &dsts,
      std::vector<int> &srcs, std::vector<int> &pdsts, std::vector<int> &psrcs,
      std::vector<int> &bdsts, std::vector<int> &bsrcs, std::vector<int> &udsts,
      std::vector<int> &usrcs, std::vector<int> &updsts, std::vector<int> &upsrcs,
      std::map<int, std::vector<int> > &assign_pcs,
      std::map<int, std::vector<int> > &passign_pcs,
      std::map<int, std::vector<int> > &bassign_pcs,
      std::map<int, std::vector<int> > &uassign_pcs,
      std::map<int, std::vector<int> > &upassign_pcs)
    : op(op),
    pc(pc),
    predicate(predicate),
    predicate_flag(predicate_flag),
    predicate_assign_pcs(predicate_assign_pcs),
    dsts(dsts),
    srcs(srcs),
    pdsts(pdsts),
    psrcs(psrcs),
    bdsts(bdsts),
    bsrcs(bsrcs),
    udsts(udsts),
    usrcs(usrcs),
    updsts(updsts),
    upsrcs(upsrcs),
    assign_pcs(assign_pcs),
    passign_pcs(passign_pcs),
    bassign_pcs(bassign_pcs),
    uassign_pcs(uassign_pcs),
    upassign_pcs(upassign_pcs) {}

  InstructionStat(const std::string &op, int pc, int predicate, int barrier_threshold,
      bool indirect, PredicateFlag predicate_flag,
      std::vector<int> &predicate_assign_pcs, std::vector<int> &dsts,
      std::vector<int> &srcs, std::vector<int> &pdsts, std::vector<int> &psrcs,
      std::vector<int> &bdsts, std::vector<int> &bsrcs, std::vector<int> &udsts,
      std::vector<int> &usrcs, std::vector<int> &updsts, std::vector<int> &upsrcs,
      std::map<int, std::vector<int> > &assign_pcs,
      std::map<int, std::vector<int> > &passign_pcs,
      std::map<int, std::vector<int> > &bassign_pcs,
      std::map<int, std::vector<int> > &uassign_pcs,
      std::map<int, std::vector<int> > &upassign_pcs, Control &control)
    : op(op),
    pc(pc),
    predicate(predicate),
    barrier_threshold(barrier_threshold),
    indirect(indirect),
    predicate_flag(predicate_flag),
    predicate_assign_pcs(predicate_assign_pcs),
    dsts(dsts),
    srcs(srcs),
    pdsts(pdsts),
    psrcs(psrcs),
    bdsts(bdsts),
    bsrcs(bsrcs),
    udsts(udsts),
    usrcs(usrcs),
    updsts(updsts),
    upsrcs(upsrcs),
    assign_pcs(assign_pcs),
    passign_pcs(passign_pcs),
    bassign_pcs(bassign_pcs),
    uassign_pcs(uassign_pcs),
    upassign_pcs(upassign_pcs),
    control(control) {}

  InstructionStat(const std::string &op, int pc, PredicateFlag predicate_flag,
                  std::vector<int> dsts, std::vector<int> srcs)
    : op(op),
    pc(pc),
    predicate_flag(predicate_flag),
    dsts(dsts),
    srcs(srcs) {}


  bool operator<(const InstructionStat &other) const { return this->pc < other.pc; }

  bool find_src_reg(int reg) { return std::find(srcs.begin(), srcs.end(), reg) != srcs.end(); }

  bool find_src_ureg(int ureg) {
    return std::find(usrcs.begin(), usrcs.end(), ureg) != usrcs.end();
  }

  bool find_src_pred_reg(int pred_reg) {
    return std::find(psrcs.begin(), psrcs.end(), pred_reg) != psrcs.end();
  }

  bool find_src_barrier(int barrier) {
    return std::find(bsrcs.begin(), bsrcs.end(), barrier) != bsrcs.end();
  }
};


struct Inst {
  Address offset;
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
  InstructionStat *inst_stat;

  Inst(Address offset, int size, Arch arch, InstructionStat *inst_stat) : offset(offset), size(size), dual_first(false), dual_second(false),
    is_call(false), is_jump(false), is_sync(false), arch(arch), inst_stat(inst_stat) { InstIntercept(); }

  // Constructor for dummy inst
  Inst(Address offset, int size, Arch arch) : offset(offset), size(size), dual_first(false), dual_second(false),
    is_call(false), is_jump(false), is_sync(false), arch(arch) { InstIntercept(); }

  Inst(Address offset, int size) : Inst(offset, size, Dyninst::Arch_none) { InstIntercept(); }

  explicit Inst(Address offset) : Inst(offset, 0) { InstIntercept(); }

  void InstIntercept() { }
};


#ifdef DYNINST_SUPPORTS_INTEL_GPU

struct IntelInst : public Inst {
  // Constructor for dummy inst
  IntelInst(Address offset, int size) : Inst(offset, size, Dyninst::Arch_intelGen9) {}

  IntelInst(Address offset, int size, InstructionStat *inst_stat) : Inst(offset, size, Dyninst::Arch_intelGen9, inst_stat) {}

  explicit IntelInst(Address offset) : Inst(offset, 0, Dyninst::Arch_intelGen9) {}
};

#endif


struct CudaInst : public Inst {
  // Constructor for dummy inst
  CudaInst(Address offset, int size) : Inst(offset, size, Dyninst::Arch_cuda) {}

  explicit CudaInst(Address offset) : Inst(offset, 0, Dyninst::Arch_cuda) {}

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
              // Extract target name of the jump or the sync instruction
              // The target name can be in two forms:
              // "**.L_<number>**" for CUDA < 11.5
              // "**.L_x_<number>**" for CUDA >= 11.5
              // We assuming the target label will keep the form "**.L_**<number>**" in the further release
              static const std::string TARGET_LABEL = ".L_";
              operands.push_back(s);
              if (is_jump || is_sync) {
                auto pos = s.find(TARGET_LABEL);
                if (pos != std::string::npos) {
                  auto digit_pos = pos + TARGET_LABEL.size();
                  // Find the start digit of the number
                  for (;digit_pos != std::string::npos && !std::isdigit(s[digit_pos]); ++digit_pos);
                  // Find the end digit of the number
                  for (;digit_pos != std::string::npos && std::isdigit(s[digit_pos]); ++digit_pos);
                  if (digit_pos != std::string::npos) {
                    this->target = s.substr(pos, digit_pos - pos);
                  }
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
  Address address;
  int begin_offset;
  std::vector<Inst *> insts;
  std::vector<Target *> targets;
  std::string name;

  Block(size_t _id, Address _address, const std::string &_name) : id(_id), address(_address), begin_offset(0), name(_name) { BlockIntercept(); }

  Block(size_t _id, const std::string &_name) : Block(_id, 0, _name) { BlockIntercept(); }


  bool operator<(const Block &other) const {
    if (this->insts.size() == 0) {
      return true;
    } else if (other.insts.size() == 0) {
      return false;
    } else {
      return this->insts[0]->offset < other.insts[0]->offset;
    }
  }

  Address startAddress() {
    auto firstInstruction = insts.front();
    return firstInstruction->offset;
  }

  Address endAddress() {
    auto lastInstruction = insts.back();
    return lastInstruction->offset + lastInstruction->size;
  }

  ~Block() {
    for (auto *target: targets) {
      delete target;
    }
  }

  void BlockIntercept() {
  }
};


struct Function {
  std::vector<Block *> blocks;
  size_t id;
  std::string name;
  Address address;

  Function(size_t _id, const std::string &_name, Address _address) : id(_id), name(_name),
    address(_address) {}

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


class GPUInstDumper {
public:
  GPUInstDumper(GPUParse::Function &_function) : function(_function) {};
  virtual void dump(GPUParse::Inst* inst);
protected:
  GPUParse::Function &function;
};

void
dumpFunction
(
  GPUParse::Function &function,
  GPUInstDumper &idump
);



//***************************************************************************
// end namespace
//***************************************************************************

}

#endif
