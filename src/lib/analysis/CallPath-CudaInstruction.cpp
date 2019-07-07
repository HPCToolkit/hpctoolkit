//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>

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

#include <lib/banal/cuda/InstructionAnalyzer.hpp>

#include <vector>
#include <iostream>

#define DEBUG_CALLPATH_CUDAINSTRUCTION 0

namespace Analysis {

namespace CallPath {

static int gpu_isample_index = -1;

// find call and stmt
static inline bool
isStmt(Prof::CCT::ANode *node) {
  auto *strct = node->structure();
  if (strct != NULL && strct->type() == Prof::Struct::ANode::TyStmt) {
    return true;
  }
  return false;
}


struct VMAStmt {
  VMA beg;
  VMA end;
  Prof::CCT::ANode *node;

  VMAStmt(VMA beg, VMA end, Prof::CCT::ANode *node) : 
    beg(beg), end(end), node(node) {}

  bool operator < (const VMAStmt &other) const {
    return this->beg < other.beg;
  }
};


void
overlayCudaInstructionsMain(Prof::CallPath::Profile &prof,
  const std::vector<std::string> &instruction_files) {
  // Check if prof contains gpu metrics
  auto *mgr = prof.metricMgr(); 
  for (size_t i = 0; i < mgr->size(); ++i) {
    if (mgr->metric(i)->namePfxBase() == "GPU INST") {
      gpu_isample_index = i;
      break;
    }
  }

  // Skip non-gpu prof
  if (gpu_isample_index == -1) {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Skip non-gpu prof" << std::endl;
    }
    return;
  }

  CudaParse::InstructionMetrics inst_metrics;
  // Read instruction files
  for (auto &file: instruction_files) {
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Read instruction file " << file << std::endl;
    }
    // Merge metrics
    CudaParse::InstructionAnalyzer::read(file, inst_metrics);
    if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
      std::cout << "Finish reading instruction file " << file << std::endl;
    }
  }

  // old_id-><inclusive id, exclusive id>
  std::map<int, std::pair<int, int> > metric_id_map;
  // Create new metrics in hpcprof
  for (auto it = inst_metrics.metric_names.begin(); it != inst_metrics.metric_names.end(); ++it) {
    auto &metric_name = it->first;
    auto old_metric_id = it->second;
    auto *in_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
    auto *ex_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
    in_desc->type(Prof::Metric::ADesc::TyIncl);
    ex_desc->type(Prof::Metric::ADesc::TyExcl);
    in_desc->partner(ex_desc);
    ex_desc->partner(in_desc);
    if (mgr->insertIf(in_desc) && mgr->insertIf(ex_desc)) {
      metric_id_map[old_metric_id] = std::make_pair<int, int>(in_desc->id(), ex_desc->id());
      if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
        std::cout << "Create metric " << metric_name <<
          " inclusive id " << in_desc->id() <<
          " exclusive id " << ex_desc->id() << std::endl;
      }
    }
  }

  // Sort instructions O(nlogn)
  std::sort(inst_metrics.inst_stats.begin(), inst_metrics.inst_stats.end());

  auto inst_pc_front = inst_metrics.inst_stats.front().pc;
  auto inst_pc_back = inst_metrics.inst_stats.back().pc;

  // Get all the stmt nodes
  std::vector<VMAStmt> vma_stmts;
  auto *prof_root = prof.cct()->root();
  Prof::CCT::ANodeIterator prof_it(prof_root, NULL/*filter*/, false/*leavesOnly*/,
    IteratorStack::PreOrder);
  for (Prof::CCT::ANode *n = NULL; (n = prof_it.current()); ++prof_it) {
    if (isStmt(n)) {
      auto *strct = n->structure();
      for (auto it = strct->vmaSet().begin(); it != strct->vmaSet().end(); ++it) {
        if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
          std::cout << " vma [" << it->beg() << ", " << it->end() << 
            ") at line " << strct->begLine() << std::endl;
        }
        // filter out
        if (it->end() < inst_pc_front || it->beg() > inst_pc_back) {
          continue;
        }
        vma_stmts.emplace_back(it->beg(), it->end(), n);
      }
    }
  }

  // Sort stmt nodes O(nlogn)
  std::sort(vma_stmts.begin(), vma_stmts.end());
  size_t cur_stmt_index = 0;

  // lay metrics over prof tree O(n)
  for (auto &inst_stat : inst_metrics.inst_stats) {
    while (cur_stmt_index < vma_stmts.size() && (inst_stat.pc < vma_stmts[cur_stmt_index].beg ||
      inst_stat.pc >= vma_stmts[cur_stmt_index].end)) {
      ++cur_stmt_index;
    }
    if (cur_stmt_index == vma_stmts.size()) {
      break;
    }
    auto cur_vma_begin = vma_stmts[cur_stmt_index].beg;
    auto cur_vma_end = vma_stmts[cur_stmt_index].end;
    for (auto iter = inst_stat.stat.begin(); iter != inst_stat.stat.end(); ++iter) {
      if (metric_id_map.find(iter->first) != metric_id_map.end()) {
        auto in_metric_id = metric_id_map[iter->first].first;
        auto ex_metric_id = metric_id_map[iter->first].second;
        auto metric_count  = iter->second;
        auto *node = vma_stmts[cur_stmt_index].node;
        node->demandMetric(in_metric_id) += static_cast<double>(metric_count);
        node->demandMetric(ex_metric_id) += static_cast<double>(metric_count);
        if (DEBUG_CALLPATH_CUDAINSTRUCTION) {
          auto in_count = node->metric(in_metric_id);
          auto ex_count = node->metric(ex_metric_id);
          auto *strct = node->structure();
          std::cout << "Associate pc " << inst_stat.pc << " with vma [" <<
            cur_vma_begin << ", " << cur_vma_end << ")" <<
            " inclusive id " << in_metric_id << " count " << in_count <<
            " exclusive id " << ex_metric_id << " count " << ex_count <<
            " at line " << strct->begLine() << std::endl;
        }
      }
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
          metrics.metric_names[metric_name] = std::stoi(metric_id);

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
                std::cout << "metric_id: " << metric_id << ", metric_count: " << metric_count << std::endl;
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
