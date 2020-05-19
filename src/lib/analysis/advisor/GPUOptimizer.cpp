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
  return 0.0;
}


double GPUWarpBalanceOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUCodeReorderOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if for memory dep and exec dep
  std::stringstream output;
  double speedup = 0.0;
  double blame = 0.0;

  // TODO(Keren): init a template
  for (auto &blame_iter : kernel_blame.blames) {
    auto blame_name = blame_iter.first;
    auto blame_metric = blame_iter.second;

    if (blame_name.find("STL_GMEM_GMEM") != std::string::npos ||
      blame_name.find("STL_IDEP_DEP") != std::string::npos) {
      blame += blame_metric;

      auto pairs = 0;
      // Find top latency pairs
      for (size_t i = 0; i < kernel_blame.inst_blames.size(); ++i) {
        auto &inst_blame = kernel_blame.inst_blames[i];
        if (inst_blame.blame_name == blame_name) {
          auto src_vma = inst_blame.src->pc;
          auto dst_vma = inst_blame.dst->pc;
          auto *src_struct =inst_blame.src_struct;
          auto *dst_struct =inst_blame.dst_struct;

          output << "Hot " << blame_name << " code (" << inst_blame.blame << "):" << std::endl;

          output << "From " << src_struct->ancestorFile()->name() << ":" << src_struct->begLine() <<
            std::hex << " 0x" << src_vma << std::dec << std::endl;

          output << "To" << dst_struct->ancestorFile()->name() << ":" << dst_struct->begLine() <<
            std::hex << " 0x" << dst_vma << std::dec << std::endl << std::endl;

          if (++pairs == _top_pairs) {
            // Find all pairs
            break;
          }
        }
      }
    }
  }

  if (blame != 0.0) {
    std::stringstream ss;
    double ratio = blame / kernel_stats.total_samples * 100;
    speedup = kernel_stats.total_samples / (kernel_stats.total_samples -
      MIN2(kernel_stats.active_warps, blame));
    // If find any match
    ss << "Apply " << this->_name << " optimization (" << std::to_string(ratio)
       << "%), estimate speedup " << speedup << "x" << std::endl;
    this->_advise += ss.str();
    this->_advise += output.str();
  }

  return speedup;
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
  double speedup = 0.0;
  // Match if for memory dep and exec dep
  std::stringstream output;

  double cur_warps = kernel_stats.active_warps;
  double max_warps = this->_arch->warps();

  if (kernel_blame.blame != 0.0) {
    double ratio = cur_warps / max_warps;
    double issue = kernel_stats.active_samples / static_cast<double>(kernel_stats.total_samples);
    double new_issue = 1 - pow(1 - issue, max_warps / cur_warps);
    speedup = new_issue / issue;
    // If find any match
    output << "Apply " << this->_name << " optimization (" << std::to_string(ratio)
       << "%), estimate speedup " << speedup << "x" << std::endl;
    output << "Increase #active_warps from " << cur_warps << " to " << max_warps << std::endl;
    this->_advise += output.str();
  }

  return speedup;
}


double GPUOccupancyDecreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUSMBalanceOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUBlockIncreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


double GPUBlockDecreaseOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  return 0.0;
}


}  // namespace Analysis
