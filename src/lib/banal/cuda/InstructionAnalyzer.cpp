#include "InstructionAnalyzer.hpp"

#include <fstream>

#define INSTRUCTION_ANALYZER_DEBUG 0

namespace CudaParse {

template <>
void analyze_instruction<INS_TYPE_MEMORY>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


template <>
void analyze_instruction<INS_TYPE_FLOAT>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


template <>
void analyze_instruction<INS_TYPE_INTEGER>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


template <>
void analyze_instruction<INS_TYPE_SPECIAL>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


template <>
void analyze_instruction<INS_TYPE_TEXTRUE>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


template <>
void analyze_instruction<INS_TYPE_CONTROL>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


template <>
void analyze_instruction<INS_TYPE_OTHER>(const Instruction &inst,
  std::vector<std::string> &metric_names) {
}


InstructionAnalyzer::InstructionAnalyzer() {
#define INIT_DISPATCHER(TYPE, VALUE)              \
  _dispatcher[TYPE] = &analyze_instruction<TYPE>; \

  FORALL_INS_TYPES(INIT_DISPATCHER)

#undef INIT_DISPATCHER
}


void InstructionAnalyzer::analyze(const std::vector<Function *> &functions,
  InstructionMetrics &metrics) {
  std::vector<std::string> metric_names;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        metric_names.clear();
        _dispatcher[inst->type](*inst, metric_names);

        InstructionStat inst_stat(inst->offset);
        for (auto metric_name : metric_names) {
          int metric_id = 0;
          if (metrics.metric_names.find(metric_name) == metrics.metric_names.end()) {
            metric_id = metrics.metric_names.size();
            metrics.metric_names[metric_name] = metric_id;
          } else {
            metric_id = metrics.metric_names[metric_name];
          }
          inst_stat.stat[metric_id] += 1;
        }
      }
    }
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

  ofs << "<inst stats>" << std::endl;

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


bool InstructionAnalyzer::read(const std::string &file_path, InstructionMetrics &metrics, bool sparse) {
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
    while (std::getline(ifs, buf, sep)) {
      // (mn,id)#
      // 01234567
      //    p  s
      auto pos = buf.find(",");
      if (pos != std::string::npos) {
        auto metric_name = buf.substr(1, pos - 1);
        auto metric_id = buf.substr(pos + 1, buf.size() - pos - 2);
        metrics.metric_names[metric_name] = std::stoi(metric_id);
      }
    }
  } else {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error reading metrics " << file_path << std::endl;
    }
    return false;
  }
  
  if (std::getline(ifs, buf) && buf == "<inst stats>") {
    std::string cur_buf;
    while (std::getline(ifs, buf, sep)) {
      bool first = true;
      InstructionStat inst_stat;
      std::istringstream iss(buf);
      // (pc,id:mc,...)#
      while (std::getline(iss, cur_buf, ',')) {
        if (first) {
          // (0x234,
          // 0123456
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
            auto metric_count = cur_buf.substr(pos + 1, cur_buf.size() - pos - 2);
            inst_stat.stat[std::stoi(metric_id)] = std::stoi(metric_count);
          }
        }
      }
      metrics.inst_stats.emplace_back(inst_stat);
    }
  } else {
    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "Error reading stats " << file_path << std::endl;
    }
    return false;
  }

  return true;
}

}
