//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <sstream>

#include <climits>
#include <cstring>
#include <cstdio>

#include <bitset>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <limits>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>

#include "GPUAdvisor.hpp"
#include "../MetricNameProfMap.hpp"
#include "../CCTGraph.hpp"

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

#include <lib/cuda/DotCFG.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>

#define DEBUG_GPUADVISOR 1
#define DEBUG_GPUADVISOR_DETAILS 1

#define WARP_SIZE 32

namespace Analysis {

void GPUAdvisor::summarizeFunctionBlames(const FunctionBlames &function_blames) {
  for (auto &function_blame_iter : function_blames) {
    auto &function_blame = function_blame_iter.second;
    auto *function = function_blame.function;
    // First line, total
    _output << function->name << "," << function_blame.blame << std::endl;
    // Following lines, blame metrics 
    for (auto &blame : function_blame.blames) {
      _output << _metric_name_prof_map->name(blame.first) << "," << blame.second << std::endl;
    }
  }
}


void GPUAdvisor::selectTopBlockBlames(const FunctionBlames &function_blames, BlockBlameQueue &top_block_blames) {
  // TODO(Keren): Clustering similar blocks?
  for (auto &function_blame_iter : function_blames) {
    for (auto &block_iter : function_blame_iter.second.block_blames) {
      auto &block_blame = block_iter.second;
      auto &min_block_blame = top_block_blames.top();
      if (min_block_blame.blame < block_blame.blame &&
        top_block_blames.size() >= _top_block_blames) {
        top_block_blames.pop();
      }
      top_block_blames.push(block_blame);
    }
  }
}


void GPUAdvisor::rankOptimizers(BlockBlameQueue &top_block_blames, OptimizerScoreMap &optimizer_scores) {
  while (top_block_blames.empty() == false) {
    auto &block_blame = top_block_blames.top();
    for (auto *optimizer : _code_optimizers) {
      double score = optimizer->match(block_blame);
      optimizer_scores[optimizer] += score;
    }
    for (auto *optimizer : _parallel_optimizers) {
      double score = optimizer->match(block_blame);
      optimizer_scores[optimizer] += score;
    }
    top_block_blames.pop();
  }
}


void GPUAdvisor::concatAdvise(const OptimizerScoreMap &optimizer_scores) {
  // Descendant order
  std::map<double, std::vector<GPUOptimizer *>, std::greater<double>> optimizer_rank;

  for (auto &iter : optimizer_scores) {
    auto *optimizer = iter.first;
    auto score = iter.second;
    optimizer_rank[score].push_back(optimizer);
  }

  size_t rank = 0;
  for (auto &iter : optimizer_rank) {
    for (auto *optimizer : iter.second) {
      ++rank;
      // TODO(Keren): concat advise for the current gpu_root
      _output << optimizer->advise();

      if (rank >= _top_optimizers) {
        // Reach limit
        break;
      }
    }
  }
}


void GPUAdvisor::advise(const TotalBlames &total_blames) {
  _output.clear();

  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks(); ++mpi_rank) {
    // For each MPI process
    for (auto thread_id = 0; thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank); ++thread_id) {
      // For each CPU thread
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) == -1) {
        // Skip tracing threads
        continue;
      }

      const CCTBlames &cct_blames = total_blames.at(mpi_rank).at(thread_id);
      
      for (auto &cct_blame_iter : cct_blames) {
        auto &function_blames = cct_blame_iter.second;

        // 1. Summarize function statistics
        summarizeFunctionBlames(function_blames);

        // 2. Pick top N important blocks
        BlockBlameQueue top_block_blames;
        selectTopBlockBlames(function_blames, top_block_blames);

        // 3. Rank optimizers
        OptimizerScoreMap optimizer_scores;
        rankOptimizers(top_block_blames, optimizer_scores);

        // 4. Output top 5 advise to output
        concatAdvise(optimizer_scores);
      }
    }
  }

  std::cout << _output.str();
}

}  // namespace Analysis
