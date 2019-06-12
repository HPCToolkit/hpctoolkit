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


class InstructionAnalyzer {
 public:
  InstructionAnalyzer() {}
 
  void analyze(const std::vector<Function *> &functions, InstructionMetrics &instruction_metrics);

  bool dump(const std::string &file_path, InstructionMetrics &instruction_metrics);

  bool read(const std::string &file_path, InstructionMetrics &instruction_metrics);

 private:
  void analyze_memory(const Instruction &inst);

  void analyze_float(const Instruction &inst);

  void analyze_integer(const Instruction &inst);

  void analyze_special(const Instruction &inst);

  void analyze_tex(const Instruction &inst);

  void analyze_control(const Instruction &inst);

  void analyze_other(const Instruction &inst);
};

}  // namespace CudaParse

#endif
