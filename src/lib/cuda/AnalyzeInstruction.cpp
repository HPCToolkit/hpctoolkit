#include "AnalyzeInstruction.hpp"

#include <Instruction.h>
#include <AbslocInterface.h>
#include <slicing.h>
#include <Graph.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

#include "DotCFG.hpp"
#include "Instruction.hpp"

#define INSTRUCTION_ANALYZER_DEBUG 0

#define FAST_SLICING

namespace CudaParse {

template <InstructionType inst_type>
void analyze_instruction(Instruction &inst, std::string &op);

template <>
void analyze_instruction<INS_TYPE_MEMORY>(Instruction &inst, std::string &op) {
  op = "MEMORY";

  std::string width;
  std::string ldst;
  std::string scope;

  const std::string &opcode = inst.opcode;

  if (opcode.find("LD") != std::string::npos) {
    ldst = ".LOAD";
    if (opcode == "LDS") {
      scope = ".SHARED";
    } else if (opcode == "LDL") {
      scope = ".LOCAL";
    } else if (opcode == "LDG") {
      scope = ".GLOBAL";
    } else if (opcode == "LDC") {
      scope = ".CONSTANT";
    }
  } else if (opcode.find("ST") != std::string::npos) {
    ldst = ".STORE";
    if (opcode == "STS") {
      scope = ".SHARED";
    } else if (opcode == "STL") {
      scope = ".LOCAL";
    } else if (opcode == "STG") {
      scope = ".GLOBAL";
    }
  } else if (opcode == "RED" || opcode.find("ATOM") != std::string::npos) {
    ldst = ".ATOMIC";
    if (opcode == "ATOMS") {
      scope = ".SHARED";
    } else if (opcode == "ATOMG") {
      scope = ".GLOBAL";
    }
  } else {
    ldst = ".OTHER";
  }

  width = ".32";
  for (auto &modifier : inst.modifiers) {
    if (modifier == "8" || modifier == "U8" || modifier == "S8") {
      width = ".8";
    } else if (modifier == "16" || modifier == "U16" || modifier == "S16") {
      width = ".16";
    } else if (modifier == "32" || modifier == "U32" || modifier == "S32") {
      width = ".32";
    } else if (modifier == "64" || modifier == "U64" || modifier == "S64") {
      width = ".64";
    } else if (modifier == "128" || modifier == "U128" || modifier == "S128") {
      width = ".128";
    }
  }

  op += ldst + scope + width;
}


template <>
void analyze_instruction<INS_TYPE_FLOAT>(Instruction &inst, std::string &op) {
  op = "FLOAT";

  std::string type;
  std::string width;

  const std::string &opcode = inst.opcode;

  width = ".32";
  if (opcode[0] == 'D') {
    width = ".64";
  } else if (opcode[0] == 'H') {
    width = ".16";
  }
  
  if (opcode == "MUFU") {
    type = ".MUFU";
  } else if (opcode.find("ADD") != std::string::npos) {
    type = ".ADD";
  } else if (opcode.find("MUL") != std::string::npos) {
    type = ".MUL";
  } else if (opcode.find("FMA") != std::string::npos) {
    type = ".MAD";
  } else if (opcode.find("MMA") != std::string::npos) {
    type = ".TENSOR";
  } else {
    type = ".OTHER";
  }

  op += type + width;
}


template <>
void analyze_instruction<INS_TYPE_INTEGER>(Instruction &inst, std::string &op) {
  op = "INTEGER";

  std::string type;

  const std::string &opcode = inst.opcode;

  if (opcode.find("MAD") != std::string::npos) {
    type = ".MAD";
  } else if (opcode.find("DP") != std::string::npos) {
    type = ".DOT";
  } else if (opcode.find("MMA") != std::string::npos) {
    type = ".TENSOR";
  } else if (opcode.find("MUL") != std::string::npos) {
    type = ".MUL";
  } else if (opcode.find("ADD") != std::string::npos) {
    if (opcode == "IADD3") {
      type = ".ADD3";
    } else {
      type = ".ADD";
    }
  } else {
    type = ".OTHER";
  }

  auto width = ".32";

  for (auto &modifier : inst.modifiers) {
    if (modifier == "WIDE") {
      // IADD only has 32 bit operands, it simulates 64 bit calculation by two IADD instructions
      // IMAD could have 64 bit operands with a 'wide' modifier
      width = ".64";
    } else if (modifier == "MOV") {
      // IMAD.MOV are used for MOVE either float or integer, probbably for higher throughput?
      type += ".MOVE";
    }
  }

  op += type + width;
}


template <>
void analyze_instruction<INS_TYPE_TEXTRUE>(Instruction &inst, std::string &op) {
  op = "MEMORY.TEXTURE";
}


template <>
void analyze_instruction<INS_TYPE_PREDICATE>(Instruction &inst, std::string &op) {
  op = "PREDICATE";
}


template <>
void analyze_instruction<INS_TYPE_CONTROL>(Instruction &inst, std::string &op) {
  op = "CONTROL";

  std::string type;

  const std::string &opcode = inst.opcode;

  if (opcode.find("MEMBAR") != std::string::npos ||
    opcode.find("DEPBAR") != std::string::npos ||
    opcode.find("ERRBAR") != std::string::npos) {
    type = ".BAR";
  } else if (opcode.find("BAR") != std::string::npos) {
    type = ".SYNC";
  } else if (opcode.find("SYNC") != std::string::npos) {
    type = ".CONVERGE";
  } else if (opcode.find("CAL") != std::string::npos) {
    type = ".CALL";
  } else if (opcode.find("EXIT") != std::string::npos) {
    type = ".EXIT";
  } else if (opcode.find("RET") != std::string::npos) {
    type = ".RET";
  } else if (opcode.find("JM") != std::string::npos) {
    type = ".JMP";
  } else if (opcode.find("BR") != std::string::npos) {
    type = ".BRANCH";
  } else {
    type = ".OTHER";
  }

  op += type;
}


template <>
void analyze_instruction<INS_TYPE_MISC>(Instruction &inst, std::string &op) {
  op = "MISC";

  std::string type;

  const std::string &opcode = inst.opcode;

  if (opcode.find("I2") != std::string::npos ||
    opcode.find("F2") != std::string::npos || opcode == "FRND") {
    type = ".CONVERT";
    if (opcode.find("I2F") != std::string::npos || opcode.find("FRND") != std::string::npos) {
      type += ".I2F";
    } else if (opcode.find("F2I") != std::string::npos) {
      type += ".F2I";
    } else if (opcode.find("I2I") != std::string::npos) {
      type += ".I2I";
    } else if (opcode.find("F2F") != std::string::npos) {
      type += ".F2F";
      // TODO(Keren): other types
      if (opcode.find("F64.F32") != std::string::npos) {
        type += "._64_TO_32";
      } else if (opcode.find("F32.F64") != std::string::npos) {
        type += "._32_TO_64";
      }
    }
  } else if (opcode.find("SHFL") != std::string::npos ||
    opcode.find("PRMT") != std::string::npos) {
    type = ".SHUFFLE";
  } else if (opcode.find("MOV") != std::string::npos) {
    type = ".MOVE";
    if (opcode.find("MOV32I") != std::string::npos) {
      type += ".I";
    }
  } else {
    type = ".OTHER";
  }

  op += type;
}


template <>
void analyze_instruction<INS_TYPE_UNIFORM>(Instruction &inst, std::string &op) {
  auto &opcode = inst.opcode;

  if (opcode.find("R2UR") != std::string::npos ||
    opcode.find("S2UR") != std::string::npos ||
    opcode.find("UR2UP") != std::string::npos ||
    opcode.find("UP2UR") != std::string::npos) {
    op = "MISC.MOVE";
  } else if (opcode.find("UCLEA") != std::string::npos) {
    op = "INTEGER";
  } else if (opcode.find("REDUX") != std::string::npos ||
    opcode.find("VOTEU") != std::string::npos) {
    op = "MISC";
  } else {
    // Remove U prefix
    opcode.erase(0, 1);
    if (Instruction::opcode_types.find(opcode) != Instruction::opcode_types.end()) {
      auto inst_type = Instruction::opcode_types[opcode];

#define INST_DISPATCHER(TYPE, VALUE) \
      case TYPE: \
        { \
          analyze_instruction<TYPE>(inst, op); \
          break; \
        }

      switch (inst_type) {
        FORALL_INS_TYPES(INST_DISPATCHER)
        default:
          break;
        }

#undef INST_DISPATCHER
    }
  }

  op += ".UNIFORM";
}


static int convert_reg(const std::string &str, size_t pos) {
  int num = 0;
  bool find_digit = false;
  while (pos != std::string::npos) {
    if (isdigit(str[pos])) {
      find_digit = true;
      num = num * 10 + str[pos] - '0'; 
    } else {
      break;
    }
    ++pos;
  }
  
  if (find_digit) {
    return num;
  }

  return -1;
}


InstructionStat::InstructionStat(Instruction *inst) {
  std::string op;

#define INST_DISPATCHER(TYPE, VALUE)                \
  case TYPE:                                        \
    {                                               \
      analyze_instruction<TYPE>(*inst, op); \
      break;                                        \
    }

  switch (inst->type) {
    FORALL_INS_TYPES(INST_DISPATCHER)
    default:
      break;
    }

#undef INST_DISPATCHER

  this->op = op;
  this->pc = inst->offset;
  // -1 means no value
  this->predicate = -1;
  this->predicate_flag = InstructionStat::PredicateFlag::PREDICATE_NONE;

  if (INSTRUCTION_ANALYZER_DEBUG) {
    std::cout << inst->offset << " " << op << " ";
  }

  if (inst->predicate.size() != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << inst->predicate << " ";
    }

    auto pos = inst->predicate.find("P");
    if (pos != std::string::npos) {
      int pred = convert_reg(inst->predicate, pos + 1);
      if (pred == -1) {
        // Use special registers in predicate as a placeholder
        // e.g., @PT, @!PT
        // Skip this instruction
        return;
      }
      this->predicate = pred;
      pos = inst->predicate.find("!");
      if (pos != std::string::npos) {
        this->predicate_flag = InstructionStat::PredicateFlag::PREDICATE_FALSE;
      } else {
        this->predicate_flag = InstructionStat::PredicateFlag::PREDICATE_TRUE;
      }
    }
  }

  if (inst->is_call || inst->is_jump) {
    // Do not parse operands of call and jump
    return;
  }

  if (inst->operands.size() != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << inst->operands[0] << " ";
    }

    // STORE [R1], R2
    // LOAD R1, [R2]
    // FADD R1, R2, R3
    // IMAD.WIDE R1, R2, R3
    auto pos = inst->operands[0].find("R");
    bool store = false;
    bool uniform = false;
    if (pos != std::string::npos) {
      if (inst->operands[0].find("\[") != std::string::npos) {  // store instruction
        store = true;
      }

      if (inst->operands[0].find("UR") != std::string::npos) {  // uniform register
        uniform = true;
      }

      auto &srcs = uniform ? this->usrcs : this->srcs;
      auto &dsts = uniform ? this->udsts : this->dsts;
      auto reg = convert_reg(inst->operands[0], pos + 1);
      if (reg == -1) {
        // Use special registers in destation as a placeholder
        // Skip this instruction
        return;
      }
      if (store) {
        if (this->op.find(".SHARED") != std::string::npos ||
          this->op.find(".LOCAL") != std::string::npos) {
          // memory 32-bit
          srcs.push_back(reg);
        } else {
          // memory 64-bit
          srcs.push_back(reg);
          srcs.push_back(reg + 1);
        }
      } else {
        // load or arithmetic
        if (this->op.find(".64") != std::string::npos ||
          this->op.find("._32_TO_64") != std::string::npos) {  // vec 64
          dsts.push_back(reg);
          dsts.push_back(reg + 1);
        } else if (this->op.find(".128") != std::string::npos) {  // vec 128
          dsts.push_back(reg);
          dsts.push_back(reg + 1);
          dsts.push_back(reg + 2);
          dsts.push_back(reg + 3);
        } else {  // vec 32, 16, 8
          dsts.push_back(reg);
        }
      }
    }

    // Predicate register
    pos = inst->operands[0].find("P");
    if (pos != std::string::npos) {
      auto reg = convert_reg(inst->operands[0], pos + 1);
      if (reg == -1) {
        // Use special registers in destation as a placeholder
        // Skip this instruction
        return;
      }
      if (inst->operands[0].find("UP")) {
        // uniform predicate register
        this->updsts.push_back(reg);
      } else {
        this->pdsts.push_back(reg);
      }
    }

    for (size_t i = 1; i < inst->operands.size(); ++i) {
      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << inst->operands[i] << " ";
      }

      pos = inst->operands[i].find("R");
      if (pos != std::string::npos) {
        bool uniform = false;
        if (inst->operands[i].find("UR") != std::string::npos) {  // uniform register
          uniform = true;
        }

        auto &srcs = uniform ? this->usrcs : this->srcs;
        auto reg = convert_reg(inst->operands[i], pos + 1);
        if (this->op.find(".LOAD") != std::string::npos) {
          // load
          if (this->op.find(".SHARED") != std::string::npos ||
            this->op.find(".LOCAL") != std::string::npos) {
            // memory 32-bit
            srcs.push_back(reg);
          } else {
            // memory 64-bit
            srcs.push_back(reg);
            srcs.push_back(reg + 1);
          }
        } else {
          if (this->op.find("INTEGER") != std::string::npos) {
            // integer source only have 32
            srcs.push_back(reg);
          } else {
            // arithmetic or store
            if (this->op.find(".64") != std::string::npos ||
              this->op.find("._64_TO_32") != std::string::npos) {  // vec 64
              if (reg == -1) {  // rz
                srcs.push_back(reg);
                srcs.push_back(reg);
              } else {
                srcs.push_back(reg);
                srcs.push_back(reg + 1);
              }
            } else if (this->op.find(".128") != std::string::npos) {  // vec 128
              if (reg == -1) {
                srcs.push_back(-1);
                srcs.push_back(-1);
                srcs.push_back(-1);
                srcs.push_back(-1);
              } else {
                srcs.push_back(reg);
                srcs.push_back(reg + 1);
                srcs.push_back(reg + 2);
                srcs.push_back(reg + 3);
              }
            } else {  // vec 32, 16, 8
              srcs.push_back(reg);
            }
          }
        }
      }

      pos = inst->operands[i].find("P");
      if (pos != std::string::npos) {
        auto reg = convert_reg(inst->operands[i], pos + 1);
        if (reg != -1) {
          // Preventing PT
          if (inst->operands[i].find("UP") != std::string::npos) {
            this->upsrcs.push_back(reg);
          } else {
            this->psrcs.push_back(reg);
          }
        }
      }
    }
  }

  if (INSTRUCTION_ANALYZER_DEBUG) {
    std::cout << std::endl;
  }
}


struct InstructionStatPointCompare {
  bool operator()(const InstructionStat *l, const InstructionStat *r) {
    return *l < *r;
  }
};


void flatCudaInstructionStats(const std::vector<Function *> &functions,
  std::vector<InstructionStat *> &inst_stats) {
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          // Calculate absolute address
          auto *inst_stat = inst->inst_stat;
          inst_stats.push_back(inst_stat);
        }
      }
    }
  }

  std::sort(inst_stats.begin(), inst_stats.end(), InstructionStatPointCompare());
}


void relocateCudaInstructionStats(std::vector<Function *> &functions) {
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          // Calculate absolute address
          auto *inst_stat = inst->inst_stat;
          inst_stat->pc += function->address;

          for (auto &iter : inst_stat->assign_pcs) {
            for (auto piter = iter.second.begin(); piter != iter.second.end(); ++piter) {
              *piter += function->address;
            }
          }
          for (auto &iter : inst_stat->passign_pcs) {
            for (auto piter = iter.second.begin(); piter != iter.second.end(); ++piter) {
              *piter += function->address;
            }
          }
          for (auto &iter : inst_stat->bassign_pcs) {
            for (auto biter = iter.second.begin(); biter != iter.second.end(); ++biter) {
              *biter += function->address;
            }
          }
          for (auto &predicate_assign_pc : inst_stat->predicate_assign_pcs) {
            predicate_assign_pc += function->address;
          }
        }
      }
    }
  }
}


static int __attribute__((unused)) reg_name_to_id(const std::string &name) {
  // first 7 letters cuda::r
  auto str = name.substr(7);
  return std::stoi(str);
}


class IgnoreRegPred : public Dyninst::Slicer::Predicates {
 public:
  IgnoreRegPred(std::vector<Dyninst::AbsRegion> &rhs) : _rhs(rhs) {}

  virtual bool modifyCurrentFrame(Dyninst::Slicer::SliceFrame &slice_frame,
    Dyninst::GraphPtr graph_ptr, Dyninst::Slicer *slicer) {
    std::vector<Dyninst::AbsRegion> delete_abs_regions;

    for (auto &active_iter : slice_frame.active) {
      // Filter unmatched regs
      auto &abs_region = active_iter.first;
      bool find = false;
      for (auto &rhs_abs_region : _rhs) {
        if (abs_region.absloc().reg() == rhs_abs_region.absloc().reg()) {
          find = true;
          break;
        }
      }
      if (find == false) {
        delete_abs_regions.push_back(abs_region);
      }
    }

    for (auto &abs_region : delete_abs_regions) {
      slice_frame.active.erase(abs_region);
    }

    return true;
  }

 private:
  std::vector<Dyninst::AbsRegion> _rhs;
};


void controlCudaInstructions(const char *cubin, std::vector<Function *> &functions) {
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          // Calculate absolute address
          auto *inst_stat = inst->inst_stat;
          auto offset = inst_stat->pc + function->address + 8;
          // TODO(Keren): only 
          uint64_t bits = *((uint64_t *)(cubin + offset));
          // Reuse
          inst_stat->control.reuse = ((bits & (0x3c00000000000000)) >> 58);
          inst_stat->control.wait  = ((bits & (0x03f0000000000000)) >> 52);
          inst_stat->control.read  = ((bits & (0x000e000000000000)) >> 49);
          inst_stat->control.write = ((bits & (0x0001c00000000000)) >> 46);
          inst_stat->control.yield = ((bits & (0x0000200000000000)) >> 45);
          inst_stat->control.stall = ((bits & (0x00001e0000000000)) >> 41);
          
          auto wait = inst_stat->control.wait;
          for (auto i = 0; i < InstructionStat::WAIT_BITS; ++i) {
            if (wait & (1 << i)) {
              inst_stat->bsrcs.push_back(i);
            }
          }

          auto read = inst_stat->control.read;
          if (read != InstructionStat::BARRIER_NONE) {
            inst_stat->bdsts.push_back(read);
          }

          auto write = inst_stat->control.write;
          if (write != InstructionStat::BARRIER_NONE) {
            inst_stat->bdsts.push_back(write);
          }
        }
      }
    }
  }
}

static int TRACK_LIMIT = 8;

static void trackDependency(const std::map<int, InstructionStat *> &inst_stat_map,
  Dyninst::Address inst_addr, Dyninst::Address func_addr, std::map<int, int> &predicate_map,
  Dyninst::NodeIterator exit_node_iter, InstructionStat *inst_stat, int step) {
  if (step >= TRACK_LIMIT) {
    return;
  }
  Dyninst::NodeIterator in_begin, in_end;
  (*exit_node_iter)->ins(in_begin, in_end);
  for (; in_begin != in_end; ++in_begin) {
    auto slice_node = boost::dynamic_pointer_cast<Dyninst::SliceNode>(*in_begin);
    auto addr = slice_node->addr();
    auto *slice_inst = inst_stat_map.at(addr);

    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "find inst_addr " << inst_addr - func_addr <<
        " <- addr: " << addr - func_addr;
    }

    Dyninst::Assignment::Ptr aptr = slice_node->assign();
    auto reg = aptr->out().absloc().reg();
    auto reg_id = reg.val() & 0xFF;
    if (reg.val() & Dyninst::cuda::PR) {
      if (reg_id == inst_stat->predicate) {
        auto beg = inst_stat->predicate_assign_pcs.begin();
        auto end = inst_stat->predicate_assign_pcs.end();
        if (std::find(beg, end, addr - func_addr) == end) {
          inst_stat->predicate_assign_pcs.push_back(addr - func_addr);
        }
      }

      for (size_t i = 0; i < inst_stat->psrcs.size(); ++i) {
        if (reg_id == inst_stat->psrcs[i]) {
          auto beg = inst_stat->passign_pcs[reg_id].begin();
          auto end = inst_stat->passign_pcs[reg_id].end();
          if (std::find(beg, end, addr - func_addr) == end) {
            inst_stat->passign_pcs[reg_id].push_back(addr - func_addr);
          }
          break;
        }
      }

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << " predicate reg " << reg_id << std::endl;
      }
    } else if (reg.val() & Dyninst::cuda::BR) {
      for (size_t i = 0; i < inst_stat->bsrcs.size(); ++i) {
        if (reg_id == inst_stat->bsrcs[i]) {
          auto beg = inst_stat->bassign_pcs[reg_id].begin();
          auto end = inst_stat->bassign_pcs[reg_id].end();
          if (std::find(beg, end, addr - func_addr) == end) {
            inst_stat->bassign_pcs[reg_id].push_back(addr - func_addr);
          }
          break;
        }
      }

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << " barrier " << reg_id << std::endl;
      }
    } else if (reg.val() & Dyninst::cuda::UR) {
      for (size_t i = 0; i < inst_stat->usrcs.size(); ++i) {
        if (reg_id == inst_stat->usrcs[i]) {
          auto beg = inst_stat->uassign_pcs[reg_id].begin();
          auto end = inst_stat->uassign_pcs[reg_id].end();
          if (std::find(beg, end, addr - func_addr) == end) {
            inst_stat->uassign_pcs[reg_id].push_back(addr - func_addr);
          }
          break;
        }
      }

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << " uniform " << reg_id << std::endl;
      }
    } else if (reg.val() & Dyninst::cuda::UPR) {
      for (size_t i = 0; i < inst_stat->upsrcs.size(); ++i) {
        if (reg_id == inst_stat->upsrcs[i]) {
          auto beg = inst_stat->upassign_pcs[reg_id].begin();
          auto end = inst_stat->upassign_pcs[reg_id].end();
          if (std::find(beg, end, addr - func_addr) == end) {
            inst_stat->upassign_pcs[reg_id].push_back(addr - func_addr);
          }
          break;
        }
      }

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << " uniform predicate " << reg_id << std::endl;
      }
    } else {
      for (size_t i = 0; i < inst_stat->srcs.size(); ++i) {
        if (reg_id == inst_stat->srcs[i]) {
          auto beg = inst_stat->assign_pcs[reg_id].begin();
          auto end = inst_stat->assign_pcs[reg_id].end();
          if (std::find(beg, end, addr - func_addr) == end) {
            inst_stat->assign_pcs[reg_id].push_back(addr - func_addr);
          }
          break;
        }
      }

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << " reg " << reg_id << std::endl;
      }
    }

    if (slice_inst->predicate_flag == InstructionStat::PREDICATE_NONE) {
      // 1. No predicate, stop immediately
    } else if (inst_stat->predicate == slice_inst->predicate &&
      inst_stat->predicate_flag == slice_inst->predicate_flag) {
      // 2. Find an exact match, stop immediately
    } else {
      if ((slice_inst->predicate_flag == InstructionStat::PREDICATE_TRUE && 
          predicate_map[-(slice_inst->predicate + 1)] > 0) || 
        (slice_inst->predicate_flag == InstructionStat::PREDICATE_FALSE && 
         predicate_map[(slice_inst->predicate + 1)] > 0)) {
        // 3. Stop if find both !@PI and @PI
        // add one to avoid P0
      } else {
        // 4. Continue search
        if (slice_inst->predicate_flag == InstructionStat::PREDICATE_TRUE) {
          predicate_map[slice_inst->predicate + 1]++;
        } else {
          predicate_map[-(slice_inst->predicate + 1)]++;
        }

        trackDependency(inst_stat_map, inst_addr, func_addr, predicate_map,
          in_begin, inst_stat, step + 1);
        
        // Clear
        if (slice_inst->predicate_flag == InstructionStat::PREDICATE_TRUE) {
          predicate_map[slice_inst->predicate + 1]--;
        } else {
          predicate_map[-(slice_inst->predicate + 1)]--;
        }
      }
    }
  }
}


class FirstMatchPred : public Dyninst::Slicer::Predicates {
 public:
  virtual bool endAtPoint(Dyninst::Assignment::Ptr ap) {
    return true;
  }
};


void sliceCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  int threads, std::vector<Function *> &functions) {
  // Build a instruction map
  std::map<int, InstructionStat *> inst_stat_map;
  std::map<int, Block *> inst_block_map;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          auto *inst_stat = inst->inst_stat;
          inst_stat_map[inst->offset] = inst_stat;
          inst_block_map[inst->offset] = block;
        }
      }
    }
  }

  std::vector<std::pair<Dyninst::ParseAPI::Block *, Dyninst::ParseAPI::Function *>> block_vec;
  for (auto dyn_func : func_set) {
    for (auto *dyn_block : dyn_func->blocks()) {
      block_vec.emplace_back(dyn_block, dyn_func);
    }
  }

  // TODO(Keren)
  // Prepare pass: create instruction cache for slicing
  Dyninst::AssignmentConverter ac(true, false);
  Dyninst::Slicer::InsnCache dyn_inst_cache;
    
  #pragma omp parallel for schedule(dynamic) firstprivate(ac, dyn_inst_cache) num_threads(threads)
  for (size_t i = 0; i < block_vec.size(); ++i) {
    auto *dyn_block = block_vec[i].first;
    auto *dyn_func = block_vec[i].second;
    auto func_addr = dyn_func->addr();

    Dyninst::ParseAPI::Block::Insns insns;
    dyn_block->getInsns(insns);

    for (auto &inst_iter : insns) {
      auto &inst = inst_iter.second;
      auto inst_addr = inst_iter.first;
      auto *inst_stat = inst_stat_map.at(inst_addr);

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << inst_stat->pc << std::endl;
        std::cout << "try to find inst_addr " << inst_addr - func_addr << std::endl;
      }

      std::vector<Dyninst::Assignment::Ptr> assignments;
      ac.convert(inst, inst_addr, dyn_func, dyn_block, assignments); 

      for (auto a : assignments) {
#ifdef FAST_SLICING
        FirstMatchPred p;
#else
        IgnoreRegPred p(a->inputs());
#endif

        Dyninst::Slicer s(a, dyn_block, dyn_func, &ac, &dyn_inst_cache);
        Dyninst::GraphPtr g = s.backwardSlice(p); 

        Dyninst::NodeIterator exit_begin, exit_end;
        g->exitNodes(exit_begin, exit_end);

        for (; exit_begin != exit_end; ++exit_begin) {
          std::map<int, int> predicate_map;
          // DFS to iterate the whole dependency graph
          if (inst_stat->predicate_flag == InstructionStat::PREDICATE_TRUE) {
            predicate_map[inst_stat->predicate + 1]++;
          } else if (inst_stat->predicate_flag == InstructionStat::PREDICATE_FALSE) {
            predicate_map[-(inst_stat->predicate + 1)]++;
          }
#ifdef FAST_SLICING
          TRACK_LIMIT = 1;
#endif
          trackDependency(inst_stat_map, inst_addr, func_addr, predicate_map,
            exit_begin, inst_stat, 0);
        }
      }
    }
  }
}


void processLivenessCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions) {
}


bool dumpCudaInstructions(const std::string &file_path,
  const std::vector<Function *> &functions) {
  boost::property_tree::ptree root;

  for (auto *function : functions) {
    boost::property_tree::ptree ptree_function;
    boost::property_tree::ptree ptree_blocks;
    ptree_function.put("id", function->id);
    ptree_function.put("index", function->index);
    ptree_function.put("name", function->name);
    ptree_function.put("address", function->address);
    ptree_function.put("global", function->global);
    ptree_function.put("size", function->size);
    ptree_function.put("unparsable", function->unparsable);

    if (function->unparsable == true) {
      continue;
    }

    for (auto *block : function->blocks) {
      boost::property_tree::ptree ptree_block;
      boost::property_tree::ptree ptree_insts;
      boost::property_tree::ptree ptree_targets;
      ptree_block.put("id", block->id);
      ptree_block.put("name", block->name);
      ptree_block.put("address", block->address);

      // Targets
      for (auto *target : block->targets) {
        boost::property_tree::ptree ptree_target;
        ptree_target.put("id", target->block->id);
        ptree_target.put("pc", target->inst->inst_stat->pc);
        ptree_target.put("type", target->type);
        ptree_targets.push_back(std::make_pair("", ptree_target));
      }

      ptree_block.add_child("targets", ptree_targets);

      // Insts
      for (auto *inst : block->insts) {
        boost::property_tree::ptree ptree_inst;
        boost::property_tree::ptree ptree_srcs;
        boost::property_tree::ptree ptree_dsts;
        boost::property_tree::ptree ptree_usrcs;
        boost::property_tree::ptree ptree_udsts;
        boost::property_tree::ptree ptree_psrcs;
        boost::property_tree::ptree ptree_pdsts;
        boost::property_tree::ptree ptree_upsrcs;
        boost::property_tree::ptree ptree_updsts;
        boost::property_tree::ptree ptree_bsrcs;
        boost::property_tree::ptree ptree_bdsts;

        // Instruction offsets have been relocated
        if (inst->inst_stat == NULL) {
          // Append artificial NOP instructions
          ptree_inst.put("pc", inst->offset - function->address);
          ptree_inst.put("op", "MISC.OTHER");
        } else {
          // Append Normal instructions
          ptree_inst.put("pc", inst->offset - function->address);
          ptree_inst.put("op", inst->inst_stat->op);
          ptree_inst.put("pred", inst->inst_stat->predicate);

          boost::property_tree::ptree ptree_predicate_assign_pcs;
          for (auto predicate_assign_pc : inst->inst_stat->predicate_assign_pcs) {
            boost::property_tree::ptree tt;
            tt.put("", predicate_assign_pc);
            ptree_predicate_assign_pcs.push_back(std::make_pair("", tt));
          }

          ptree_inst.add_child("pred_assign_pcs", ptree_predicate_assign_pcs);
          ptree_inst.put("pred_flag", inst->inst_stat->predicate_flag);

          // Control info
          boost::property_tree::ptree control;
          control.put("reuse", inst->inst_stat->control.reuse);
          control.put("wait",  inst->inst_stat->control.wait);
          control.put("read",  inst->inst_stat->control.read);
          control.put("write", inst->inst_stat->control.write);
          control.put("yield", inst->inst_stat->control.yield);
          control.put("stall", inst->inst_stat->control.stall);
          ptree_inst.add_child("control", control);

          // Dest operands, dest predicate operands, and dest barriers
          std::function<void(const std::string &, boost::property_tree::ptree &, std::vector<int> &)> output_dsts =
            [&ptree_inst](const std::string &dst_name, boost::property_tree::ptree &ptree_dsts, std::vector<int> &dsts) {
              for (auto dst : dsts) {
                boost::property_tree::ptree t;
                t.put("", dst);
                ptree_dsts.push_back(std::make_pair("", t));
              }
              ptree_inst.add_child(dst_name, ptree_dsts);
            };

          output_dsts("dsts", ptree_dsts, inst->inst_stat->dsts);
          output_dsts("udsts", ptree_udsts, inst->inst_stat->udsts);
          output_dsts("pdsts", ptree_pdsts, inst->inst_stat->pdsts);
          output_dsts("updsts", ptree_updsts, inst->inst_stat->updsts);
          output_dsts("bdsts", ptree_bdsts, inst->inst_stat->bdsts);

          // Source operands, source predicate operands, and dest barriers
          std::function<void(const std::string &, const std::string &, boost::property_tree::ptree &, std::vector<int> &, std::map<int, std::vector<int>> &)> output_srcs =
            [&ptree_inst](const std::string &src_name, const std::string &assign_name, boost::property_tree::ptree &ptree_srcs, std::vector<int> &srcs, std::map<int, std::vector<int>> &assign_pcs) {
              for (auto src : srcs) {
                boost::property_tree::ptree t;
                t.put("id", src);

                boost::property_tree::ptree ptree_assign_pcs;
                auto iter = assign_pcs.find(src);
                if (iter != assign_pcs.end()) {
                  for (auto assign_pc : iter->second) {
                    boost::property_tree::ptree tt;
                    tt.put("", assign_pc);
                    ptree_assign_pcs.push_back(std::make_pair("", tt));
                  }
                }
                t.add_child(assign_name, ptree_assign_pcs);

                ptree_srcs.push_back(std::make_pair("", t));
              }

              ptree_inst.add_child(src_name, ptree_srcs);
            };
          
          output_srcs("srcs", "assign_pcs", ptree_srcs, inst->inst_stat->srcs, inst->inst_stat->assign_pcs);
          output_srcs("usrcs", "uassign_pcs", ptree_usrcs, inst->inst_stat->usrcs, inst->inst_stat->uassign_pcs);
          output_srcs("psrcs", "passign_pcs", ptree_psrcs, inst->inst_stat->psrcs, inst->inst_stat->passign_pcs);
          output_srcs("upsrcs", "upassign_pcs", ptree_upsrcs, inst->inst_stat->upsrcs, inst->inst_stat->upassign_pcs);
          output_srcs("bsrcs", "bassign_pcs", ptree_bsrcs, inst->inst_stat->bsrcs, inst->inst_stat->bassign_pcs);
        }

        ptree_insts.push_back(std::make_pair("", ptree_inst));
      }

      ptree_block.add_child("insts", ptree_insts);
      ptree_blocks.push_back(std::make_pair("", ptree_block));
    }

    ptree_function.add_child("blocks", ptree_blocks);
    root.push_back(std::make_pair("", ptree_function));
  }

  if (INSTRUCTION_ANALYZER_DEBUG) {
    boost::property_tree::write_json(file_path, root, std::locale(), true);
  } else {
    boost::property_tree::write_json(file_path, root, std::locale(), false);
  }

  return true;
}


bool readCudaInstructions(const std::string &file_path, std::vector<Function *> &functions) {
  boost::property_tree::ptree root;

  boost::property_tree::read_json(file_path, root);

  // CFG does not have parallel edges
  // block-> <target block id, <pc, type> > 
  std::map<Block *, std::map<int, std::pair<int, int> > > block_target_map;
  std::map<int, Block *> block_map;
  std::map<int, Instruction *> inst_map;

  for (auto &ptree_function : root) {
    int function_id = ptree_function.second.get<int>("id", 0);
    int function_index = ptree_function.second.get<int>("index", 0);
    int function_address = ptree_function.second.get<int>("address", 0);
    int size = ptree_function.second.get<int>("size", 0);
    std::string name = ptree_function.second.get<std::string>("name", "");
    bool unparsable = ptree_function.second.get<bool>("unparsable", "");
    bool global = ptree_function.second.get<bool>("global", "");
    auto *function = new Function(function_id, function_index, name, function_address, unparsable, global, size);

    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Function id: " << function_id << std::endl;
    }

    if (function->unparsable) {
      // Skip unparsable functions
      continue;
    }

    auto &ptree_blocks = ptree_function.second.get_child("blocks");
    for (auto &ptree_block : ptree_blocks) {
      int block_id = ptree_block.second.get<int>("id", 0);
      std::string name = ptree_block.second.get<std::string>("name", "");
      auto *block = new Block(block_id, name);
      block_map[block_id] = block;

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << "Block id: " << block_id << std::endl;
      }

      // Record targets id first
      std::vector<int> tgts; 
      auto &ptree_tgts = ptree_block.second.get_child("targets");
      for (auto &ptree_tgt : ptree_tgts) {
        int inst_pc = ptree_tgt.second.get<int>("pc", 0);
        int target_id = ptree_tgt.second.get<int>("id", 0);
        int type = ptree_tgt.second.get<int>("type", 0);
        block_target_map[block][target_id] = std::make_pair(inst_pc, type);
      }

      // Insts
      auto &ptree_insts = ptree_block.second.get_child("insts");
      for (auto &ptree_inst : ptree_insts) {
        int pc = ptree_inst.second.get<int>("pc", 0);
        std::string op = ptree_inst.second.get<std::string>("op", "");
        int pred = ptree_inst.second.get<int>("pred", -1);
        InstructionStat::PredicateFlag pred_flag = static_cast<InstructionStat::PredicateFlag>(
          ptree_inst.second.get<int>("pred_flag", 0));
        std::vector<int> pred_assign_pcs;
        auto &ptree_pred_assign_pcs = ptree_inst.second.get_child("pred_assign_pcs");
        for (auto &ptree_pred_assign_pc : ptree_pred_assign_pcs) {
          int pred_assign_pc = boost::lexical_cast<int>(ptree_pred_assign_pc.second.data());
          pred_assign_pcs.push_back(pred_assign_pc);
        }

        InstructionStat::Control control;
        auto &ptree_control = ptree_inst.second.get_child("control");
        control.reuse = ptree_control.get<int>("reuse", 0);
        control.wait = ptree_control.get<int>("wait", 0);
        control.read = ptree_control.get<int>("read", 0);
        control.write = ptree_control.get<int>("write", 0);
        control.yield = ptree_control.get<int>("yield", 0);
        control.stall = ptree_control.get<int>("stall", 0);

        std::function<void(const std::string &, std::vector<int> &)> fill_dsts =
          [&ptree_inst](const std::string &dst_name, std::vector<int> &dsts) {
            auto &ptree_dsts = ptree_inst.second.get_child(dst_name);
            for (auto &ptree_dst : ptree_dsts) {
              int dst = boost::lexical_cast<int>(ptree_dst.second.data());
              dsts.push_back(dst);
            }
          };

        std::vector<int> dsts;
        fill_dsts("dsts", dsts);

        std::vector<int> udsts;
        fill_dsts("udsts", udsts);

        std::vector<int> pdsts;
        fill_dsts("pdsts" ,pdsts);

        std::vector<int> updsts;
        fill_dsts("updsts", updsts);

        std::vector<int> bdsts;
        fill_dsts("bdsts", bdsts);

        std::function<void(const std::string &, const std::string &, std::vector<int> &, std::map<int, std::vector<int>> &)> fill_srcs =
          [&ptree_inst](const std::string &src_name, const std::string &assign_name, std::vector<int> &srcs, std::map<int, std::vector<int>> &assign_pcs) {
            auto &ptree_srcs = ptree_inst.second.get_child(src_name);
            for (auto &ptree_src : ptree_srcs) {
              int src = ptree_src.second.get<int>("id", 0);
              srcs.push_back(src);
              auto &ptree_assign_pcs = ptree_src.second.get_child(assign_name);
              for (auto &ptree_assign_pc : ptree_assign_pcs) {
                int assign_pc = boost::lexical_cast<int>(ptree_assign_pc.second.data());
                assign_pcs[src].push_back(assign_pc);
              }
            }
          };

        std::vector<int> srcs; 
        std::map<int, std::vector<int> > assign_pcs;
        fill_srcs("srcs", "assign_pcs", srcs, assign_pcs); 

        std::vector<int> usrcs; 
        std::map<int, std::vector<int> > uassign_pcs;
        fill_srcs("usrcs", "uassign_pcs", usrcs, uassign_pcs); 

        std::vector<int> psrcs; 
        std::map<int, std::vector<int> > passign_pcs;
        fill_srcs("psrcs", "passign_pcs", psrcs, passign_pcs); 

        std::vector<int> upsrcs; 
        std::map<int, std::vector<int> > upassign_pcs;
        fill_srcs("upsrcs", "upassign_pcs", upsrcs, upassign_pcs); 

        std::vector<int> bsrcs;
        std::map<int, std::vector<int> > bassign_pcs;
        fill_srcs("bsrcs", "bassign_pcs", bsrcs, bassign_pcs); 

        auto *inst_stat = new InstructionStat(op, pc, pred, pred_flag, pred_assign_pcs, dsts, srcs,
          pdsts, psrcs, bdsts, bsrcs, udsts, usrcs, updsts, upsrcs,
          assign_pcs, passign_pcs, bassign_pcs, uassign_pcs, upassign_pcs, control);
        auto *inst = new Instruction(inst_stat);
        inst_map[pc] = inst;

        if (INSTRUCTION_ANALYZER_DEBUG) {
          std::cout << "Inst pc: " << pc << std::endl;
          std::cout << "     op: " << op << std::endl;
          std::cout << "     pred: " << pred << std::endl;
          std::cout << "     pred_flag: " << pred_flag << std::endl;
          std::cout << "     dsts: ";
          for (auto dst : dsts) {
            std::cout << dst << ", ";
          }
          std::cout << std::endl;
          std::cout << "     srcs: " << std::endl;
          for (auto src : srcs) {
            if (src != -1) {
              std::cout << "     " << src << "-> ";
              auto iter = assign_pcs.find(src);
              if (iter != assign_pcs.end()) {
                for (auto assign_pc : iter->second) {
                  std::cout << "0x" << std::hex << assign_pc << std::dec << ", ";
                }
              }
              std::cout << std::endl;
            }
          }
          std::cout << "     udsts: ";
          for (auto udst : udsts) {
            std::cout << udst << ", ";
          }
          std::cout << std::endl;
          std::cout << "     usrcs: " << std::endl;
          for (auto usrc : usrcs) {
            if (usrc != -1) {
              std::cout << "     " << usrc << "-> ";
              auto iter = uassign_pcs.find(usrc);
              if (iter != uassign_pcs.end()) {
                for (auto uassign_pc : iter->second) {
                  std::cout << "0x" << std::hex << uassign_pc << std::dec << ", ";
                }
              }
              std::cout << std::endl;
            }
          }
          std::cout << "     pdsts: ";
          for (auto pdst : pdsts) {
            std::cout << pdst << ", ";
          }
          std::cout << std::endl;
          std::cout << "     psrcs: " << std::endl;
          for (auto psrc : psrcs) {
            if (psrc != -1) {
              std::cout << "     " << psrc << ", ";
              auto iter = passign_pcs.find(psrc);
              if (iter != passign_pcs.end()) {
                for (auto passign_pc : iter->second) {
                  std::cout << "0x" << std::hex << passign_pc << std::dec << ", ";
                }
              }
              std::cout << std::endl;
            }
          }
          std::cout << "     updsts: ";
          for (auto updst : updsts) {
            std::cout << updst << ", ";
          }
          std::cout << std::endl;
          std::cout << "     upsrcs: " << std::endl;
          for (auto upsrc : upsrcs) {
            if (upsrc != -1) {
              std::cout << "     " << upsrc << ", ";
              auto iter = upassign_pcs.find(upsrc);
              if (iter != upassign_pcs.end()) {
                for (auto upassign_pc : iter->second) {
                  std::cout << "0x" << std::hex << upassign_pc << std::dec << ", ";
                }
              }
              std::cout << std::endl;
            }
          }
          std::cout << "     bdsts: ";
          for (auto bdst : bdsts) {
            std::cout << bdst << ", ";
          }
          std::cout << std::endl;
          std::cout << "     bsrcs: " << std::endl;
          for (auto bsrc : bsrcs) {
            if (bsrc != -1) {
              std::cout << "     " << bsrc << ", ";
              auto iter = bassign_pcs.find(bsrc);
              if (iter != bassign_pcs.end()) {
                for (auto bassign_pc : iter->second) {
                  std::cout << "0x" << std::hex << bassign_pc << std::dec << ", ";
                }
              }
              std::cout << std::endl;
            }
          }
        }

        block->insts.emplace_back(inst);
      }

      function->blocks.emplace_back(block);
    }

    functions.emplace_back(function);
  }

  // Reconstruct targets
  for (auto &block_iter : block_target_map) {
    auto *block = block_iter.first;
    for (auto &iter : block_iter.second) {
      auto target_id = iter.first;
      auto pc = iter.second.first;
      auto type = iter.second.second;
      auto *inst = inst_map[pc];
      auto *target_block = block_map[target_id];
      block->targets.push_back(new Target(inst, target_block, (TargetType)(type)));
    }
  }

  return true;
}

}  // namespace CudaParse
