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

#include <lib/cuda/InstructionAnalyzer.hpp>

#include <vector>
#include <iostream>

#define DEBUG_CALLPATH_CUDAINSTRUCTION 0

namespace Analysis {

namespace CallPath {

typedef std::map<std::string, std::pair<int, int> > MetricNameProfMap;

typedef std::map<int, std::pair<int, int> > MetricIdProfMap;

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
createMetrics(MetricNameProfMap &metric_name_prof_map, MetricNameIdMap &metric_name_id_map,
  MetricIdProfMap &metric_id_prof_map, Prof::Metric::Mgr &mgr) {
  // Create new metrics in hpcprof
  // raw_id-><inclusive id, exclusive id>
  // Existing metric names
  for (auto it = metric_name_id_map.begin(); it != metric_name_id_map.end(); ++it) {
    auto &metric_name = it->first;
    auto raw_metric_id = it->second;
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

    if (in_id != -1 && ex_id != -1) {
      metric_id_prof_map[raw_metric_id] = std::pair<int, int>(in_id, ex_id);
    } else {
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "Failed to add metric " << metric_name << std::endl;
      }
    }
  }

}


static void
gatherStmts(Prof::LoadMap::LMId_t lm_id, CudaParse::InstructionStats &inst_stats,
  std::vector<VMAStmt> &vma_stmts, Prof::CCT::ANode *prof_root) {
  // Sort instructions O(nlogn)
  std::sort(inst_stats.begin(), inst_stats.end());

  auto inst_pc_front = inst_stats.front().pc;
  auto inst_pc_back = inst_stats.back().pc;

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
associateInstStmts(const std::vector<VMAStmt> &vma_stmts, CudaParse::InstructionStats &inst_stats,
  MetricIdProfMap &metric_id_prof_map, Prof::Metric::Mgr &mgr) {
  size_t cur_stmt_index = 0;

  // Lay metrics over prof tree O(n)
  for (auto &inst_stat : inst_stats) {
    // while lm_ip < inst_stat.pc
    while (cur_stmt_index < vma_stmts.size() && inst_stat.pc > vma_stmts[cur_stmt_index].lm_ip) {
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "inst_stat.pc: 0x" << std::hex << inst_stat.pc << ", vma: 0x" <<
          vma_stmts[cur_stmt_index].lm_ip << std::dec << std::endl;
      }
      ++cur_stmt_index;
    }
    if (cur_stmt_index == vma_stmts.size()) {
      break;
    }
    if (inst_stat.pc != vma_stmts[cur_stmt_index].lm_ip) {
      continue;
    }

    auto cur_vma = vma_stmts[cur_stmt_index].lm_ip;
    auto *node = vma_stmts[cur_stmt_index].node;
    double sum_insts_f = getTotalGPUInst(node);
    for (auto iter = inst_stat.stat.begin(); iter != inst_stat.stat.end(); ++iter) {
      if (metric_id_prof_map.find(iter->first) == metric_id_prof_map.end()) {
        if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
          std::cout << "Fail to find metric id: " << iter->first << std::endl;
        }
        continue;
      }
      auto in_metric_id = metric_id_prof_map[iter->first].first;
      auto ex_metric_id = metric_id_prof_map[iter->first].second;

      // Calculate estimate number of executed instructions
      node->demandMetric(in_metric_id) += sum_insts_f;
      node->demandMetric(ex_metric_id) += sum_insts_f;
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        auto in_count = node->metric(in_metric_id);
        auto ex_count = node->metric(ex_metric_id);
        std::cout << "Associate pc: 0x" << std::hex << inst_stat.pc <<
          " with vma: " << cur_vma << std::dec <<
          " inclusive id " << in_metric_id <<
          " name " << mgr.metric(in_metric_id)->name() << " count " << in_count <<
          " exclusive id " << ex_metric_id <<
          " name " << mgr.metric(ex_metric_id)->name() << " count " << ex_count << std::endl;
      }
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
    CudaParse::InstructionMetrics inst_metrics;
    CudaParse::InstructionAnalyzer::read(file, inst_metrics);
    
    // Step 2: Merge metrics
    // Find new metric names and insert new mappings between from raw ids to prof metric ids
    MetricNameIdMap &metric_name_id_map = inst_metrics.metric_names;
    MetricIdProfMap metric_id_prof_map;
    createMetrics(metric_name_prof_map, metric_name_id_map, metric_id_prof_map, *mgr);
    
    // Step 3: Gather all CCT nodes with lm_id
    std::vector<VMAStmt> vma_stmts;
    CudaParse::InstructionStats &inst_stats = inst_metrics.inst_stats;
    auto *prof_root = prof.cct()->root();
    gatherStmts(lm_id, inst_stats, vma_stmts, prof_root);

    // Step 4: Lay metrics over prof tree
    associateInstStmts(vma_stmts, inst_stats, metric_id_prof_map, *mgr);
    
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Finish reading instruction file " << file << std::endl;
    }
  }
}

}  // namespace CallPath

}  // namespace Analysis


namespace CudaParse {

bool InstructionAnalyzer::read(
  const std::string &file_path, CudaParse::InstructionMetrics &metrics, bool sparse) {
  std::ifstream ifs(file_path, std::ifstream::in);
  if ((ifs.rdstate() & std::ifstream::failbit) != 0) {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Error opening " << file_path << std::endl;
    }
    return false;
  }

  const char sep = sparse ? '\n' : '#'; 

  std::string buf;
  if (std::getline(ifs, buf) && buf == "<metric names>") {
    if (std::getline(ifs, buf)) {
      std::istringstream iss(buf);
      std::string cur_buf;
      while (std::getline(iss, cur_buf, sep)) {
        // (mn,id)#
        // 01234567
        //    p  s
        auto pos = cur_buf.find(",");
        if (pos != std::string::npos) {
          auto metric_name = cur_buf.substr(1, pos - 1);
          auto metric_id = cur_buf.substr(pos + 1, cur_buf.size() - pos - 2);
          // Add a MIX prefix
          metrics.metric_names["MIX:" + metric_name] = std::stoi(metric_id);

          if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
            std::cout << "metric_name: " << metric_name << ", metric_id: " << metric_id << std::endl;
          }
        }
      }
    }
  } else {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Error reading metrics " << file_path << std::endl;
    }
    return false;
  }
  
  if (std::getline(ifs, buf) && buf == "<inst stats>") {
    if (std::getline(ifs, buf)) {
      std::istringstream iss(buf);
      std::string cur_buf;
      while (std::getline(iss, cur_buf, sep)) {
        bool first = true;
        CudaParse::InstructionStat inst_stat;
        std::istringstream isss(cur_buf);
        // (pc,id:mc,...)#
        while (std::getline(isss, cur_buf, ',')) {
          if (cur_buf == ")") {
            break;
          }
          if (first) {
            // (111,
            // 01234
            std::string tmp = cur_buf.substr(1);
            inst_stat.pc = std::stoi(tmp);
            first = false;
          } else {
            // id:mc,
            // 012345
            //   p  s
            auto pos = cur_buf.find(":");
            if (pos != std::string::npos) {
              auto metric_id = cur_buf.substr(0, pos);
              auto metric_count = cur_buf.substr(pos + 1, cur_buf.size() - pos - 1);
              inst_stat.stat[std::stoi(metric_id)] = std::stoi(metric_count);

              if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
                std::cout << "pc 0x" << std::hex << inst_stat.pc << std::dec <<
                  " metric_id: " << metric_id << ", metric_count: " << metric_count << std::endl;
              }
            }
          }
        }
        metrics.inst_stats.emplace_back(inst_stat);
      }
    }
  } else {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Error reading stats " << file_path << std::endl;
    }
    return false;
  }

  return true;
}

}  // CudaParse
