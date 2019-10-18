#include "AnalyzeInstruction.hpp"

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
    if (modifier == "64" || modifier == "128") {
      width = "." + modifier;
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

  op += type;
}


template <>
void analyze_instruction<INS_TYPE_TEXTRUE>(const Instruction &inst, std::string &op) {
  op = "TEXTURE";
}


template <>
void analyze_instruction<INS_TYPE_CONTROL>(const Instruction &inst, std::string &op) {
  op = "CONTROL";

  std::string type;

  const std::string &opcode = inst.opcode;

  if (opcode.find("MEMBAR") != std::string::npos ||
    opcode.find("DEPBAR") != std::string::npos) {
    type = ".BAR";
  } else if (opcode.find("SYNC") != std::string::npos ||
    opcode.find("BAR") != std::string::npos) {
    type = ".SYNC";
    if (opcode.find("WARP") != std::string::npos) {
      type += ".WARP";
    } else {
      type += ".BLOCK";
    }
  } else if (opcode.find("SHFL") != std::string::npos) {
    type = ".SHFL";
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
  this->dst = -1;

  if (inst->predicate.size() != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << inst->predicate << " ";
    }

    auto pos = inst->predicate.find("P");
    if (pos != std::string::npos) {
      this->predicate = convert_reg(inst->predicate, pos + 1);
    }
  }

  if (inst->operands.size() != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << inst->operands[0] << " ";
    }

    auto pos = inst->operands[0].find("R");
    if (pos != std::string::npos) {
      this->dst = convert_reg(inst->operands[0], pos + 1);
    }

    for (size_t i = 1; i < inst->operands.size(); ++i) {
      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << inst->operands[i] << " ";
      }

      pos = inst->operands[i].find("R");
      if (pos != std::string::npos) {
        this->srcs.push_back(convert_reg(inst->operands[i], pos + 1));
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
        inst_stats.emplace_back(inst->inst_stat);
      }
    }
  }

  std::sort(inst_stats.begin(), inst_stats.end(), InstructionStatPointCompare());
}


void sliceCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions) {
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

        if (inst->inst_stat == NULL) {
          // Append NOP instructions
          ptree_inst.put("pc", inst->offset);
          ptree_inst.put("op", "NOP");
        } else {
          // Append Normal instructions
          ptree_inst.put("pc", inst->inst_stat->pc);
          ptree_inst.put("op", inst->inst_stat->op);
          if (inst->inst_stat->predicate != -1) {
            ptree_inst.put("pred", inst->inst_stat->predicate);
          } else {
            ptree_inst.put("pred", "");
          }
          if (inst->inst_stat->dst != -1) {
            ptree_inst.put("dst", inst->inst_stat->dst);
          } else {
            ptree_inst.put("dst", "");
          }

          for (auto src : inst->inst_stat->srcs) {
            boost::property_tree::ptree t;
            t.put("", src);
            ptree_srcs.push_back(std::make_pair("", t));
          }

          ptree_inst.add_child("srcs", ptree_srcs);
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

  // block-> < pc, <target block id, type> > 
  std::map<Block *, std::map<int, std::pair<int, int> > > block_target_map;
  std::map<int, Block *> block_map;
  std::map<int, Instruction *> inst_map;

  for (auto &ptree_function : root) {
    int function_id = ptree_function.second.get<int>("id", 0);
    std::string name = ptree_function.second.get<std::string>("name", "");
    auto *function = new Function(function_id, name);

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
        block_target_map[block][inst_pc] = std::make_pair(target_id, type);
      }

      // Insts
      auto &ptree_insts = ptree_block.second.get_child("insts");
      for (auto &ptree_inst : ptree_insts) {
        int pc = ptree_inst.second.get<int>("pc", 0);
        std::string op = ptree_inst.second.get<std::string>("op", "");
        int pred = ptree_inst.second.get<int>("pred", -1);
        int dst = ptree_inst.second.get<int>("dst", -1);

        std::vector<int> srcs; 
        auto &ptree_srcs = ptree_inst.second.get_child("srcs");
        for (auto &ptree_src : ptree_srcs) {
          srcs.push_back(boost::lexical_cast<int>(ptree_src.second.data()));
        }

        auto *inst_stat = new InstructionStat(op, pc, pred, dst, srcs);
        auto *inst = new Instruction(inst_stat);
        inst_map[pc] = inst;

        if (INSTRUCTION_ANALYZER_DEBUG) {
          std::cout << "Inst pc: " << pc << std::endl;
          std::cout << "     op: " << op << std::endl;
          std::cout << "     pred: " << pred << std::endl;
          std::cout << "     dst: " << dst << std::endl;
          std::cout << "     srcs: ";
          for (auto src : srcs) {
            std::cout << src << ",";
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
      auto pc = iter.first;
      auto target_id = iter.second.first;
      auto type = iter.second.second;
      auto *inst = inst_map[pc];
      auto *target_block = block_map[target_id];
      block->targets.push_back(new Target(inst, target_block, (TargetType)(type)));
    }
  }

  return true;
}

}  // namespace CudaParse
