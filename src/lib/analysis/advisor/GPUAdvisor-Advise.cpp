//************************* System Include Files ****************************

#include <fstream>
#include <iostream>
#include <sstream>

#include <climits>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <bitset>
#include <iomanip>
#include <limits>
#include <queue>
#include <string>
#include <typeinfo>
#include <unordered_map>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>
#include <include/uint.h>

#include "../CCTGraph.hpp"
#include "../MetricNameProfMap.hpp"
#include "GPUAdvisor.hpp"
#include "Inspection.hpp"

using std::string;

#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Metric-ADesc.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <lib/profxml/PGMReader.hpp>
#include <lib/profxml/XercesUtil.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/cuda/DotCFG.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/IOUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/diagnostics.h>

#define DEBUG_GPUADVISOR 0
#define DEBUG_GPUADVISOR_DETAILS 0

#define MIN2(a, b) ((a > b) ? b : a)

namespace Analysis {

void GPUAdvisor::concatAdvise(const OptimizerRank &optimizer_rank) {
  size_t rank = 0;

  // TODO(Keren): use other formatters
  SimpleInspectionFormatter formatter;
  for (auto &iter : optimizer_rank) {
    double score = iter.first;

    if (score == 0.0) {
      // Do not output not effective optimizers
      break;
    }

    for (auto *optimizer : iter.second) {
      ++rank;

      _output << optimizer->advise(&formatter);

      if (rank >= _top_optimizers) {
        // Reach limit
        break;
      }
    }
  }
}

KernelStats GPUAdvisor::readKernelStats(int mpi_rank, int thread_id) {
  auto metric_index_blocks = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER:BLKS_ACUMU", false);
  auto metric_index_block_threads = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER:BLK_THR_ACUMU", false);
  auto metric_index_block_smem = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER:BLK_SMEM_ACUMU", false);
  auto metric_index_thread_reg = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER:THR_REG_ACUMU", false);
  auto metric_index_warps = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER:FGP_ACT_ACUMU", false);
  auto metric_index_time = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER (sec)", false);
  auto metric_index_count = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GKER:COUNT", false);
  auto metric_index_samples_dropped =
      _metric_name_prof_map->metric_id(mpi_rank, thread_id, "GSAMP:DRP", false);
  auto metric_index_samples_expected =
      _metric_name_prof_map->metric_id(mpi_rank, thread_id, "GSAMP:EXP", false);
  auto metric_index_samples_total =
      _metric_name_prof_map->metric_id(mpi_rank, thread_id, "GSAMP:TOT", false);
  auto metric_index_sample_frequency = _metric_name_prof_map->metric_id(
      mpi_rank, thread_id, "GSAMP:PER (cyc)", false);

  auto blocks = _gpu_kernel->metric(metric_index_blocks);
  auto block_threads = _gpu_kernel->metric(metric_index_block_threads);
  auto block_smem = _gpu_kernel->metric(metric_index_block_smem);
  auto thread_regs = _gpu_kernel->metric(metric_index_thread_reg);
  auto warps = _gpu_kernel->metric(metric_index_warps);
  auto samples_expected = _gpu_kernel->metric(metric_index_samples_expected);
  auto samples_dropped = _gpu_kernel->metric(metric_index_samples_dropped);
  auto samples_total = _gpu_kernel->metric(metric_index_samples_total);
  auto sample_frequency = _gpu_kernel->metric(metric_index_sample_frequency);
  auto time = _gpu_kernel->metric(metric_index_time);
  auto count = _gpu_kernel->metric(metric_index_count);

  blocks /= count;
  block_threads /= count;
  block_smem /= count;
  thread_regs /= count;
  warps /= count;
  time /= count;

  samples_total = samples_total - samples_dropped;
  samples_expected *= sample_frequency;
  samples_total *= sample_frequency;

  if (DEBUG_GPUADVISOR) {
    std::cout << "blocks: " << blocks << std::endl;
    std::cout << "block_threads: " << block_threads << std::endl;
    std::cout << "block_smem: " << block_smem << std::endl;
    std::cout << "thread_regs: " << thread_regs << std::endl;
    std::cout << "warps: " << warps << std::endl;
    std::cout << "samples_expected: " << samples_expected << std::endl;
    std::cout << "samples_dropped: " << samples_dropped << std::endl;
    std::cout << "samples_total: " << samples_total << std::endl;
    std::cout << "sample_frequency: " << sample_frequency << std::endl;
    std::cout << "time: " << time << std::endl;
    std::cout << "count: " << count << std::endl;
    std::cout << std::endl;
  }

  return KernelStats(blocks, block_threads, block_smem, thread_regs, warps, 0,
                     samples_total, samples_expected, count, 0, time);
}

void GPUAdvisor::advise(const CCTBlames &cct_blames) {
  // NOTE: stringstream.clear() does not work
  _output.str("");

  for (auto mpi_rank = 0; mpi_rank < _metric_name_prof_map->num_mpi_ranks();
       ++mpi_rank) {
    // For each MPI process
    for (auto thread_id = 0;
         thread_id < _metric_name_prof_map->num_thread_ids(mpi_rank);
         ++thread_id) {
      // For each CPU thread
      if (_metric_name_prof_map->metric_id(mpi_rank, thread_id, _inst_metric) ==
          -1) {
        // Skip tracing threads
        continue;
      }

      if (cct_blames.find(mpi_rank) != cct_blames.end()) {
        auto &mpi_blames = cct_blames.at(mpi_rank);

        if (mpi_blames.find(thread_id) != mpi_blames.end()) {
          KernelStats kernel_stats = readKernelStats(mpi_rank, thread_id);

          auto &kernel_blame = mpi_blames.at(thread_id);

          // 1. Summarize function statistics
          if (DEBUG_GPUADVISOR) {
            std::cout << "[" << mpi_rank << "," << thread_id << "]"
                      << std::endl;

            std::cout << "Lat: " << kernel_blame.lat_blame << std::endl;

            for (auto &lat_blame_iter : kernel_blame.lat_blames) {
              // Following lines, blame metrics
              auto lat = lat_blame_iter.first;
              auto lat_blame = lat_blame_iter.second;
              std::cout << lat << ": " << lat_blame << "("
                        << lat_blame / kernel_blame.lat_blame * 100 << "%)"
                        << std::endl;
            }

            std::cout << std::endl
                      << "Stall: " << kernel_blame.stall_blame << std::endl;

            for (auto &stall_blame_iter : kernel_blame.stall_blames) {
              // Following lines, blame metrics
              auto stall = stall_blame_iter.first;
              auto stall_blame = stall_blame_iter.second;
              std::cout << stall << ": " << stall_blame << "("
                        << stall_blame / kernel_blame.stall_blame * 100 << "%)"
                        << std::endl;
            }
          }

          // XXX(Keren): pc sample total sample info is inaccurate for small kernels
          // Calculate active samples
          kernel_stats.total_samples = kernel_blame.lat_blame;
          kernel_stats.active_samples =
              kernel_blame.lat_blame - kernel_blame.stall_blame;
          kernel_stats.sm_efficiency = kernel_stats.total_samples / kernel_stats.expected_samples;

          // 2. Rank optimizers
          OptimizerRank code_optimizer_rank;
          OptimizerRank parallel_optimizer_rank;
          OptimizerRank binary_optimizer_rank;

          for (auto *optimizer : _code_optimizers) {
            double score = optimizer->match(kernel_blame, kernel_stats);
            code_optimizer_rank[score].push_back(optimizer);
          }

          for (auto *optimizer : _parallel_optimizers) {
            double score = optimizer->match(kernel_blame, kernel_stats);
            parallel_optimizer_rank[score].push_back(optimizer);
          }

          for (auto *optimizer : _binary_optimizers) {
            double score = optimizer->match(kernel_blame, kernel_stats);
            binary_optimizer_rank[score].push_back(optimizer);
          }

          // 3. Output top advises
          _output << std::endl << "Code Optimizers" << std::endl << std::endl;
          concatAdvise(code_optimizer_rank);

          _output << std::endl
                  << "Parallel Optimizers" << std::endl
                  << std::endl;
          concatAdvise(parallel_optimizer_rank);

          _output << std::endl << "Binary Optimizers" << std::endl << std::endl;
          concatAdvise(binary_optimizer_rank);
        }
      }
    }
  }

  auto *gpu_kernel_struct = _vma_struct_map.begin()->second;
  auto *gpu_proc_struct = gpu_kernel_struct->ancestorProc();
  auto *gpu_file_struct = gpu_kernel_struct->ancestorFile();

  if (_output.rdbuf()->in_avail() != 0) {
    std::cout << _output.str();
  }

  // Clean advise
  for (auto *optimizer : _code_optimizers) {
    optimizer->clear();
  }

  for (auto *optimizer : _parallel_optimizers) {
    optimizer->clear();
  }

  for (auto *optimizer : _binary_optimizers) {
    optimizer->clear();
  }
}

} // namespace Analysis
