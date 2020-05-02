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
  enum PredicateFlag {
    PREDICATE_NONE = 0,
    PREDICATE_TRUE = 1,
    PREDICATE_FALSE = 2,
  };

  static int const WAIT_BITS = 6;
  static int const BARRIER_NONE = 7;

  struct Control {
    uint8_t reuse;
    uint8_t wait;
    uint8_t read;
    uint8_t write;
    uint8_t yield;
    uint8_t stall;

    Control() : reuse(0), wait(0), read(7), write(7), yield(0), stall(1) {}
  };

  std::string op;
  int pc;
  int predicate;  // P0-P6
  PredicateFlag predicate_flag;
  std::vector<int> predicate_assign_pcs;
  std::vector<int> dsts;  // R0-R255: only records normal registers
  std::vector<int> srcs;  // R0-R255: only records normal registers
  std::vector<int> pdsts;  // P0-P6: only records predicate registers
  std::vector<int> psrcs;  // P0-P6: only records predicate registers
  std::vector<int> bdsts;  // B1-B6: only records barriers
  std::vector<int> bsrcs;  // B1-B6: only records barriers
  std::map<int, std::vector<int> > assign_pcs;
  std::map<int, std::vector<int> > passign_pcs;
  std::map<int, std::vector<int> > bassign_pcs;
  Control control;

  InstructionStat() {}

  explicit InstructionStat(const Instruction *inst);

  InstructionStat(const std::string &op,
    int pc, int predicate, PredicateFlag predicate_flag,
    std::vector<int> &predicate_assign_pcs,
    std::vector<int> &dsts, std::vector<int> &srcs,
    std::vector<int> &pdsts, std::vector<int> &psrcs,
    std::vector<int> &bdsts, std::vector<int> &bsrcs) :
    op(op), pc(pc), predicate(predicate), predicate_flag(predicate_flag),
    predicate_assign_pcs(predicate_assign_pcs),
    dsts(dsts), srcs(srcs), pdsts(pdsts), psrcs(psrcs),
    bdsts(bdsts), bsrcs(bsrcs) {}

  InstructionStat(const std::string &op,
    int pc, int predicate, PredicateFlag predicate_flag,
    std::vector<int> &predicate_assign_pcs,
    std::vector<int> &dsts, std::vector<int> &srcs,
    std::vector<int> &pdsts, std::vector<int> &psrcs,
    std::vector<int> &bdsts, std::vector<int> &bsrcs,
    std::map<int, std::vector<int> > &assign_pcs,
    std::map<int, std::vector<int> > &passign_pcs,
    std::map<int, std::vector<int> > &bassign_pcs) :
    op(op), pc(pc), predicate(predicate), predicate_flag(predicate_flag),
    predicate_assign_pcs(predicate_assign_pcs),
    dsts(dsts), srcs(srcs), pdsts(pdsts), psrcs(psrcs),
    bdsts(bdsts), bsrcs(bsrcs),
    assign_pcs(assign_pcs), passign_pcs(passign_pcs), bassign_pcs(bassign_pcs) {}

  InstructionStat(const std::string &op,
    int pc, int predicate, PredicateFlag predicate_flag,
    std::vector<int> &predicate_assign_pcs,
    std::vector<int> &dsts, std::vector<int> &srcs,
    std::vector<int> &pdsts, std::vector<int> &psrcs,
    std::vector<int> &bdsts, std::vector<int> &bsrcs,
    std::map<int, std::vector<int> > &assign_pcs,
    std::map<int, std::vector<int> > &passign_pcs,
    std::map<int, std::vector<int> > &bassign_pcs,
    Control &control) :
    op(op), pc(pc), predicate(predicate), predicate_flag(predicate_flag),
    predicate_assign_pcs(predicate_assign_pcs),
    dsts(dsts), srcs(srcs), pdsts(pdsts), psrcs(psrcs), 
    bdsts(bdsts), bsrcs(bsrcs),
    assign_pcs(assign_pcs), passign_pcs(passign_pcs), bassign_pcs(bassign_pcs),
    control(control) {}

  bool operator < (const InstructionStat &other) const {
    return this->pc < other.pc;
  }

  bool find_src_reg(int reg) {
    return std::find(srcs.begin(), srcs.end(), reg) != srcs.end();
  }

  bool find_src_pred_reg(int pred_reg) {
    return std::find(psrcs.begin(), psrcs.end(), pred_reg) != psrcs.end();
  }

  bool find_src_barrier(int barrier) {
    return std::find(bsrcs.begin(), bsrcs.end(), barrier) != bsrcs.end();
  }
};

void relocateCudaInstructionStats(std::vector<Function *> &functions);

void flatCudaInstructionStats(const std::vector<Function *> &functions,
  std::vector<InstructionStat *> &inst_stats);

void controlCudaInstructions(const char *cubin, std::vector<Function *> &functions);

void sliceCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions);

void processLivenessCudaInstructions(const Dyninst::ParseAPI::CodeObject::funclist &func_set,
  std::vector<Function *> &functions);

bool dumpCudaInstructions(const std::string &file_path, const std::vector<Function *> &functions);

bool readCudaInstructions(const std::string &file_path, std::vector<Function *> &Function_stats);


}  // namespace CudaParse

#endif
