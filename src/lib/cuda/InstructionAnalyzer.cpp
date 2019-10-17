#include "InstructionAnalyzer.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

#include "DotCFG.hpp"

#define INSTRUCTION_ANALYZER_DEBUG 1

namespace CudaParse {

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


InstructionStat::InstructionStat(const Instruction &inst) {
  std::string op;

#define INST_DISPATCHER(TYPE, VALUE)                \
  case TYPE:                                        \
    {                                               \
      analyze_instruction<TYPE>(inst, op); \
      break;                                        \
    }

  switch (inst.type) {
    FORALL_INS_TYPES(INST_DISPATCHER)
    default:
      break;
    }

#undef INST_DISPATCHER

  this->op = op;
  this->pc = inst.offset;
  // -1 means no value
  this->predicate = -1;
  this->dst = -1;

  if (inst.predicate.size() != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << inst.predicate << " ";
    }

    auto pos = inst.predicate.find("P");
    if (pos != std::string::npos) {
      this->predicate = convert_reg(inst.predicate, pos + 1);
    }
  }

  if (inst.operands.size() != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << inst.operands[0] << " ";
    }

    auto pos = inst.operands[0].find("R");
    if (pos != std::string::npos) {
      this->dst = convert_reg(inst.operands[0], pos + 1);
    }

    for (size_t i = 1; i < inst.operands.size(); ++i) {
      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << inst.operands[i] << " ";
      }

      pos = inst.operands[i].find("R");
      if (pos != std::string::npos) {
        this->srcs.push_back(convert_reg(inst.operands[i], pos + 1));
      }
    }
  }

  if (INSTRUCTION_ANALYZER_DEBUG) {
    std::cout << std::endl;
  }
}


void InstructionAnalyzer::analyze(const std::vector<Function *> &functions) {
  std::string op;
  for (auto *function : functions) {
    FunctionStat function_stat(function->id);
    for (auto *block : function->blocks) {
      BlockStat block_stat(block->id);
      for (auto *inst : block->insts) {
        InstructionStat inst_stat(*inst);

        if (INSTRUCTION_ANALYZER_DEBUG) {
          std::cout << inst->to_string() << "  ----  " << inst_stat.op << std::endl;
        }

        block_stat.inst_stats.emplace_back(inst_stat);
      }
      function_stat.block_stats.emplace_back(block_stat);
    }
    _function_stats.emplace_back(function_stat);
  }

  if (INSTRUCTION_ANALYZER_DEBUG) {
    std::cout << "Finish analysis" << std::endl;
  }
}


bool InstructionAnalyzer::dump() {
  boost::property_tree::ptree root;

  for (auto &function_stat : _function_stats) {
    boost::property_tree::ptree function;
    boost::property_tree::ptree blocks;
    function.put("id", function_stat.id);

    for (auto &block_stat : function_stat.block_stats) {
      boost::property_tree::ptree block;
      boost::property_tree::ptree insts;
      block.put("id", block_stat.id);

      for (auto &inst_stat : block_stat.inst_stats) {
        boost::property_tree::ptree inst;
        boost::property_tree::ptree srcs;
        inst.put("pc", inst_stat.pc);
        inst.put("op", inst_stat.op);
        if (inst_stat.predicate != -1) {
          inst.put("pred", inst_stat.predicate);
        } else {
          inst.put("pred", "");
        }
        if (inst_stat.dst != -1) {
          inst.put("dst", inst_stat.dst);
        } else {
          inst.put("dst", "");
        }

        for (auto src : inst_stat.srcs) {
          boost::property_tree::ptree t;
          t.put("", src);
          srcs.push_back(std::make_pair("", t));
        }
        
        inst.add_child("srcs", srcs);
        insts.push_back(std::make_pair("", inst));
      }

      block.add_child("insts", insts);
      blocks.push_back(std::make_pair("", block));
    }

    function.add_child("blocks", blocks);
    root.push_back(std::make_pair("", function));
  }

  if (INSTRUCTION_ANALYZER_DEBUG) {
    boost::property_tree::write_json(_file_path, root, std::locale(), true);
  } else {
    boost::property_tree::write_json(_file_path, root, std::locale(), false);
  }

  return true;
}


bool InstructionAnalyzer::read() {
  boost::property_tree::ptree root;

  boost::property_tree::read_json(_file_path, root);

  for (auto &function_iter : root) {
    int id = function_iter.second.get<int>("id", 0);
    FunctionStat function_stat(id);

    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Function id: " << id << std::endl;
    }

    auto &blocks = function_iter.second.get_child("blocks");
    for (auto &block_iter : blocks) {
      int id = block_iter.second.get<int>("id", 0);
      BlockStat block_stat(id);

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << "Block id: " << id << std::endl;
      }

      auto &insts = block_iter.second.get_child("insts");
      for (auto &inst_iter : insts) {
        int pc = inst_iter.second.get<int>("pc", 0);
        std::string op = inst_iter.second.get<std::string>("op", "");
        int pred = inst_iter.second.get<int>("pred", -1);
        int dst = inst_iter.second.get<int>("dst", -1);

        std::vector<int> srcs; 
        auto &src_iters = inst_iter.second.get_child("srcs");
        for (auto &iter : src_iters) {
          srcs.push_back(boost::lexical_cast<float>(iter.second.data()));
        }

        InstructionStat inst_stat(op, pc, pred, dst, srcs);

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

        block_stat.inst_stats.emplace_back(inst_stat);
      
      }

      function_stat.block_stats.emplace_back(block_stat);
    }

    _function_stats.emplace_back(function_stat);
  }

  return true;
}

}  // namespace CudaParse
