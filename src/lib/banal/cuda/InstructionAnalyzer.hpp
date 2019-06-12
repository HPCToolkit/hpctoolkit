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

  explicit InstructionStat(int pc): pc(pc) {}

  InstructionStat() {}
};

typedef std::vector<InstructionStat> InstructionStats;


struct InstructionMetrics {
  std::map<std::string, int> metric_names;
  InstructionStats inst_stats;
};


template <InstructionTypes inst_type>
void analyze_instruction(const Instruction &inst, std::vector<std::string> &metric_names);


class InstructionAnalyzer {
 public:
  InstructionAnalyzer();
 
  void analyze(const std::vector<Function *> &functions, InstructionMetrics &instruction_metrics);

  bool dump(const std::string &file_path, InstructionMetrics &instruction_metrics, bool sparse = false);

  bool read(const std::string &file_path, InstructionMetrics &instruction_metrics, bool sparse = false);
 
 private:
  void (*_dispatcher[INS_TYPE_COUNT])(const Instruction &inst, std::vector<std::string> &metric_names);
};

}  // namespace CudaParse

#endif
