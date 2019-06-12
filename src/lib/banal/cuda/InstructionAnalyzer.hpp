#ifndef _INSTRUCTION_ANALYZER_H_
#define _INSTRUCTION_ANALYZER_H_

#include <map>
#include <vector>
#include <string>

#include "Instruction.hpp"
#include "DotCFG.hpp"

namespace CudaParse {

struct InstructionStat {
  int pc;
  std::map<int, int> stat; 
};

typedef std::vector<InstructionStat> InstructionStats;


struct InstructionMetrics {
  std::vector<std::string> metrics;
  InstructionStats inst_stats;
};


template <InstructionTypes inst_type>
void analyze_instruction(const Instruction &inst, std::string &metric);


class InstructionAnalyzer {
 public:
  InstructionAnalyzer();
 
  void analyze(const std::vector<Function *> &functions, InstructionMetrics &instruction_metrics);

  bool dump(const std::string &file_path, InstructionMetrics &instruction_metrics);

  bool read(const std::string &file_path, InstructionMetrics &instruction_metrics);
 
 private:
  void (*_dispatcher[INS_TYPE_COUNT])(const Instruction &inst, std::string &metric);
};

}  // namespace CudaParse

#endif
