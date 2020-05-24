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

#include "GPUOptimizer.hpp"
#include "GPUAdvisor.hpp"

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


#define MIN2(x, y) (x > y ? y : x)

namespace Analysis {

GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type, GPUArchitecture *arch) {
  GPUOptimizer *optimizer = NULL;

  switch (type) {
#define DECLARE_OPTIMIZER_CASE(TYPE, CLASS, VALUE) \
    case TYPE: { \
      optimizer = new CLASS( #CLASS , arch); \
      break; \
    }
    FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CASE)
#undef DECLARE_OPTIMIZER_CASE
    default:
      break;
  }

  return optimizer;
}


double GPURegisterIncreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPURegisterDecreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPULoopUnrollOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPULoopNoUnrollOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUStrengthReductionOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if for exec dep
  double blame = 0.0;

  _inspection.optimization = this->_name;

  auto insts = 0;
  // Find top latency pairs
  for (auto &inst_blame : kernel_blame.lat_inst_blames) {
    auto *src_inst = inst_blame.src;

    if (_arch->latency(src_inst->op).first >= 10 && src_inst->op.find("MEMORY") == std::string::npos) {
      blame += inst_blame.lat_blame;

      if (++insts <= _top_regions) {
        _inspection.top_regions.push_back(inst_blame);
      }
    }
  }

  if (blame != 0.0) {
    _inspection.ratio = blame / kernel_stats.total_samples;
    _inspection.speedup = kernel_stats.total_samples / (kernel_stats.total_samples - blame);
  }

  return _inspection.speedup;
}


double GPUWarpBalanceOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if sync stall
  _inspection.optimization = this->_name;

  auto blame = 0.0;
  for (auto &stall_blame_iter : kernel_blame.stall_blames) {
    auto blame_name = stall_blame_iter.first;
    auto blame_metric = stall_blame_iter.second;

    if (blame_name.find(":LAT_SYNC") != std::string::npos) {
      blame += blame_metric;
    }
  }

  auto insts = 0;
  // Find top latency pairs
  for (auto &inst_blame : kernel_blame.stall_inst_blames) {
    if (inst_blame.blame_name.find(":LAT_SYNC") != std::string::npos) {
      _inspection.top_regions.push_back(inst_blame);
      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  if (blame != 0.0) {
    _inspection.ratio = blame / kernel_stats.total_samples;
    _inspection.speedup = kernel_stats.total_samples / (kernel_stats.total_samples -
      MIN2(kernel_stats.active_samples, blame));
  }

  return _inspection.speedup;
}


double GPUCodeReorderOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if for memory dep and exec dep
  _inspection.optimization = this->_name;

  auto blame = 0.0;
  for (auto &stall_blame_iter : kernel_blame.stall_blames) {
    auto blame_name = stall_blame_iter.first;
    auto blame_metric = stall_blame_iter.second;

    if (blame_name.find(":LAT_GMEM_GMEM") != std::string::npos ||
      blame_name.find(":LAT_IDEP_DEP") != std::string::npos) {
      blame += blame_metric;
    }
  }

  auto insts = 0;
  // Find top latency pairs
  for (auto &inst_blame : kernel_blame.stall_inst_blames) {
    if (inst_blame.blame_name.find(":LAT_GMEM_GMEM") != std::string::npos ||
      inst_blame.blame_name.find(":LAT_IDEP_DEP") != std::string::npos) {
      _inspection.top_regions.push_back(inst_blame);
      if (++insts >= _top_regions) {
        break;
      }
    }
  }

  if (blame != 0.0) {
    // TODO(Keren) nest the latency hiding region to a loop
    _inspection.ratio = blame / kernel_stats.total_samples;
    _inspection.speedup = kernel_stats.total_samples / (kernel_stats.total_samples -
      MIN2(kernel_stats.active_samples, blame));
  }

  return _inspection.speedup;
}


double GPUKernelMergeOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUFunctionInlineOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUSharedMemoryCoalesceOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUGlobalMemoryCoalesceOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUOccupancyIncreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  _inspection.optimization = this->_name;

  double cur_warps = kernel_stats.active_warps;
  double max_warps  = this->_arch->warps();

  if (kernel_blame.stall_blame != 0.0) {
    _inspection.warp_count.first = cur_warps;
    if (cur_warps < _arch->schedulers()) {
      _inspection.warp_count.second = _arch->schedulers();
      _inspection.ratio = cur_warps / _arch->schedulers();
      _inspection.speedup = _arch->schedulers() / cur_warps;
    } else {
      double new_warps = MIN2(cur_warps * 2, max_warps);
      _inspection.warp_count.second = new_warps;

      double blame = 0.0;
      for (auto &lat_blame_iter : kernel_blame.lat_blames) {
        auto blame_name = lat_blame_iter.first;
        auto blame_metric = lat_blame_iter.second;

        if (blame_name.find(":LAT_NONE") != std::string::npos ||
          blame_name.find(":LAT_NSEL") != std::string::npos) {
          blame += blame_metric;
        }
      }

      double issue = blame / static_cast<double>(kernel_stats.total_samples);
      double warp_issue = 1 - pow(1 - issue, cur_warps / _arch->schedulers());
      double new_warp_issue = 1 - pow(1 - issue, new_warps / _arch->schedulers());

      _inspection.ratio = 1 - warp_issue;
      _inspection.speedup = new_warp_issue / warp_issue;
    }
  }

  return _inspection.speedup;
}


double GPUOccupancyDecreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUSMBalanceOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUBlockIncreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  _inspection.optimization = this->_name;

  int cur_blocks = kernel_stats.blocks;
  int sms = this->_arch->sms();

  double balanced_blocks = ((cur_blocks - 1) / sms + 1) * sms;
  _inspection.ratio = cur_blocks / balanced_blocks;
  _inspection.speedup = balanced_blocks / cur_blocks;

  _inspection.block_count.first = cur_blocks;
  _inspection.block_count.second = balanced_blocks;

  return _inspection.speedup;
}


double GPUBlockDecreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


}  // namespace Analysis
