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
// Copyright ((c)) 2002-2024, Rice University
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

//***************************************************************************
// system includes
//***************************************************************************
#include <iostream>



//***************************************************************************
// local includes
//***************************************************************************

#include "GPUCFGFactory.hpp"
#include "GPUFunction.hpp"



//***************************************************************************
// macros
//***************************************************************************

#define DEBUG_GPU_CFGFACTORY 0



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {


//***************************************************************************
// interface operations
//***************************************************************************


Function *GPUCFGFactory::mkfunc(Address addr, FuncSource src,
  std::string name, CodeObject * obj, CodeRegion * region,
  Dyninst::InstructionSource * isrc) {
  // Find function by name
  for (auto *function : _functions) {
    if (function->name == name) {
      GPUFunction *ret_func = new GPUFunction(function->address, name, obj, region, isrc);

      bool first_entry = true;
      if (DEBUG_GPU_CFGFACTORY) {
        std::cout << "******\nFunction: " << function->name
                  << " addr: 0x" << std::hex << addr << std::dec << std::endl;
      }
      for (auto *block : function->blocks) {
        auto arch = block->insts.front()->arch;

        GPUBlock *ret_block = NULL;
        // If a block has not been created by callers, create it
        // Otherwise get the block from _block_filter

        {
          tbb::concurrent_hash_map<size_t, GPUBlock *>::accessor a;
          if (_block_filter.insert(a, std::make_pair(block->address, nullptr))) {
            if (DEBUG_GPU_CFGFACTORY) {
              std::cout << "New block: " << block->name << " id: " << block->id
                        << " addr: 0x" << std::hex << block->address << std::dec << std::endl;
            }
            std::vector<std::pair<Offset, size_t>> inst_offsets;
            for (auto *inst : block->insts) {
              inst_offsets.emplace_back(std::make_pair(inst->offset, inst->size));
            }
            auto last_addr = inst_offsets.back().first;
            auto block_end = last_addr + inst_offsets.back().second;
            ret_block = new GPUBlock(obj, region, block->address,
              block_end, last_addr, block->insts, arch);
            a->second = ret_block;
            blocks_.add(ret_block);
          } else {
            if (DEBUG_GPU_CFGFACTORY) {
              std::cout << "Old block: " << block->name << " id: " << block->id
                        << " addr: 0x" << std::hex << block->address << std::dec << std::endl;
            }
            ret_block = a->second;
          }
        }

        ret_func->add_block(ret_block);

        if (first_entry) {
          ret_func->setEntry(ret_block);
          first_entry = false;
        }

        // Create edges and related blocks
        for (auto *target : block->targets) {
          GPUBlock *ret_target_block = NULL;
          {
            tbb::concurrent_hash_map<size_t, GPUBlock *>::accessor a;
            if (_block_filter.insert(a, std::make_pair(target->block->address, nullptr))) {
              if (DEBUG_GPU_CFGFACTORY) {
                std::cout << "New block: " << target->block->name << " id: " << target->block->id
                          << " addr: 0x" << std::hex << target->block->address << std::dec << std::endl;
              }
              std::vector<std::pair<Offset, size_t>> inst_offsets;
              for (auto *inst : target->block->insts) {
                inst_offsets.push_back(std::make_pair(inst->offset, inst->size));
              }
              auto last_addr = inst_offsets.back().first;
              auto block_end = last_addr + inst_offsets.back().second;
              ret_target_block = new GPUBlock(obj, region, target->block->address,
                block_end, last_addr, target->block->insts, arch);
              a->second = ret_target_block;
              blocks_.add(ret_target_block);
            } else {
              if (DEBUG_GPU_CFGFACTORY) {
                std::cout << "Old block: " << target->block->name << " id: " << target->block->id
                          << " addr: 0x" << std::hex << target->block->address << std::dec << std::endl;
              }
              ret_target_block = a->second;
            }
          }

          Edge *ret_edge = new Edge(ret_block, ret_target_block, target->type);
          ret_edge->ignore_index();
          if (DEBUG_GPU_CFGFACTORY) {
            std::cout << "Edge: "<< block->name << " -> " << target->block->name << std::endl;
          }
          ret_edge->install();
          edges_.add(ret_edge);
        }
      }
      return ret_func;
    }
  }
  return NULL;
  // iterate blocks
  // add blocks
  // iterate targets
  // add edges
}



//***************************************************************************
// end namespaces
//***************************************************************************
}
}
