#include "InstructionAnalyzer.hpp"


namespace CudaParse {

template <>
void analyze_instruction<INS_TYPE_MEMORY>(const Instruction &inst, std::string &metric) {
}


template <>
void analyze_instruction<INS_TYPE_FLOAT>(const Instruction &inst, std::string &metric) {
}


template <>
void analyze_instruction<INS_TYPE_INTEGER>(const Instruction &inst, std::string &metric) {
}


template <>
void analyze_instruction<INS_TYPE_SPECIAL>(const Instruction &inst, std::string &metric) {
}


template <>
void analyze_instruction<INS_TYPE_TEXTRUE>(const Instruction &inst, std::string &metric) {
}


template <>
void analyze_instruction<INS_TYPE_CONTROL>(const Instruction &inst, std::string &metric) {
}


template <>
void analyze_instruction<INS_TYPE_OTHER>(const Instruction &inst, std::string &metric) {
}


InstructionAnalyzer::InstructionAnalyzer() {
#define INIT_DISPATCHER(TYPE, VALUE)              \
  _dispatcher[TYPE] = &analyze_instruction<TYPE>; \

  FORALL_INS_TYPES(INIT_DISPATCHER)

#undef INIT_DISPATCHER
}


void InstructionAnalyzer::analyze(const std::vector<Function *> &functions,
  InstructionMetrics &metrics) {
  std::string metric;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        analyze_instruction<INS_TYPE_MEMORY>(*inst, metric);
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
