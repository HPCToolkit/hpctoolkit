//************************* System Include Files ****************************

#include <fstream>
#include <iostream>

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

#include <algorithm>
#include <typeinfo>
#include <unordered_map>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>
#include <include/uint.h>

#include "GPUAdvisor.hpp"
#include "GPUEstimator.hpp"
#include "GPUOptimizer.hpp"

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

#include <lib/cuda/AnalyzeInstruction.hpp>

#include <iostream>
#include <vector>

#define MIN2(x, y) (x > y ? y : x)
#define BLAME_GPU_INST_METRIC_NAME "BLAME " GPU_INST_METRIC_NAME

namespace Analysis {

GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type, GPUArchitecture *arch) {
  GPUOptimizer *optimizer = NULL;

  switch (type) {
#define DECLARE_OPTIMIZER_CASE(TYPE, CLASS, VALUE)                             \
  case TYPE: {                                                                 \
    optimizer = new CLASS(#CLASS, arch);                                       \
    break;                                                                     \
  }
    FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CASE)
#undef DECLARE_OPTIMIZER_CASE
  default:
    break;
  }

  return optimizer;
}


double GPUOptimizer::match(const KernelBlame &kernel_blame,
                           const KernelStats &kernel_stats) {
  // Match if for local memory dep
  _inspection.optimization = this->_name;
  double blame = this->match_impl(kernel_blame, kernel_stats);
  std::tie(_inspection.ratio, _inspection.speedup) =
      _estimator->estimate(blame, kernel_stats);
  return _inspection.speedup;
}


double
GPURegisterIncreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                         const KernelStats &kernel_stats) {
  // Match if for local memory dep
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto &blame_name = inst_blame->blame_name;

    if (blame_name == BLAME_GPU_INST_METRIC_NAME ":LAT_GMEM_LMEM") {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->stall_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double
GPURegisterDecreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                         const KernelStats &kernel_stats) {
  // Match if occupancy is low
  // TODO: if register is the limitation factor of occupancy
  // increase it to increase occupancy

  // TODO: Extend use liveness analysis to find high pressure regions
  return 0.0;
}


double GPULoopUnrollOptimizer::match_impl(const KernelBlame &kernel_blame,
                                          const KernelStats &kernel_stats) {
  // Match if inst_blame->from and inst_blame->to are in the same loop
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs that are in the same loop
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    auto &blame_name = inst_blame->blame_name;

    if (blame_name.find("LAT_DEP") != std::string::npos ||
        blame_name.find("LAT_GMEM") != std::string::npos ||
        blame_name.find("LAT_SYNC") != std::string::npos) {
      auto *src_struct = inst_blame->src_struct;
      auto *dst_struct = inst_blame->dst_struct;

      if (src_struct->ancestorLoop() != NULL &&
          dst_struct->ancestorLoop() != NULL &&
          src_struct->ancestorLoop() == dst_struct->ancestorLoop()) {
        _inspection.top_regions.push_back(*inst_blame);
        blame += inst_blame->stall_blame;

        if (++insts >= _top_regions) {
          break;
        }
      }
    }
  }

  return blame;
}


double GPULoopNoUnrollOptimizer::match_impl(const KernelBlame &kernel_blame,
                                            const KernelStats &kernel_stats) {
  // Match if instruction fetch latency
  auto blame = 0.0;
  auto insts = 0;

  if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") !=
      kernel_blame.stall_blames.end()) {
    blame = kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET");
  }

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    auto &blame_name = inst_blame->blame_name;

    if (blame_name == BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->stall_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double
GPUStrengthReductionOptimizer::match_impl(const KernelBlame &kernel_blame,
                                          const KernelStats &kernel_stats) {
  // Match if for exec dep
  const int LAT_UPPER = 10;
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *src_inst = inst_blame->src_inst;

    if (_arch->latency(src_inst->op).first >= LAT_UPPER &&
        src_inst->op.find("MEMORY") == std::string::npos) {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->lat_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double GPUWarpBalanceOptimizer::match_impl(const KernelBlame &kernel_blame,
                                           const KernelStats &kernel_stats) {
  // Match if sync stall
  auto blame = 0.0;
  auto insts = 0;

  if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_SYNC") !=
      kernel_blame.stall_blames.end()) {
    blame = kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_SYNC");
  }

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_SYNC") != std::string::npos) {
      _inspection.top_regions.push_back(*inst_blame);
      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double GPUCodeReorderOptimizer::match_impl(const KernelBlame &kernel_blame,
                                           const KernelStats &kernel_stats) {
  // Match if for memory dep and exec dep
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_GMEM_GMEM") != std::string::npos ||
        inst_blame->blame_name.find(":LAT_IDEP_DEP") != std::string::npos) {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->stall_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double GPUKernelMergeOptimizer::match_impl(const KernelBlame &kernel_blame,
                                           const KernelStats &kernel_stats) {
  // Match if ifetch and small kernel invoked many times
  const int KERNEL_COUNT_LIMIT = 10;
  const double KERNEL_TIME_LIMIT = 100 * 1e-6; // 100us

  auto blame = 0.0;
  if (kernel_stats.time <= KERNEL_TIME_LIMIT &&
    kernel_stats.count >= KERNEL_COUNT_LIMIT) {
    if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") !=
      kernel_blame.stall_blames.end()) {
      blame += kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET");
    }
  }

  return blame;
}


double GPUFunctionInlineOptimizer::match_impl(const KernelBlame &kernel_blame,
                                              const KernelStats &kernel_stats) {
  // Match if ifetch and device function
  auto blame = 0.0;
  auto insts = 0;

  // Match if latency in function epilogue and prologues, and device function
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *function = inst_blame->src_function;
    if (function->global == false) {
      auto *src_struct = inst_blame->src_struct;
      auto *src_inst = inst_blame->src_inst;
      auto *dst_struct = inst_blame->dst_struct;
      auto *dst_inst = inst_blame->dst_inst;
      if (dst_struct->begLine() == dst_struct->ancestorProc()->begLine() ||
          src_struct->begLine() == src_struct->ancestorProc()->begLine()) {
        // prologue STL
        if (dst_inst->op.find("STORE.LOCAL") != std::string::npos) {
          blame += inst_blame->lat_blame;

          if (++insts >= _top_regions) {
            break;
          }
        }
      } else if (dst_struct->begLine() == dst_struct->ancestorProc()->endLine()) {
        // epilogue LD
        // TODO interprocedural attribution
        if (dst_inst->op.find("LOAD.LOCAL") != std::string::npos) {
          blame += inst_blame->lat_blame;

          if (++insts >= _top_regions) {
            break;
          }
        }
      }
    }
  }

  return blame;
}


double GPUFunctionSplitOptimizer::match_impl(const KernelBlame &kernel_blame,
                                             const KernelStats &kernel_stats) {
  // Match if ifetch and device function
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_IFET") != std::string::npos) {
      auto *src_struct = inst_blame->src_struct;
      if (src_struct->ancestorAlien() != NULL) {
        _inspection.top_regions.push_back(*inst_blame);
        blame += inst_blame->stall_blame;

        if (++insts >= _top_regions) {
          break;
        }
      }
    }
  }

  return blame;
}


double
GPUSharedMemoryCoalesceOptimizer::match_impl(const KernelBlame &kernel_blame,
                                             const KernelStats &kernel_stats) {
  // Match if shared memory latency is high
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_IDEP_SMEM") != std::string::npos) {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->stall_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double
GPUGlobalMemoryCoalesceOptimizer::match_impl(const KernelBlame &kernel_blame,
                                             const KernelStats &kernel_stats) {
  // Match if global memory latency is high
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_GMEM_GMEM") != std::string::npos) {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->stall_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double
GPUOccupancyIncreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                          const KernelStats &kernel_stats) {
  auto blame = 0.0;

  if (kernel_blame.stall_blame != 0.0) {
    _inspection.active_warp_count = kernel_stats.active_warps;

    for (auto &lat_blame_iter : kernel_blame.lat_blames) {
      auto blame_name = lat_blame_iter.first;
      auto blame_metric = lat_blame_iter.second;

      if (blame_name.find(":LAT_NONE") != std::string::npos ||
          blame_name.find(":LAT_NSEL") != std::string::npos) {
        blame += blame_metric;
      }
    }
  }

  return blame;
}


double
GPUOccupancyDecreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                          const KernelStats &kernel_stats) {
  // Match if not selected if high
  auto blame = 0.0;
  auto insts = 0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_NSEL") != std::string::npos) {
      _inspection.top_regions.push_back(*inst_blame);
      blame += inst_blame->stall_blame;

      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  return blame;
}


double GPUSMBalanceOptimizer::match_impl(const KernelBlame &kernel_blame,
                                         const KernelStats &kernel_stats) {
  // Match if blocks are large while SM efficiency is low
  auto blame = kernel_stats.sm_efficiency;

  return blame;
}


double GPUBlockIncreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                             const KernelStats &kernel_stats) {
  int cur_blocks = kernel_stats.blocks;
  int sms = this->_arch->sms();

  _inspection.block_count = cur_blocks;
  double balanced_blocks = ((cur_blocks - 1) / sms + 1) * sms;
  auto blame = cur_blocks / balanced_blocks;

  return blame;
}


double GPUBlockDecreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                             const KernelStats &kernel_stats) {
  // Match if threads are few, increase number of threads per block. i.e.
  // threads coarsen
  const int WARP_COUNT_LIMIT = 2;

  auto blame = 0.0;
  auto warps = (kernel_stats.threads - 1) / _arch->warp_size() + 1;

  _inspection.block_count = kernel_stats.blocks;
  _inspection.thread_count = kernel_stats.threads;

  // blocks are enough, while threads are few
  // Concurrent (not synchronized) blocks may introduce instruction cache
  // latency Reduce the number of blocks keep warps fetch the same instructions
  // at every cycle
  if (kernel_stats.blocks > _arch->sms() && warps <= WARP_COUNT_LIMIT) {
    if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME":LAT_IFET") !=
        kernel_blame.stall_blames.end()) {
      blame += kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME":LAT_IFET");
    }
  }

  return blame;
}

} // namespace Analysis
