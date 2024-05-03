// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// local includes
//******************************************************************************

#include "GPUCFG.hpp"


//******************************************************************************
// namespace imports
//******************************************************************************

using namespace GPUParse;



//***************************************************************************
// private operations
//***************************************************************************

static void
dumpBlockInstructions(
  std::vector<GPUParse::Inst*> &insts,
  GPUInstDumper &idump
)
{
  if (insts.size() > 0) {
    std::cout << "    instructions:\n";
    std::cout << std::hex;
    for (auto *inst : insts) {
      idump.dump(inst);
    }
    std::cout << std::dec;
  }
}


static void
dumpBlockTargets
(
 std::vector<GPUParse::Target*> &targets
)
{
  std::cout << "    targets: {";
  if (targets.size() > 0) {
    const char *separator = "";
    std::cout << std::hex;
    for (auto *target : targets) {
      std::cout << separator << target->block->startAddress();
      separator = ", ";
    }
    std::cout << std::dec;
  }
  std::cout << "}" << std::endl;
}


static void
dumpBlock(
 GPUParse::Block *block,
 GPUInstDumper &idump
)
{
  std::cout << "\n  block [" << std::hex
            << block->startAddress() << ", "
            << block->endAddress()
            << std::dec << ")"
            << std::endl;
  dumpBlockInstructions(block->insts, idump);
  dumpBlockTargets(block->targets);
}



//***************************************************************************
// interface operations
//***************************************************************************

void GPUInstDumper::dump(GPUParse::Inst* inst)
{
  // force vtable generation for this virtual function
}


void
GPUParse::dumpFunction
(
  GPUParse::Function &function,
  GPUInstDumper &idump
)
{
  std::cout << "-----------------------------------------------------------------\n"
            << "Function " << function.name << "\n"
            << "-----------------------------------------------------------------\n";
  for (auto *block : function.blocks) {
    dumpBlock(block, idump);
  }
  std::cout << "\n\n";
}
