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
// Copyright ((c)) 2002-2020, Rice University
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

#include "../cuda/DotCFG.hpp"
#include "IntelCFGFactory.hpp"
#include "IntelFunction.hpp"
#include "IntelBlock.hpp"
#include "IntelCodeSource.hpp"
#include "ReadIntelCFG.hpp"

//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 1

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


static std::string
getFileNameFromAbsolutePath(const std::string &str) {
  // TODO(Aaron): you can just find the last "/" and grab "/" to the end
  std::vector<std::string> tokens; 
  std::stringstream str_stream(str); 
  std::string intermediate; 

	// Tokenizing w.r.t. '/'
	while(std::getline(str_stream, intermediate, '/')) { 
		tokens.push_back(intermediate); 
	} 
	return tokens[tokens.size() - 1];
}


static void
parseIntelCFG
(
 char *text_section,
 int text_section_size,
 CudaParse::Function &function
)
{
	KernelView kv(IGA_GEN9, text_section, text_section_size, iga::SWSB_ENCODE_MODE::SingleDistPipe);
  std::map<int, CudaParse::Block *> block_offset_map;

	int offset = 0;
	int size = 0;
  int block_id = 0;

  // Construct basic blocks
	while (offset < text_section_size) {
    auto *block = new CudaParse::Block(block_id, function.name + "_" + std::to_string(block_id)); 
    function.blocks.push_back(block);
    block_offset_map[offset] = block;

		size = kv.getInstSize(offset);
    auto *inst = new CudaParse::Inst(offset, size);
    block->insts.push_back(inst);

		while (!kv.isInstTarget(offset + size) && (offset + size < text_section_size)) {
			offset += size;	
			size = kv.getInstSize(offset);
			if (size == 0) {
				// this is a weird edge case, what to do?
				break;
			}

      inst = new CudaParse::Inst(offset, size);
      block->insts.push_back(inst);
		}

    offset = block->insts.back()->offset;
    if (kv.getOpcode(offset) == iga::Op::CALL || kv.getOpcode(offset) == iga::Op::CALLA) {
      inst->is_call = true;
    } else {
      inst->is_jump = true;
    }
  }

  // Construct targets
  std::array<int, KV_MAX_TARGETS_PER_INSTRUCTION> jump_targets;
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    auto *block = function.blocks[i];
    auto *inst = block->insts.back();
		size_t jump_targets_count = kv.getInstTargets(inst->offset, jump_targets.data());
		int next_block_start_offset = 0;
    if (i != function.blocks.size() - 1) {
      next_block_start_offset = function.blocks[i + 1]->insts.front()->offset;
    }

		for (size_t i = 0; i < jump_targets_count; i++) {
      auto *target_block = block_offset_map.at(jump_targets[i]);
			if (jump_targets[i] == next_block_start_offset) {
        // Fall through
        if (inst->is_call) {
          block->targets.push_back(new CudaParse::Target(inst, target_block, CudaParse::TargetType::CALL_FT));
        } else {
          block->targets.push_back(new CudaParse::Target(inst, target_block, CudaParse::TargetType::FALLTHROUGH));
        }
			} else {
        // Jump
        block->targets.push_back(new CudaParse::Target(inst, target_block, CudaParse::TargetType::DIRECT));
			}
		}
	}
}


bool
readIntelCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 std::map<int, int> &inst_size,
 bool cfg_wanted,
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj
)
{
  auto function_name = getFileNameFromAbsolutePath(elfFile->getFileName());
  addCustomFunctionObject(function_name, the_symtab); //adds a dummy function object

  char *text_section = NULL;
  auto text_section_size = elfFile->getTextSection(text_section);
  if (text_section_size == 0) {
    return false;
  }

  CudaParse::Function function(0, function_name);
  parseIntelCFG(text_section, text_section_size, function);
  std::vector<CudaParse::Function *> functions = {&function};

  CFGFactory *cfg_fact = new IntelCFGFactory(functions);
  *code_src = new IntelCodeSource(functions, the_symtab); 
  *code_obj = new CodeObject(*code_src, cfg_fact);
  (*code_obj)->parse();

  for (auto *block : function.blocks) {
    for (auto *inst : block->insts) {
      inst_size[inst->offset] = inst->size;
    }
  }

  return true;
}
