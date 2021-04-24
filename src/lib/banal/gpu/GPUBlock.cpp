#include "GPUBlock.hpp"
#include <Instruction.h>
#include <Operand.h>
#include <Operation_impl.h>
#include <Register.h>

#include <include/hpctoolkit-config.h>


//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0
#define MAX_INST_SIZE 32



//******************************************************************************

namespace Dyninst {
namespace ParseAPI {

static void
appendOperandstoInst
(
 GPUParse::Inst *inst,
 InstructionAPI::Instruction& instruction
)
{
  // Instruction predicate flags
    int pred = 0;// single pred register;
    if (inst->inst_stat->predicate_flag ==
        GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
      MachRegister r(pred | intel::PR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      // bool isRead, bool isWritten, bool isImplicit, bool trueP, bool falseP
      instruction.appendOperand(reg_ptr, true, false, false, true, false);
    } else if (inst->inst_stat->predicate_flag ==
               GPUParse::InstructionStat::PredicateFlag::PREDICATE_FALSE) {
      MachRegister r(pred | intel::PR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      // bool isRead, bool isWritten, bool isImplicit, bool trueP, bool falseP
      instruction.appendOperand(reg_ptr, true, false, false, false, true);
    }

#if DEBUG
    std::cout << "dst register: ";
#endif
    if (inst->inst_stat->dsts.size() == 0)
    {
#if DEBUG
      std::cout << 128;
#endif
      // Fake register
      MachRegister r(128 | intel::GPR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, false, true);
  } else {
    for (auto dst : inst->inst_stat->dsts) {
      if (dst != -1) {
#if DEBUG
        std::cout << dst << ", ";
#endif
        MachRegister r(dst | intel::GPR | Arch_intelGen9);
        InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
        instruction.appendOperand(reg_ptr, false, true);
      }
    }
  }
#if DEBUG
  std::cout << std::endl;
#endif

#if DEBUG
  std::cout << "src register: ";
#endif
  for (auto src : inst->inst_stat->srcs) {
    if (src != -1) {
#if DEBUG
      std::cout << src << ", ";
#endif
      MachRegister r(src | intel::GPR | Arch_intelGen9);
      InstructionAPI::RegisterAST::Ptr reg_ptr(new InstructionAPI::RegisterAST(r));
      instruction.appendOperand(reg_ptr, true, false);
    }
  }

#if DEBUG
  std::cout << std::endl;
#endif
}


GPUBlock::GPUBlock(CodeObject * o, CodeRegion * r,
  Address start, Address end, Address last, 
  std::vector<GPUParse::Inst*> insts,
  Dyninst::Architecture arch) :
  Block(o, r, start, end, last), _insts(std::move(insts)), _arch(arch) {}

#if 0
Address GPUBlock::last() const {
  return this->_insts.back()->offset;
}
#endif


void GPUBlock::getInsns(Insns &insns) const {
  unsigned char dummy_inst[MAX_INST_SIZE];
#ifdef DYNINST_SUPPORTS_INTEL_GPU
  entryID entry_id = _entry_ids_max_;
  
  if (_arch == Arch_cuda) {
    entry_id = cuda_op_general;
  } else if (_arch == Arch_intelGen9) {
    entry_id = intel_gpu_op_general;
  }

  // Don't construct CFG if Dyninst does not support this GPU arch
  if (entry_id == _entry_ids_max_) {
    return;
  }
#endif  // DYNINST_SUPPORTS_INTEL_GPU

  for (auto &ins : _insts) {

    auto offset = ins->offset;
    auto size = ins->size;

#ifdef DYNINST_SUPPORTS_INTEL_GPU
    InstructionAPI::Operation op(entry_id, "", _arch);
#else
    // Dyninst 10.2.0 does not have cuda_op_general
    InstructionAPI::Operation op;
#endif

    std::cout << "offset3: " << offset << ", size3: " << size << std::endl;
    InstructionAPI::Instruction inst(op, size, dummy_inst, _arch);
    if (latency_blame_enabled) {
      appendOperandstoInst(ins, inst);
    }
    insns.emplace(offset, inst);
  }
}


void GPUBlock::enable_latency_blame() {
  latency_blame_enabled = true;
}
}
}
