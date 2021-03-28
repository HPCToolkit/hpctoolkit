// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *


//***************************************************************************

#ifdef ENABLE_IGC

//******************************************************************************
// system includes
//******************************************************************************

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>

#include <Symtab.h>
#include <CodeSource.h>
#include <CodeObject.h>

#include <iga/kv.hpp>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/binutils/ElfHelper.hpp>
#include <lib/support/diagnostics.h>

#include "DotCFG.hpp"
#include "GPUCFGFactory.hpp"
#include "GPUFunction.hpp"
#include "GPUBlock.hpp"
#include "GPUCodeSource.hpp"
#include "ReadIntelCFG.hpp"

//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#define MAX_STR_SIZE 1024
#define INTEL_GPU_DEBUG_SECTION_NAME "Intel(R) OpenCL Device Debug"

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;

static void 
addCustomFunctionObject
(
 const std::string &func_obj_name,
 Symtab *symtab
)
{
  Region *reg = NULL;
  bool status = symtab->findRegion(reg, ".text");
  assert(status == true);

  unsigned long reg_size = reg->getMemSize();
  Symbol *custom_symbol = new Symbol(
      func_obj_name, 
      SymtabAPI::Symbol::ST_FUNCTION, // SymbolType
      Symbol::SL_LOCAL, //SymbolLinkage
      SymtabAPI::Symbol::SV_DEFAULT, //SymbolVisibility
      0, //Offset,
      NULL, //Module *module 
      reg, //Region *r
      reg_size, //unsigned s
      false, //bool d
      false, //bool a
      -1, //int index
      -1, //int strindex
      false //bool cs
  );
  
  //adding the custom symbol into the symtab object
  status = symtab->addSymbol(custom_symbol); //(Symbol *newsym)
  assert(status == true);
}


static void
parseIntelCFG
(
 char *text_section,
 int text_section_size,
 GPUParse::Function &function
)
{
  KernelView kv(IGA_GEN9, text_section, text_section_size, iga::SWSB_ENCODE_MODE::SingleDistPipe);
  std::map<int, GPUParse::Block *> block_offset_map;

  int offset = 0;
  int size = 0;
  int block_id = 0;

  // Construct basic blocks
  while (offset < text_section_size) {
    auto *block = new GPUParse::Block(block_id, offset, function.name + "_" + std::to_string(block_id)); 
    block_id++;

    function.blocks.push_back(block);
    block_offset_map[offset] = block;

    size = kv.getInstSize(offset);
    auto *inst = new GPUParse::IntelInst(offset, size);
    block->insts.push_back(inst);

    while (!kv.isInstTarget(offset + size) && (offset + size < text_section_size)) {
      offset += size;  
      size = kv.getInstSize(offset);
      if (size == 0) {
        // this is a weird edge case, what to do?
        break;
      }

      inst = new GPUParse::IntelInst(offset, size);
      block->insts.push_back(inst);
    }

    if (kv.getOpcode(offset) == iga::Op::CALL || kv.getOpcode(offset) == iga::Op::CALLA) {
      inst->is_call = true;
    } else {
      inst->is_jump = true;
    }
    offset += size;
  }

  // Construct targets
  std::array<int, KV_MAX_TARGETS_PER_INSTRUCTION + 1> jump_targets;
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    auto *block = function.blocks[i];
    auto *inst = block->insts.back();
    size_t jump_targets_count = kv.getInstTargets(inst->offset, jump_targets.data());
    
    // Add fall through edge
    int next_block_start_offset = 0;
    if (i != function.blocks.size() - 1) {
      next_block_start_offset = function.blocks[i + 1]->insts.front()->offset;
    }
    jump_targets[jump_targets_count] = next_block_start_offset;

    for (size_t j = 0; j < jump_targets_count + 1; j++) {
      auto *target_block = block_offset_map.at(jump_targets[j]);
      // TODO(Aaron): call edge
      auto type = GPUParse::TargetType::DIRECT;
      // Jump
      bool added = false;
      for (auto *target : block->targets) {
        if (target->block == target_block) {
          added = true;
        }
      }
      if (!added) {
        block->targets.push_back(new GPUParse::Target(inst, target_block, type));
      }
    }
  }

  if (DEBUG) {
    for (auto *block : function.blocks) {
      std::cout << std::hex;
      std::cout << block->name << ": [" << block->insts.front()->offset << ", " << block->insts.back()->offset << "]" << std::endl;
      for (auto *target : block->targets) {
        std::cout << "\t" << block->name << "->" << target->block->name << std::endl;
      }
      std::cout << std::dec;
    }
  }
}


bool
readIntelCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 bool cfg_wanted,
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj
)
{
  // An Intel GPU binary for a kernel does not contain a function symbol for the kernel
  // in its symbol table. Without a function symbol in the symbol table, Dyninst will not
  // associate line map entries with addresses in the kernel. To cope with this defect of
  // binaries for Intel GPU kernels, we add a function symbol for the kernel to its Dyninst
  // symbol table.	
  auto function_name = elfFile->getGPUKernelName();
  addCustomFunctionObject(function_name, the_symtab); //adds a dummy function object

  if (cfg_wanted) {
    char *text_section = NULL;
    auto text_section_size = elfFile->getTextSection(&text_section);
    if (text_section_size == 0) {
      *code_src = new SymtabCodeSource(the_symtab);
      *code_obj = new CodeObject(*code_src, NULL, NULL, false, true);

      return false;
    }

    GPUParse::Function function(0, function_name);
    parseIntelCFG(text_section, text_section_size, function);
    std::vector<GPUParse::Function *> functions = {&function};

    CFGFactory *cfg_fact = new GPUCFGFactory(functions);
    *code_src = new GPUCodeSource(functions, the_symtab); 
    *code_obj = new CodeObject(*code_src, cfg_fact);
    (*code_obj)->parse();

    return true;
  }

  *code_src = new SymtabCodeSource(the_symtab);
  *code_obj = new CodeObject(*code_src, NULL, NULL, false, true);

  return false;
}

#endif // ENABLE_IGC
