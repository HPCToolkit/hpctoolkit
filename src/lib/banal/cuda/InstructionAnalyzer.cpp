#include "InstructionAnalyzer.hpp"

#include <fstream>

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

  if (opcode.find("SYNC") != std::string::npos) {
    type = ".SYNC";
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


InstructionAnalyzer::InstructionAnalyzer() {
#define INIT_DISPATCHER(TYPE, VALUE)              \
  _dispatcher[TYPE] = &analyze_instruction<TYPE>; \

  FORALL_INS_TYPES(INIT_DISPATCHER)

#undef INIT_DISPATCHER
}


void InstructionAnalyzer::analyze(const std::vector<Function *> &functions,
  InstructionMetrics &metrics) {
  std::string metric_name;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        _dispatcher[inst->type](*inst, metric_name);

        if (INSTRUCTION_ANALYZER_DEBUG) {
          std::cout << inst->to_string() << "  ----  " << metric_name << std::endl;
        }

        InstructionStat inst_stat(inst->offset);
        int metric_id = 0;
        if (metrics.metric_names.find(metric_name) == metrics.metric_names.end()) {
          metric_id = metrics.metric_names.size();
          metrics.metric_names[metric_name] = metric_id;
        } else {
          metric_id = metrics.metric_names[metric_name];
        }
        inst_stat.stat[metric_id] += 1;
        metrics.inst_stats.emplace_back(inst_stat);
      }
    }
  }
  if (INSTRUCTION_ANALYZER_DEBUG) {
    std::cout << "Finish analysis" << std::endl;
  }
}


bool InstructionAnalyzer::dump(const std::string &file_path, InstructionMetrics &metrics, bool sparse) {
  std::ofstream ofs(file_path, std::ofstream::out);
  
  if ((ofs.rdstate() & std::ofstream::failbit) != 0) {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error opening " << file_path << std::endl;
    }
    return false;
  }

  if (metrics.metric_names.size() == 0) {
    // no metrics
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error no metrics " << file_path << std::endl;
    }
    return false;
  }

  const char sep = sparse ? '\n' : '#'; 

  ofs << "<metric names>" << std::endl;

  // (metric_name,id)#
  for (auto it = metrics.metric_names.begin(); it != metrics.metric_names.end(); ++it) {
    ofs << "(" << it->first << "," << it->second << ")" << sep;
  }

  ofs << std::endl << "<inst stats>" << std::endl;

  // (pc,metric_id:metric_count, ...)#
  for (auto &inst_stat : metrics.inst_stats) {
    ofs << "(" << inst_stat.pc << ",";
    for (auto it = inst_stat.stat.begin(); it != inst_stat.stat.end(); ++it) {
      ofs << it->first << ":" << it->second << ",";
    }
    ofs << ")" << sep;
  }

  ofs.close();
  return true;
}

}  // namespace CudaParse
