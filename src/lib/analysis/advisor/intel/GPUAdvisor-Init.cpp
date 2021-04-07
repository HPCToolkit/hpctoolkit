#include "GPUAdvisor.hpp"
#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/CCT-TreeIterator.hpp>  // ANodeIterator

// 1. Init static instruction information in vma_prop_map
// 2. Init an instruction dependency graph
void GPUAdvisor::configInst(const std::string &lm_name,
    const std::vector<GPUParse::Function *> &functions) {
  _vma_struct_map.clear();
  _vma_prop_map.clear();
  _inst_dep_graph.clear();
  _function_offset.clear();

  // Property map
  for (auto *function : functions) {
    _function_offset[function->address] = function->address;  // renamed index to address

    for (auto *block : function->blocks) {
      for (auto *_inst : block->insts) {
        auto *inst = _inst->inst_stat;

        VMAProperty prop;

        prop.inst = inst;
        // Ensure inst->pc has been relocated
        prop.vma = inst->pc;
        prop.function = function;
        prop.block = block;

        auto latency = _arch->latency(inst->op);
        prop.latency_lower = latency.first;
        prop.latency_upper = latency.second;
        prop.latency_issue = _arch->issue(inst->op);

        _vma_prop_map[prop.vma] = prop;
      }
    }
  }

  // Instruction Graph
  for (auto &iter : _vma_prop_map) {
    auto *inst = iter.second.inst;
    _inst_dep_graph.addNode(inst);

    // Add latency dependencies
    for (auto &inst_iter : inst->assign_pcs) {
      for (auto pc : inst_iter.second) {
        auto *dep_inst = _vma_prop_map.at(pc).inst;
        _inst_dep_graph.addEdge(dep_inst, inst);
      }
    }

    for (auto &inst_iter : inst->passign_pcs) {
      for (auto pc : inst_iter.second) {
        auto *dep_inst = _vma_prop_map.at(pc).inst;
        _inst_dep_graph.addEdge(dep_inst, inst);
      }
    }

    for (auto &inst_iter : inst->uassign_pcs) {
      for (auto pc : inst_iter.second) {
        auto *dep_inst = _vma_prop_map.at(pc).inst;
        _inst_dep_graph.addEdge(dep_inst, inst);
      }
    }

    for (auto &inst_iter : inst->bassign_pcs) {
      for (auto pc : inst_iter.second) {
        auto *dep_inst = _vma_prop_map.at(pc).inst;
        _inst_dep_graph.addEdge(dep_inst, inst);
      }
    }

    for (auto pc : inst->predicate_assign_pcs) {
      auto *dep_inst = _vma_prop_map.at(pc).inst;
      _inst_dep_graph.addEdge(dep_inst, inst);
    }
  }

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
}
