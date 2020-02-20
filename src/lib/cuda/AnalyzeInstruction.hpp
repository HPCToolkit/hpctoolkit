#ifndef _CUDA_ANALYZE_INSTRUCTION_H_
#define _CUDA_ANALYZE_INSTRUCTION_H_

#include <map>
#include <vector>
#include <string>

#include <CodeObject.h>

namespace CudaParse {

class Function;
class Instruction;

struct InstructionStat {
  std::string op;
  unsigned int pc;
  int predicate;  // P0-P6
  std::vector<int> dsts;  // R0-R255: only records normal registers
  std::vector<int> srcs;  // R0-R255, only records normal registers
  std::map<int, std::vector<int> > assign_pcs;

  InstructionStat() {}

  explicit InstructionStat(const Instruction *inst);

  InstructionStat(const std::string &op,
    unsigned int pc, int predicate, std::vector<int> &dsts, std::vector<int> &srcs) :
    op(op), pc(pc), predicate(predicate), dsts(dsts),
    srcs(srcs) {}

  InstructionStat(const std::string &op,
    unsigned int pc, int predicate, std::vector<int> &dsts, std::vector<int> &srcs,
    std::map<int, std::vector<int> > &assign_pcs) :
    op(op), pc(pc), predicate(predicate), dsts(dsts),
    srcs(srcs), assign_pcs(assign_pcs) {}

  bool operator < (const InstructionStat &other) const {
    return this->pc < other.pc;
  }
};

void relocateCudaInstructionStats(std::vector<Function *> &functions);

void flatCudaInstructionStats(const std::vector<Function *> &functions,
  std::vector<InstructionStat *> &inst_stats);

void sliceCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions);

void processLivenessCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions);

bool dumpCudaInstructions(const std::string &file_path, const std::vector<Function *> &functions);

bool readCudaInstructions(const std::string &file_path, std::vector<Function *> &Function_stats);


}  // namespace CudaParse

#endif
