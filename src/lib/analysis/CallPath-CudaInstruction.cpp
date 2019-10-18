//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>
#include <cstdio>

#include <typeinfo>
#include <unordered_map>
#include <algorithm>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>

#include "CallPath-CudaInstruction.hpp"

using std::string;

#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Struct-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Metric-ADesc.hpp>

#include <lib/profxml/XercesUtil.hpp>
#include <lib/profxml/PGMReader.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>

#include <lib/cuda/AnalyzeInstruction.hpp>

#include <vector>
#include <iostream>

#define DEBUG_CALLPATH_CUDAINSTRUCTION 0

namespace Analysis {

namespace CallPath {

typedef std::map<std::string, std::pair<int, int> > MetricNameProfMap;

typedef std::map<std::string, int> MetricNameIdMap;

static std::vector<size_t> gpu_issue_index;

struct VMAStmt {
  VMA lm_ip;
  Prof::CCT::ANode *node;

  VMAStmt(VMA lm_ip, Prof::CCT::ANode *node) : 
    lm_ip(lm_ip), node(node) {}

  bool operator < (const VMAStmt &other) const {
    return this->lm_ip < other.lm_ip;
  }
};


static inline int
getTotalGPUInst(Prof::CCT::ANode *node) {
  int sum = 0;
  // GPU INST - GPU LAT
  for (size_t i = 0; i < gpu_issue_index.size(); ++i) {
    sum += node->demandMetric(gpu_issue_index[i]);
  }
  return sum;
}


static inline std::string
getLMfromInst(const std::string &inst_file) {
  char resolved_path[PATH_MAX];
  realpath(inst_file.c_str(), resolved_path);
  std::string file = std::string(resolved_path);

  // xxxx/structs/xxxx/xxxx.cubin.inst
  auto dot_pos = file.rfind(".");
  std::string cubin_file = file.substr(0, dot_pos);

  auto slash_pos0 = cubin_file.rfind("/structs");
  auto slash_pos1 = cubin_file.rfind("/");
  cubin_file.replace(slash_pos0, slash_pos1 - slash_pos0, "/cubins"); 
  std::cout << cubin_file << std::endl;

  return cubin_file;
}


static void
createMetrics(const std::vector<CudaParse::InstructionStat *> &inst_stats,
  MetricNameProfMap &metric_name_prof_map, Prof::Metric::Mgr &mgr) {
  // Create new metrics in hpcprof
  // raw_id-><inclusive id, exclusive id>
  // Existing metric names
  for (auto *inst_stat : inst_stats) {
    auto &metric_name = inst_stat->op;
    int in_id = -1;
    int ex_id = -1;
    if (metric_name_prof_map.find(metric_name) == metric_name_prof_map.end()) {
      // Create inclusive and exclusive descriptors
      auto *in_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
      auto *ex_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
      in_desc->type(Prof::Metric::ADesc::TyIncl);
      ex_desc->type(Prof::Metric::ADesc::TyExcl);
      in_desc->partner(ex_desc);
      ex_desc->partner(in_desc);

      if (mgr.insertIf(in_desc) && mgr.insertIf(ex_desc)) {
        in_id = in_desc->id();
        ex_id = ex_desc->id();
        // Add new metric names
        metric_name_prof_map[metric_name] = std::pair<int, int>(in_id, ex_id);
        if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
          std::cout << "Create metric " << metric_name <<
            " inclusive id " << in_desc->id() <<
            " exclusive id " << ex_desc->id() << std::endl;
        }
      }
    } else {
      // we already have raw_id-><inclusive id, exclusive id> mappings
      in_id = metric_name_prof_map[metric_name].first;
      ex_id = metric_name_prof_map[metric_name].second;
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "Existing metric " << metric_name <<
          " inclusive id " << in_id <<
          " exclusive id " << ex_id << std::endl;
      }
    }
  }
}


static void
gatherStmts(const Prof::LoadMap::LMId_t lm_id, const std::vector<CudaParse::InstructionStat *> &inst_stats,
  const Prof::CCT::ANode *prof_root, std::vector<VMAStmt> &vma_stmts) {
  auto inst_pc_front = inst_stats.front()->pc;
  auto inst_pc_back = inst_stats.back()->pc;

  if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
    std::cout << "inst pc range: [0x" << std::hex << inst_pc_front <<
      ", 0x" << inst_pc_back << "]" << std::dec << std::endl;
  }

  // Get all the stmt nodes
  Prof::CCT::ANodeIterator prof_it(prof_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
    // TODO(keren): match loadmap
    //if (isStmt(n)) {
      Prof::CCT::ADynNode* n_dyn = dynamic_cast<Prof::CCT::ADynNode*>(n);
      if (n_dyn) {
        Prof::LoadMap::LMId_t n_lm_id = n_dyn->lmId(); // ok if LoadMap::LMId_NULL
        VMA n_lm_ip = n_dyn->lmIP();
        // filter out
        if (n_lm_id != lm_id || n_lm_ip < inst_pc_front || n_lm_ip > inst_pc_back) {
          continue;
        }

        if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
          std::cout << "Find CCT node vma: 0x" << std::hex << n_lm_ip << std::dec << std::endl;
        }

        vma_stmts.emplace_back(n_lm_ip, n);
      }
    //}
  }

  // Sort stmt nodes O(nlogn)
  std::sort(vma_stmts.begin(), vma_stmts.end());
}


static void
associateInstStmts(const std::vector<VMAStmt> &vma_stmts, const std::vector<CudaParse::InstructionStat *> &inst_stats,
  MetricNameProfMap &metric_name_prof_map, Prof::Metric::Mgr &mgr) {
  size_t cur_stmt_index = 0;

  // Lay metrics over prof tree O(n)
  for (auto *inst_stat : inst_stats) {
    // while lm_ip < inst_stat->pc
    while (cur_stmt_index < vma_stmts.size() && inst_stat->pc > vma_stmts[cur_stmt_index].lm_ip) {
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "inst_stat->pc: 0x" << std::hex << inst_stat->pc << ", vma: 0x" <<
          vma_stmts[cur_stmt_index].lm_ip << std::dec << std::endl;
      }
      ++cur_stmt_index;
    }
    if (cur_stmt_index == vma_stmts.size()) {
      break;
    }
    if (inst_stat->pc != vma_stmts[cur_stmt_index].lm_ip) {
      continue;
    }

    auto cur_vma = vma_stmts[cur_stmt_index].lm_ip;
    auto *node = vma_stmts[cur_stmt_index].node;
    double sum_insts_f = getTotalGPUInst(node);
    if (metric_name_prof_map.find(inst_stat->op) == metric_name_prof_map.end()) {
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "Fail to find metric: " << inst_stat->op << std::endl;
      }
      continue;
    }
    auto in_metric_id = metric_name_prof_map[inst_stat->op].first;
    auto ex_metric_id = metric_name_prof_map[inst_stat->op].second;

    // Calculate estimate number of executed instructions
    node->demandMetric(in_metric_id) += sum_insts_f;
    node->demandMetric(ex_metric_id) += sum_insts_f;
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      auto in_count = node->metric(in_metric_id);
      auto ex_count = node->metric(ex_metric_id);
      std::cout << "Associate pc: 0x" << std::hex << inst_stat->pc <<
        " with vma: " << cur_vma << std::dec <<
        " inclusive id " << in_metric_id <<
        " name " << mgr.metric(in_metric_id)->name() << " count " << in_count <<
        " exclusive id " << ex_metric_id <<
        " name " << mgr.metric(ex_metric_id)->name() << " count " << ex_count << std::endl;
    }
  }
}


void
overlayCudaInstructionsMain(Prof::CallPath::Profile &prof,
  const std::vector<std::string> &instruction_files) {

  // Check if prof contains gpu metrics
  auto *mgr = prof.metricMgr(); 
  for (size_t i = 0; i < mgr->size(); ++i) {
    if (mgr->metric(i)->namePfxBase() == "STALL:NONE" &&
      mgr->metric(i)->type() == Prof::Metric::ADesc::TyIncl) {
      // Assume exclusive metrics index is i+1
      gpu_issue_index.push_back(i);
    }
  }
  // Skip non-gpu prof
  if (gpu_issue_index.size() == 0) {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Skip non-gpu prof" << std::endl;
    }
    return;
  }

  MetricNameProfMap metric_name_prof_map;
  // Read instruction files
  for (auto &file: instruction_files) {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
      std::cout << "Read instruction file " << file << std::endl;
      std::cout << "-------------------------------------------------" << std::endl;
    }

    std::string lm_name = getLMfromInst(file);
    Prof::LoadMap::LMSet_nm::iterator lm_iter;
    if ((lm_iter = prof.loadmap()->lm_find(lm_name)) == prof.loadmap()->lm_end_nm()) {
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "Instruction file module " << lm_name << " not found " << std::endl;
      }
      continue;
    }

    Prof::LoadMap::LMId_t lm_id = (*lm_iter)->id();

    // Step 1: Read metrics
    std::vector<CudaParse::Function *> functions;
    CudaParse::readCudaInstructions(file, functions);

    // Sort the instructions by PC
    std::vector<CudaParse::InstructionStat *> inst_stats;
    CudaParse::flatCudaInstructionStats(functions, inst_stats);
    
    // Step 2: Merge metrics
    // Find new metric names and insert new mappings between from name to prof metric ids
    createMetrics(inst_stats, metric_name_prof_map, *mgr);
    
    // Step 3: Gather all CCT nodes with lm_id
    std::vector<VMAStmt> vma_stmts;
    auto *prof_root = prof.cct()->root();
    gatherStmts(lm_id, inst_stats, prof_root, vma_stmts);

    // Step 4: Lay metrics over prof tree
    associateInstStmts(vma_stmts, inst_stats, metric_name_prof_map, *mgr);
    
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Finish reading instruction file " << file << std::endl;
    }
  }
}

}  // namespace CallPath

}  // namespace Analysis
