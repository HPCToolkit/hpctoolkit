#include "CudaCFGFactory.hpp"
#include "CudaFunction.hpp"
#include <iostream>

#define DEBUG_CUDA_CFGFACTORY 0

namespace Dyninst {
namespace ParseAPI {

Function *CudaCFGFactory::mkfunc(Address addr, FuncSource src, 
  std::string name, CodeObject * obj, CodeRegion * region, 
  Dyninst::InstructionSource * isrc) {
  // Find function by name
  for (auto *function : _functions) {
    if (function->name == name) {
      CudaFunction *ret_func = new CudaFunction(function->address, name, obj, region, isrc);

      bool first_entry = true;
      if (DEBUG_CUDA_CFGFACTORY) {
        std::cout << "Function: " << function->name << " addr: 0x" <<
          std::hex << addr << std::dec << std::endl;
      }
      for (auto *block : function->blocks) {
        CudaBlock *ret_block = NULL;
        // If a block has not been created by callers, create it
        // Otherwise get the block from _block_filter
        if (_block_filter.find(block->id) == _block_filter.end()) {
          if (DEBUG_CUDA_CFGFACTORY) {
            std::cout << "New block: " << block->name << " id: " << block->id << std::endl;
          }
          std::vector<Offset> inst_offsets;
          for (auto *inst : block->insts) {
            inst_offsets.push_back(inst->offset);
          }
          ret_block = new CudaBlock(obj, region, block->address, inst_offsets);
          _block_filter[block->id] = ret_block;
          blocks_.add(ret_block);
        } else {
          if (DEBUG_CUDA_CFGFACTORY) {
            std::cout << "Old block: " << block->name << " id: " << block->id << std::endl;
          }
          ret_block = _block_filter[block->id];
        }
        ret_func->add_block(ret_block);

        if (first_entry) {
          ret_func->setEntry(ret_block);
          first_entry = false;
        }

        // Create edges and related blocks
        for (auto *target : block->targets) {
          CudaBlock *ret_target_block = NULL;
          if (_block_filter.find(target->block->id) == _block_filter.end()) {
            if (DEBUG_CUDA_CFGFACTORY) {
              std::cout << "New block: " << target->block->name << " id: " << target->block->id << std::endl;
            }
            std::vector<Offset> inst_offsets;
            for (auto *inst : target->block->insts) {
              inst_offsets.push_back(inst->offset);
            }
            ret_target_block = new CudaBlock(obj, region, target->block->address, inst_offsets);
            _block_filter[target->block->id] = ret_target_block;
            blocks_.add(ret_target_block);
          } else {
            if (DEBUG_CUDA_CFGFACTORY) {
              std::cout << "Old block: " << target->block->name << " id: " << target->block->id << std::endl;
            }
            ret_target_block = _block_filter[target->block->id];
          }

          Edge *ret_edge = new Edge(ret_block, ret_target_block, target->type);
          ret_edge->ignore_index();
          if (DEBUG_CUDA_CFGFACTORY) {
            std::cout << "Edge: "<< " -> " << target->block->name << std::endl;
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

}
}
