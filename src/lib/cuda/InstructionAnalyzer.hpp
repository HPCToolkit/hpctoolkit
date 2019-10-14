#ifndef _CUDA_INSTRUCTION_ANALYZER_H_
#define _CUDA_INSTRUCTION_ANALYZER_H_

#include <map>
#include <vector>
#include <string>

#include "Instruction.hpp"

namespace CudaParse {

class Function;

struct InstructionStat {
  unsigned int pc;
  std::map<int, int> stat; 
  int predicate;  // P0-P6
  int dst;  // R0-R255: only records normal registers
  std::vector<int> srcs;  // R0-R255, only records normal registers

  explicit InstructionStat(unsigned int pc): pc(pc) {}

  InstructionStat() {}

  bool operator < (InstructionStat &other) const {
    return this->pc < other.pc;
  }
};

typedef std::vector<InstructionStat> InstructionStats;


struct InstructionMetrics {
  std::map<std::string, int> metric_names;
  InstructionStats inst_stats;
};


template <InstructionTypes inst_type>
void analyze_instruction(const Instruction &inst, std::string &metric_names);


class InstructionAnalyzer {
 public:
  InstructionAnalyzer();
 
  void analyze(const std::vector<Function *> &functions, InstructionMetrics &instruction_metrics);

  static bool dump(const std::string &file_path, InstructionMetrics &instruction_metrics, bool sparse = false);
  
  static bool read(const std::string &file_path, CudaParse::InstructionMetrics &metrics, bool sparse = false);
 
 private:
  void (*_dispatcher[INS_TYPE_COUNT])(const Instruction &inst, std::string &metric_names);
};

}  // namespace CudaParse

#endif
