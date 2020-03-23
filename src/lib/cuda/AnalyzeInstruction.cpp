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

namespace CudaParse {

template <InstructionType inst_type>
void analyze_instruction(const Instruction &inst, std::string &op);

template <>
void analyze_instruction<INS_TYPE_MEMORY>(const Instruction &inst, std::string &op) {
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
void analyze_instruction<INS_TYPE_FLOAT>(const Instruction &inst, std::string &op) {
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
void analyze_instruction<INS_TYPE_INTEGER>(const Instruction &inst, std::string &op) {
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
void analyze_instruction<INS_TYPE_TEXTRUE>(const Instruction &inst, std::string &op) {
  op = "TEXTURE";
}


template <>
void analyze_instruction<INS_TYPE_PREDICATE>(const Instruction &inst, std::string &op) {
  op = "PREDICATE";
}


template <>
void analyze_instruction<INS_TYPE_CONTROL>(const Instruction &inst, std::string &op) {
  op = "CONTROL";

  std::string type;

  const std::string &opcode = inst.opcode;

  if (opcode.find("MEMBAR") != std::string::npos ||
    opcode.find("DEPBAR") != std::string::npos) {
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
void analyze_instruction<INS_TYPE_MISC>(const Instruction &inst, std::string &op) {
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


InstructionStat::InstructionStat(const Instruction *inst) {
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
      this->predicate = convert_reg(inst->predicate, pos + 1);
      pos = inst->predicate.find("!");
      if (pos != std::string::npos) {
        this->predicate_flag = InstructionStat::PredicateFlag::PREDICATE_FALSE;
      } else {
        this->predicate_flag = InstructionStat::PredicateFlag::PREDICATE_TRUE;
      }
    }
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
    if (pos != std::string::npos) {
      if (inst->operands[0].find("\[") != std::string::npos) {  // store instruction
        store = true;
      }

      auto reg = convert_reg(inst->operands[0], pos + 1);
      if (store) {
        if (this->op.find(".SHARED") != std::string::npos ||
          this->op.find(".LOCAL") != std::string::npos) {
          // memory 32-bit
          this->srcs.push_back(reg);
        } else {
          // memory 64-bit
          this->srcs.push_back(reg);
          this->srcs.push_back(reg + 1);
        }
      } else {
        // load or arithmetic
        if (this->op.find(".64") != std::string::npos ||
          this->op.find("._32_TO_64") != std::string::npos) {  // vec 64
          this->dsts.push_back(reg);
          this->dsts.push_back(reg + 1);
        } else if (this->op.find(".128") != std::string::npos) {  // vec 128
          this->dsts.push_back(reg);
          this->dsts.push_back(reg + 1);
          this->dsts.push_back(reg + 2);
          this->dsts.push_back(reg + 3);
        } else {  // vec 32, 16, 8
          this->dsts.push_back(reg);
        }
      }
    }

    // Predicate register
    pos = inst->operands[0].find("P");
    if (pos != std::string::npos) {
      auto reg = convert_reg(inst->operands[0], pos + 1);
      this->pdsts.push_back(reg);
    }

    for (size_t i = 1; i < inst->operands.size(); ++i) {
      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << inst->operands[i] << " ";
      }

      pos = inst->operands[i].find("R");
      if (pos != std::string::npos) {
        auto reg = convert_reg(inst->operands[i], pos + 1);
        if (this->op.find(".LOAD") != std::string::npos) {
          // load
          if (this->op.find(".SHARED") != std::string::npos ||
            this->op.find(".LOCAL") != std::string::npos) {
            // memory 32-bit
            this->srcs.push_back(reg);
          } else {
            // memory 64-bit
            this->srcs.push_back(reg);
            this->srcs.push_back(reg + 1);
          }
        } else {
          if (this->op.find("INTEGER") != std::string::npos) {
            // integer source only have 32
            this->srcs.push_back(reg);
          } else {
            // arithmetic or store
            if (this->op.find(".64") != std::string::npos ||
              this->op.find("._64_TO_32") != std::string::npos) {  // vec 64
              if (reg == -1) {  // rz
                this->srcs.push_back(reg);
                this->srcs.push_back(reg);
              } else {
                this->srcs.push_back(reg);
                this->srcs.push_back(reg + 1);
              }
            } else if (this->op.find(".128") != std::string::npos) {  // vec 128
              if (reg == -1) {
                this->srcs.push_back(-1);
                this->srcs.push_back(-1);
                this->srcs.push_back(-1);
                this->srcs.push_back(-1);
              } else {
                this->srcs.push_back(reg);
                this->srcs.push_back(reg + 1);
                this->srcs.push_back(reg + 2);
                this->srcs.push_back(reg + 3);
              }
            } else {  // vec 32, 16, 8
              this->srcs.push_back(reg);
            }
          }
        }
      }

      pos = inst->operands[i].find("P");
      if (pos != std::string::npos) {
        auto reg = convert_reg(inst->operands[i], pos + 1);
        if (reg != -1) {
          // Preventing PT
          this->psrcs.push_back(reg);
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
        }
      }
    }
  }
}


static int reg_name_to_id(const std::string &name) {
  // first 7 letters cuda::r
  auto str = name.substr(7);
  return std::stoi(str);
}


class FirstMatchPred : public Dyninst::Slicer::Predicates {
 public:
  virtual bool endAtPoint(Dyninst::Assignment::Ptr ap) {
    return true;
  }
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
        }
      }
    }
  }
}


void sliceCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions) {
  // Build a instruction map
  std::map<int, InstructionStat *> inst_stats_map;
  std::map<int, Block *> inst_block_map;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          auto *inst_stat = inst->inst_stat;
          inst_stats_map[inst->offset] = inst_stat;
          inst_block_map[inst->offset] = block;
        }
      }
    }
  }

  for (auto *dyn_func : func_set) {
    Dyninst::AssignmentConverter ac(true, false);
    auto func_addr = dyn_func->addr();

    Dyninst::Slicer::InsnCache dyn_inst_cache;
    for (auto *dyn_block : dyn_func->blocks()) {
      // Create instruction cache for slicing
      Dyninst::ParseAPI::Block::Insns insns;
      dyn_block->getInsns(insns);
      for (auto &iter : insns) {
        std::pair<Dyninst::InstructionAPI::Instruction, Dyninst::Address> p(iter.second, iter.first);
        dyn_inst_cache[dyn_block].emplace_back(std::move(p));
      }
    }

    for (auto *dyn_block : dyn_func->blocks()) {
      Dyninst::ParseAPI::Block::Insns insns;
      dyn_block->getInsns(insns);

      for (auto &inst_iter : insns) {
        auto inst = inst_iter.second;
        auto inst_addr = inst_iter.first;
        auto *inst_stat = inst_stats_map[inst_addr];

        if (INSTRUCTION_ANALYZER_DEBUG) {
          std::cout << "try to find inst_addr " << inst_addr - func_addr << std::endl;
        }

        std::vector<Dyninst::Assignment::Ptr> assignments;
        ac.convert(inst, inst_addr, dyn_func, dyn_block, assignments); 

        for (auto a : assignments) {
          FirstMatchPred p;
          Dyninst::Slicer s(a, dyn_block, dyn_func, &ac, &dyn_inst_cache);
          Dyninst::GraphPtr g = s.backwardSlice(p); 

          Dyninst::NodeIterator exit_begin, exit_end;
          g->exitNodes(exit_begin, exit_end);

          for (; exit_begin != exit_end; ++exit_begin) {
            Dyninst::NodeIterator in_begin, in_end;
            (*exit_begin)->ins(in_begin, in_end);
            for (; in_begin != in_end; ++in_begin) {
              auto slice_node = boost::dynamic_pointer_cast<Dyninst::SliceNode>(*in_begin);
              auto addr = slice_node->addr();

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
            }
          }
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
        boost::property_tree::ptree ptree_psrcs;
        boost::property_tree::ptree ptree_pdsts;

        // Instruction offsets have been relocated
        if (inst->inst_stat == NULL) {
          // Append artificial NOP instructions
          ptree_inst.put("pc", inst->offset - function->address);
          ptree_inst.put("op", "MISC.OTHER");
        } else {
          // Append Normal instructions
          ptree_inst.put("pc", inst->offset - function->address);
          ptree_inst.put("op", inst->inst_stat->op);
          if (inst->inst_stat->predicate != -1) {
            ptree_inst.put("pred", inst->inst_stat->predicate);

            boost::property_tree::ptree ptree_predicate_assign_pcs;
            for (auto predicate_assign_pc : inst->inst_stat->predicate_assign_pcs) {
              boost::property_tree::ptree tt;
              tt.put("", predicate_assign_pc);
              ptree_predicate_assign_pcs.push_back(std::make_pair("", tt));
            }
            ptree_inst.add_child("pred_assign_pcs", ptree_predicate_assign_pcs);
          } else {
            ptree_inst.put("pred", "");
          }
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

          // Dest operands and dest predicate operands
          for (auto dst : inst->inst_stat->dsts) {
            boost::property_tree::ptree t;
            t.put("", dst);
            ptree_dsts.push_back(std::make_pair("", t));
          }
          ptree_inst.add_child("dsts", ptree_dsts);

          for (auto pdst : inst->inst_stat->pdsts) {
            boost::property_tree::ptree t;
            t.put("", pdst);
            ptree_pdsts.push_back(std::make_pair("", t));
          }
          ptree_inst.add_child("pdsts", ptree_pdsts);

          // Source operands and source predicate operands
          for (auto src : inst->inst_stat->srcs) {
            boost::property_tree::ptree t;
            t.put("id", src);

            boost::property_tree::ptree ptree_assign_pcs;
            auto iter = inst->inst_stat->assign_pcs.find(src);
            if (iter != inst->inst_stat->assign_pcs.end()) {
              for (auto assign_pc : iter->second) {
                boost::property_tree::ptree tt;
                tt.put("", assign_pc);
                ptree_assign_pcs.push_back(std::make_pair("", tt));
              }
            }
            t.add_child("assign_pcs", ptree_assign_pcs);

            ptree_srcs.push_back(std::make_pair("", t));
          }

          ptree_inst.add_child("srcs", ptree_srcs);

          for (auto psrc : inst->inst_stat->psrcs) {
            boost::property_tree::ptree t;
            t.put("id", psrc);

            boost::property_tree::ptree ptree_passign_pcs;
            auto iter = inst->inst_stat->passign_pcs.find(psrc);
            if (iter != inst->inst_stat->passign_pcs.end()) {
              for (auto passign_pc : iter->second) {
                boost::property_tree::ptree tt;
                tt.put("", passign_pc);
                ptree_passign_pcs.push_back(std::make_pair("", tt));
              }
            }
            t.add_child("passign_pcs", ptree_passign_pcs);

            ptree_psrcs.push_back(std::make_pair("", t));
          }

          ptree_inst.add_child("psrcs", ptree_psrcs);
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
    std::string name = ptree_function.second.get<std::string>("name", "");
    auto *function = new Function(function_id, function_index, name, function_address);

    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Function id: " << function_id << std::endl;
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

        std::vector<int> dsts;
        auto &ptree_dsts = ptree_inst.second.get_child("dsts");
        for (auto &ptree_dst : ptree_dsts) {
          int dst = boost::lexical_cast<int>(ptree_dst.second.data());
          dsts.push_back(dst);
        }

        std::vector<int> pdsts;
        auto &ptree_pdsts = ptree_inst.second.get_child("pdsts");
        for (auto &ptree_pdst : ptree_pdsts) {
          int pdst = boost::lexical_cast<int>(ptree_pdst.second.data());
          pdsts.push_back(pdst);
        }

        std::vector<int> srcs; 
        std::map<int, std::vector<int> > assign_pcs;
        auto &ptree_srcs = ptree_inst.second.get_child("srcs");
        for (auto &ptree_src : ptree_srcs) {
          int src = ptree_src.second.get<int>("id", 0);
          srcs.push_back(src);
          auto &ptree_assign_pcs = ptree_src.second.get_child("assign_pcs");
          for (auto &ptree_assign_pc : ptree_assign_pcs) {
            int assign_pc = boost::lexical_cast<int>(ptree_assign_pc.second.data());
            assign_pcs[src].push_back(assign_pc);
          }
        }

        std::vector<int> psrcs; 
        std::map<int, std::vector<int> > passign_pcs;
        auto &ptree_psrcs = ptree_inst.second.get_child("psrcs");
        for (auto &ptree_psrc : ptree_psrcs) {
          int psrc = ptree_psrc.second.get<int>("id", 0);
          psrcs.push_back(psrc);
          auto &ptree_passign_pcs = ptree_psrc.second.get_child("passign_pcs");
          for (auto &ptree_passign_pc : ptree_passign_pcs) {
            int passign_pc = boost::lexical_cast<int>(ptree_passign_pc.second.data());
            passign_pcs[psrc].push_back(passign_pc);
          }
        }

        auto *inst_stat = new InstructionStat(op, pc, pred, pred_flag, pred_assign_pcs, dsts, srcs,
          pdsts, psrcs, assign_pcs, passign_pcs, control);
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
          std::cout << "     srcs: ";
          for (auto src : srcs) {
            std::cout << src << ": ";
            auto iter = assign_pcs.find(src);
            if (iter != assign_pcs.end()) {
              for (auto assign_pc : iter->second) {
                std::cout << assign_pc << std::endl;
              }
            }
          }
          std::cout << std::endl;
          std::cout << "     pdsts: ";
          for (auto pdst : pdsts) {
            std::cout << pdst << ", ";
          }
          std::cout << std::endl;
          std::cout << "     psrcs: ";
          for (auto psrc : psrcs) {
            std::cout << psrc << ": ";
            auto iter = passign_pcs.find(psrc);
            if (iter != passign_pcs.end()) {
              for (auto passign_pc : iter->second) {
                std::cout << passign_pc << std::endl;
              }
            }
          }
          std::cout << std::endl;
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
