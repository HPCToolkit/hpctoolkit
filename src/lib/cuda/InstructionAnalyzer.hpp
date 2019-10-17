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


class InstructionAnalyzer {
 public:
  InstructionAnalyzer() {}
 
  static void analyze_dot(const std::vector<Function *> &functions,
    FunctionStats &function_stats);

  static void flat(const FunctionStats &function_stats,
    std::vector<InstructionStat> &inst_stats);

  static bool dump(const std::string &file_path, const FunctionStats &function_stats);
  
  static bool read(const std::string &file_path, FunctionStats &Function_stats);
};

}  // namespace CudaParse

#endif
