#include "CudaCFGFactory.hpp"
#include "CudaFunction.hpp"
#include <iostream>

namespace Dyninst {
namespace ParseAPI {

Function *CudaCFGFactory::mkfunc(Address addr, FuncSource src, 
  std::string name, CodeObject * obj, CodeRegion * region, 
  Dyninst::InstructionSource * isrc) {
  // find function by name
  for (auto *function : _functions) {
    if (function->name == name) {
      CudaFunction *ret_func = new CudaFunction(addr, name, obj, region, isrc);

      bool first_entry = true;
      for (auto *block : function->blocks) {
        CudaBlock *ret_block = NULL;
        if (_block_filter.find(block->id) == _block_filter.end()) {
          std::vector<Offset> inst_offsets;
          for (auto *inst : block->insts) {
            inst_offsets.push_back(inst->offset);
          }
          ret_block = new CudaBlock(obj, region, block->insts[0]->offset, inst_offsets);
          _block_filter[block->id] = ret_block;
          blocks_.add(*ret_block);
          ret_func->add_block(ret_block);
        } else {
          ret_block = _block_filter[block->id];
        }

        if (first_entry) {
          ret_func->setEntry(ret_block);
          first_entry = false;
        }

        for (auto *target : block->targets) {
          CudaBlock *ret_target_block = NULL;
          if (_block_filter.find(target->block->id) == _block_filter.end()) {
            std::vector<Offset> inst_offsets;
            for (auto *inst : target->block->insts) {
              inst_offsets.push_back(inst->offset);
            }
            ret_target_block = new CudaBlock(obj, region, target->block->insts[0]->offset, inst_offsets);
            _block_filter[target->block->id] = ret_target_block;
            blocks_.add(*ret_target_block);
            ret_func->add_block(ret_target_block);
          } else {
            ret_target_block = _block_filter[target->block->id];
          }

          Edge *ret_edge = NULL;
          if (target->type == CudaParse::CALL) {
            ret_edge = new Edge(ret_block, ret_target_block, CALL);
          } else if (target->type = CudaParse::FALLTHROUGH) { 
            ret_edge = new Edge(ret_block, ret_target_block, FALLTHROUGH);
          } else {  // TODO(Keren): Add more edge types
            ret_edge = new Edge(ret_block, ret_target_block, DIRECT);
          }
          ret_edge->install();
          edges_.add(*ret_edge);
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
