#include "InstructionAnalyzer.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "DotCFG.hpp"

#define INSTRUCTION_ANALYZER_DEBUG 0

namespace CudaParse {

template <>
void analyze_instruction<INS_TYPE_MEMORY>(const Instruction &inst, std::string &metric_name) {
  metric_name = "MEMORY";

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

  metric_name += ldst + scope + width;
}


template <>
void analyze_instruction<INS_TYPE_FLOAT>(const Instruction &inst, std::string &metric_name) {
  metric_name = "FLOAT";

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

  metric_name += type + width;
}


template <>
void analyze_instruction<INS_TYPE_INTEGER>(const Instruction &inst, std::string &metric_name) {
  metric_name = "INTEGER";

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

  metric_name += type;
}


template <>
void analyze_instruction<INS_TYPE_TEXTRUE>(const Instruction &inst, std::string &metric_name) {
  metric_name = "TEXTURE";
}


template <>
void analyze_instruction<INS_TYPE_CONTROL>(const Instruction &inst, std::string &metric_name) {
  metric_name = "CONTROL";

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

  metric_name += type;
}


template <>
void analyze_instruction<INS_TYPE_MISC>(const Instruction &inst, std::string &metric_name) {
  metric_name = "MISC";

  std::string type;

  const std::string &opcode = inst.opcode;

  if (opcode.find("I2") != std::string::npos ||
    opcode.find("F2") != std::string::npos || opcode == "FRND") {
    type = ".CONVERT";
  } else {
    type = ".OTHER";
  }

  metric_name += type;
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
  std::string metric_name;

#define INST_DISPATCHER(TYPE, VALUE)                \
  case TYPE:                                        \
    {                                               \
      analyze_instruction<TYPE>(inst, metric_name); \
      break;                                        \
    }

  switch (inst.type) {
    FORALL_INS_TYPES(INST_DISPATCHER)
    default:
      break;
    }

#undef INST_DISPATCHER

  this->metric_name = metric_name;
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
  std::string metric_name;
  for (auto *function : functions) {
    FunctionStat function_stat;
    function_stat.id = function->id;
    for (auto *block : function->blocks) {
      BlockStat block_stat;
      block_stat.id = block->id;
      for (auto *inst : block->insts) {
        InstructionStat inst_stat(*inst);

        if (INSTRUCTION_ANALYZER_DEBUG) {
          std::cout << inst->to_string() << "  ----  " << inst_stat.metric_name << std::endl;
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
        inst.put("op", inst_stat.metric_name);
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
#if 0
  std::ifstream ifs(file_path, std::ifstream::in);
  if ((ifs.rdstate() & std::ifstream::failbit) != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error opening " << file_path << std::endl;
    }
    return false;
  }

  const char sep = sparse ? '\n' : '#'; 

  std::string buf;
  if (std::getline(ifs, buf) && buf == "<metric names>") {
    if (std::getline(ifs, buf)) {
      std::istringstream iss(buf);
      std::string cur_buf;
      while (std::getline(iss, cur_buf, sep)) {
        // (mn,id)#
        // 01234567
        //    p  s
        auto pos = cur_buf.find(",");
        if (pos != std::string::npos) {
          auto metric_name = cur_buf.substr(1, pos - 1);
          auto metric_id = cur_buf.substr(pos + 1, cur_buf.size() - pos - 2);
          // Add a MIX prefix
          metrics.metric_names["MIX:" + metric_name] = std::stoi(metric_id);

          if (INSTRUCTION_ANALYZER_DEBUG) {
            std::cout << "metric_name: " << metric_name << ", metric_id: " << metric_id << std::endl;
          }
        }
      }
    }
  } else {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error reading metrics " << file_path << std::endl;
    }
    return false;
  }
  
  if (std::getline(ifs, buf) && buf == "<inst stats>") {
    if (std::getline(ifs, buf)) {
      std::istringstream iss(buf);
      std::string cur_buf;
      while (std::getline(iss, cur_buf, sep)) {
        bool first = true;
        CudaParse::InstructionStat inst_stat;
        std::istringstream isss(cur_buf);
        // (pc,id:mc,...)#
        while (std::getline(isss, cur_buf, ',')) {
          if (cur_buf == ")") {
            break;
          }
          if (first) {
            // (111,
            // 01234
            std::string tmp = cur_buf.substr(1);
            inst_stat.pc = std::stoi(tmp);
            first = false;
          } else {
            // id:mc,
            // 012345
            //   p  s
            auto pos = cur_buf.find(":");
            if (pos != std::string::npos) {
              auto metric_id = cur_buf.substr(0, pos);
              auto metric_count = cur_buf.substr(pos + 1, cur_buf.size() - pos - 1);
              inst_stat.stat[std::stoi(metric_id)] = std::stoi(metric_count);

              if (INSTRUCTION_ANALYZER_DEBUG) {
                std::cout << "pc 0x" << std::hex << inst_stat.pc << std::dec <<
                  " metric_id: " << metric_id << ", metric_count: " << metric_count << std::endl;
              }
            }
          }
        }
        // FIXME(Keren)
        std::vector<CudaParse::InstructionStat> stats;
        stats.emplace_back(inst_stat);
        metrics.inst_stats.emplace_back(stats);
      }
    }
  } else {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error reading stats " << file_path << std::endl;
    }
    return false;
  }

#endif
  return true;
}

}  // namespace CudaParse
