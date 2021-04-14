#include "GPUAdvisor.hpp"
#include "GPUArchitecture.hpp"            // Gen9
//#include <lib/prof/CCT-Tree.hpp>
//#include <lib/prof/CCT-TreeIterator.hpp>  // ANodeIterator

void
GPUAdvisor::init
(
 const std::string &gpu_arch
)
{
  if (gpu_arch == "intelGen9") {
    this->_arch = new Gen9();
  } else {
    // do nothing
  }
  // should metrics be initialized here?
}


// 1. Init static instruction information in vma_prop_map
// 2. Init an instruction dependency graph
void
GPUAdvisor::configInst
(
 const std::string &lm_name,
 const std::vector<GPUParse::Function *> &functions,
 std::map<int, std::pair<int, int>> kernel_latency_frequency_map
)
{
  _node_map.clear();
  _function_offset.clear();

  // instruction property map
  for (auto *function : functions) {
    _function_offset[function->address] = function->address;  // renamed index to address

    for (auto *block : function->blocks) {
      for (auto *_inst : block->insts) {
        auto *inst = _inst->inst_stat;

        CCTNode node;

        node.inst = inst;
        node.vma = inst->pc;
        node.function = function;
        node.block = block;
        node.latency = kernel_latency_frequency_map[inst->pc].first;
        node.frequency = kernel_latency_frequency_map[inst->pc].second;

        _node_map[node.vma] = node;
      }
    }
  }

  // Instruction Graph
  for (auto &iter : _node_map) {
    auto *inst = iter.second.inst;
    //_inst_dep_graph.addNode(inst);

    // Add latency dependencies
    for (auto &inst_iter : inst->assign_pcs) {
      for (auto pc : inst_iter.second) {
        auto *dep_inst = _node_map.at(pc).inst;
        //_inst_dep_graph.addEdge(dep_inst->pc, inst->pc);
      }
    }

    for (auto pc : inst->predicate_assign_pcs) {
      auto *dep_inst = _node_map.at(pc).inst;
      //_inst_dep_graph.addEdge(dep_inst->pc, inst->pc);
    }
  }

  // [Aaron]: commenting this code unless its use is determined
#if 0
  // Static struct
  auto *struct_root = _prof->structure()->root();
  Prof::Struct::ANodeIterator struct_iter(struct_root, NULL /*filter*/, true /*leavesOnly*/,
      IteratorStack::PreOrder);
  for (Prof::Struct::ANode *n = NULL; (n = struct_iter.current()); ++struct_iter) {
    if (n->type() == Prof::Struct::ANode::TyStmt) {
      if (n->ancestorLM()->name() == lm_name) {
        auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(n);
        for (auto &vma_interval : stmt->vmaSet()) {
          auto vma = vma_interval.beg();
          _vma_struct_map[vma] = stmt;
        }
      }
    }
  }
#endif
}
