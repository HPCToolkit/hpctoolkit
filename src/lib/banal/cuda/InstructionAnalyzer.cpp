#include "InstructionAnalyzer.hpp"


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


bool InstructionAnalyzer::dump(const std::string &file_path, InstructionMetrics &metrics) {
  return true;
}


bool InstructionAnalyzer::read(const std::string &file_path, InstructionMetrics &metrics) {
  return true;
}

}
