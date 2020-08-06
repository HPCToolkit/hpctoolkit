
//************************* System Include Files ****************************

#include <fstream>
#include <iostream>

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

#include <algorithm>
#include <stack>
#include <typeinfo>
#include <unordered_map>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/gpu-metric-names.h>
#include <include/uint.h>

#include "GPUArchitecture.hpp"
#include "GPUEstimator.hpp"

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

#include <lib/xml/xml.hpp>

#include <lib/support/IOUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/diagnostics.h>

#include <iostream>
#include <vector>

#define MIN2(x, y) (x > y ? y : x)

namespace Analysis {

GPUEstimator *GPUEstimatorFactory(GPUArchitecture *arch, GPUEstimatorType type) {
  GPUEstimator *gpu_estimator = NULL;

  switch (type) {
    case SEQ:
      gpu_estimator = new SequentialGPUEstimator(arch);
      break;
    case SEQ_LAT:
      gpu_estimator = new SequentialLatencyGPUEstimator(arch);
      break;
    case PARALLEL:
      gpu_estimator = new ParallelGPUEstimator(arch);
      break;
    case PARALLEL_LAT:
      gpu_estimator = new ParallelLatencyGPUEstimator(arch);
      break;
    default:
      break;
  }

  return gpu_estimator;
}


std::pair<std::vector<double>, std::vector<double>>
SequentialGPUEstimator::estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats) {
  std::vector<double> estimate_ratios;
  std::vector<double> estimate_speedups;

  double blame = 0.0;

  // regional analysis
  for (auto &stats : blame_stats) {
    estimate_ratios.push_back(stats.blame / kernel_stats.total_samples);
    estimate_speedups.push_back(
        kernel_stats.total_samples / (kernel_stats.total_samples - stats.blame));
    blame += stats.blame;
  }

  estimate_ratios.push_back(blame / kernel_stats.total_samples);
  estimate_speedups.push_back(
    kernel_stats.total_samples / (kernel_stats.total_samples - blame));

  return std::make_pair(estimate_ratios, estimate_speedups);
}


std::pair<std::vector<double>, std::vector<double>>
SequentialLatencyGPUEstimator::estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats) {
  std::vector<double> estimate_ratios;
  std::vector<double> estimate_speedups;

  auto hidden_samples = 0.0;
  auto active_samples = 0.0;
  auto blame = 0.0;

  if (blame_stats.size() > 0) {
    active_samples = blame_stats.back().active_samples;
  }

  // regional analysis
  for (auto &stats : blame_stats) {
    estimate_ratios.push_back(stats.blame / kernel_stats.total_samples);
    estimate_speedups.push_back(
        kernel_stats.total_samples /
        (kernel_stats.total_samples - MIN2(stats.active_samples, stats.blame)));
    hidden_samples += MIN2(stats.active_samples, stats.blame);
    blame += stats.blame;
  }

  estimate_ratios.push_back(blame / kernel_stats.total_samples);
  estimate_speedups.push_back(
    kernel_stats.total_samples /
    (kernel_stats.total_samples - MIN2(active_samples, hidden_samples)));

  return std::make_pair(estimate_ratios, estimate_speedups);
}


std::pair<std::vector<double>, std::vector<double>>
ParallelGPUEstimator::estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats) {
  std::vector<double> estimate_ratios;
  std::vector<double> estimate_speedups;

  double blame = 0.0;
  double speedup = 1.0;
  if (blame_stats.size() > 0) {
    blame = blame_stats[0].blame;
    if (blame != 0) {
      speedup = 1 / blame;
    }
  }

  estimate_ratios.push_back(blame);
  estimate_speedups.push_back(speedup);

  return std::make_pair(estimate_ratios, estimate_speedups);
}


std::pair<std::vector<double>, std::vector<double>>
ParallelLatencyGPUEstimator::estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats) {
  std::vector<double> estimate_ratios;
  std::vector<double> estimate_speedups;

  double cur_warps = kernel_stats.active_warps;
  double max_warps = _arch->warps();
  double blame = 0.0;

  if (blame_stats.size() > 0) {
    blame = blame_stats[0].blame;
  }

  double expected_threads = (((int)kernel_stats.threads - 1) / _arch->warp_size() + 1) * _arch->warp_size();
  double thread_balance = expected_threads / kernel_stats.threads;

  if (cur_warps < _arch->schedulers()) {
    estimate_ratios.push_back(cur_warps / _arch->schedulers());
    estimate_speedups.push_back(_arch->schedulers() / cur_warps * thread_balance);
  } else {
    double issue = blame / static_cast<double>(kernel_stats.total_samples);
    double warp_issue = 1 - pow(1 - issue, cur_warps / _arch->schedulers());
    double new_warp_issue = 1 - pow(1 - issue, max_warps / _arch->schedulers());

    estimate_ratios.push_back(1 - warp_issue);
    estimate_speedups.push_back(new_warp_issue / warp_issue * thread_balance);
  }

  return std::make_pair(estimate_ratios, estimate_speedups);
}

} // namespace Analysis
