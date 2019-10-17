#ifndef _CUDA_INSTRUCTION_ANALYZER_H_
#define _CUDA_INSTRUCTION_ANALYZER_H_

#include <map>
#include <vector>
#include <string>

#include "Instruction.hpp"

namespace CudaParse {

class Function;

struct InstructionStat {
  std::string op;
  int pc;
  int predicate;  // P0-P6
  int dst;  // R0-R255: only records normal registers
  std::vector<int> srcs;  // R0-R255, only records normal registers

  explicit InstructionStat(const Instruction &inst);

  InstructionStat(const std::string &op,
    unsigned int pc, int predicate, int dst, std::vector<int> &srcs) :
    op(op), pc(pc), predicate(predicate), dst(dst),
    srcs(srcs) {}

  InstructionStat() {}

  bool operator < (InstructionStat &other) const {
    return this->pc < other.pc;
  }
};

typedef std::vector<InstructionStat> InstructionStats;


struct BlockStat {
  int id;
  std::vector<int> targets;
  InstructionStats inst_stats;

  BlockStat(int id) : id(id) {}
};

typedef std::vector<BlockStat> BlockStats;


struct FunctionStat {
  int id;
  BlockStats block_stats; 

  FunctionStat(int id) : id(id) {}
};

typedef std::vector<FunctionStat> FunctionStats;


#if 0
struct InstructionMetrics {
  std::map<std::string, int> op;
  FunctionStats function_stats;

  void flat(std::vector<InstructionStat> &inst_stats) {
    for (auto &function_stat : function_stats) {
      for (auto &block_stat : function_stat.block_stats) {
        for (auto &inst_stat : block_stat.inst_stats) {
          inst_stats.emplace_back(inst_stat);
        }
      }
    }

    std::sort(inst_stats.begin(), inst_stats.end());
  }
};
#endif


template <InstructionTypes inst_type>
void analyze_instruction(const Instruction &inst, std::string &op);


class InstructionAnalyzer {
 public:
  InstructionAnalyzer(const std::string &file_path) : _file_path(file_path) {}
 
  void analyze(const std::vector<Function *> &functions);

  bool dump();
  
  bool read();

  FunctionStats &function_stats() {
    return _function_stats;
  }
 
 private:
  std::string _file_path;
  FunctionStats _function_stats;
};

}  // namespace CudaParse

#endif
