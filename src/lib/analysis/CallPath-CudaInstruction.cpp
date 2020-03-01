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
#include <include/gpu-metric-names.h>

#include "CallPath-CudaInstruction.hpp"
#include "CallPath-CudaAdvisor.hpp"

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
    auto metric_ids = metric_name_prof_map.metric_ids(metric_name);
    if (metric_ids.size() == 0) {
      if (metric_name_prof_map.add(metric_name)) {
        if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
          metric_ids = metric_name_prof_map.metric_ids(metric_name);
          for (auto id : metric_ids) {
            std::cout << "Create metric " << metric_name <<
              " inclusive id " << id << std::endl;
          }
        }
      }
    } else {
      // we already have raw_id-><inclusive id, exclusive id> mappings
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        metric_ids = metric_name_prof_map.metric_ids(metric_name);
        for (auto id : metric_ids) {
          std::cout << "Existing metric " << metric_name <<
            " inclusive id " << id << std::endl;
        }
      }
    }
  }
}


static void
gatherStmts(const Prof::LoadMap::LMId_t lm_id, const std::vector<CudaParse::InstructionStat *> &inst_stats,
  const Prof::CCT::ANode *prof_root, std::vector<VMAStmt> &vma_stmts, std::set<Prof::CCT::ADynNode *> &gpu_roots) {
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

      // Get the parent of an instruction node as the gpu root node
      Prof::CCT::ADynNode* n_parent = dynamic_cast<Prof::CCT::ADynNode*>(n->parent());
      gpu_roots.insert(n_parent);
    }
  }

  // Sort stmt nodes O(nlogn)
  std::sort(vma_stmts.begin(), vma_stmts.end());
}


static void
associateInstStmts(const std::vector<VMAStmt> &vma_stmts,
  const std::vector<CudaParse::InstructionStat *> &inst_stats,
  MetricNameProfMap &metric_name_prof_map) {
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
    auto issue_metric_ids = metric_name_prof_map.metric_ids(GPU_INST_METRIC_NAME":STL_NONE");
    auto in_metric_ids = metric_name_prof_map.metric_ids(inst_stat->op, true);
    auto ex_metric_ids = metric_name_prof_map.metric_ids(inst_stat->op, false);
    if (in_metric_ids.size() != ex_metric_ids.size()) {
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "Inclusive and exclusive metrics not match: " << inst_stat->op << std::endl;
      }
      continue;
    }
    for (size_t i = 0; i < issue_metric_ids.size(); ++i) {
      double sum_insts_f = node->demandMetric(issue_metric_ids[i]);
      auto in_metric_id = in_metric_ids[i];
      auto ex_metric_id = ex_metric_ids[i];

      // Calculate estimate number of executed instructions
      node->demandMetric(in_metric_id) += sum_insts_f;
      node->demandMetric(ex_metric_id) += sum_insts_f;

      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        auto in_count = node->metric(in_metric_id);
        auto ex_count = node->metric(ex_metric_id);
        std::cout << "Associate pc: 0x" << std::hex << inst_stat->pc <<
          " with vma: " << cur_vma << std::dec <<
          " inclusive id " << in_metric_id <<
          " name " << metric_name_prof_map.name(in_metric_id) << " count " << in_count <<
          " exclusive id " << ex_metric_id <<
          " name " << metric_name_prof_map.name(ex_metric_id) << " count " << ex_count << std::endl;
      }
    }
  }
}


void
overlayCudaInstructionsMain(Prof::CallPath::Profile &prof,
  const std::vector<std::string> &instruction_files) {
  auto *mgr = prof.metricMgr(); 
  MetricNameProfMap metric_name_prof_map(mgr);
  metric_name_prof_map.init();

  // Check if prof contains gpu metrics
  // Skip non-gpu prof
  if (metric_name_prof_map.metric_ids(GPU_INST_METRIC_NAME":STL_NONE").size() == 0) {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Skip non-gpu prof" << std::endl;
    }
    return;
  }

  CudaAdvisor cuda_advisor(&prof, &metric_name_prof_map);
  cuda_advisor.init();
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

    // Step 2: Sort the instructions by PC
    // Assign absolute addresses to instructions
    CudaParse::relocateCudaInstructionStats(functions);

    std::vector<CudaParse::InstructionStat *> inst_stats;
    // Put instructions to a vector
    CudaParse::flatCudaInstructionStats(functions, inst_stats);

    // Step 3: Find new metric names and insert new mappings between from name to prof metric ids
    createMetrics(inst_stats, metric_name_prof_map, *mgr);

    // Step 4: Gather all CCT nodes with lm_id and find GPU roots
    std::vector<VMAStmt> vma_stmts;
    std::set<Prof::CCT::ADynNode *> gpu_roots;
    auto *prof_root = prof.cct()->root();
    gatherStmts(lm_id, inst_stats, prof_root, vma_stmts, gpu_roots);

    // Step 5: Lay metrics over prof tree
    associateInstStmts(vma_stmts, inst_stats, metric_name_prof_map);

    cuda_advisor.configInst(functions);

    // Step 6: Make advise
    // Find each GPU calling context, make recommendation for each calling context 
    for (auto *gpu_root : gpu_roots) {
      // Pass current gpu root 
      cuda_advisor.configGPURoot(gpu_root);

      // <mpi_rank, <thread_id, <blames>>>
      FunctionBlamesMap function_blames_map;

      // Blame latencies
      cuda_advisor.blame(function_blames_map);

      // Make advise for the calling context and cache result
      //cuda_advisor.advise(function_blames_map);
    }

    // TODO(Keren): output advise using this file other than cuda_advisor
    // Save advise for this file and clear cache
    cuda_advisor.save(file + ".advise");
    
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Finish reading instruction file " << file << std::endl;
    }
  }
}

}  // namespace CallPath

}  // namespace Analysis
