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

#define INST_LEN 16

namespace Analysis {

GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type) {
  GPUOptimizer *optimizer = NULL;

  switch (type) {
#define DECLARE_OPTIMIZER_CASE(TYPE, CLASS, VALUE) \
    case TYPE: { \
      optimizer = new CLASS( #CLASS ); \
      break; \
    }
    FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CASE)
#undef DECLARE_OPTIMIZER_CASE
    default:
      break;
  }

  return optimizer;
}


double GPURegisterIncreaseOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPURegisterIncreaseOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPURegisterDecreaseOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPURegisterDecreaseOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPULoopUnrollOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPULoopUnrollOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPULoopNoUnrollOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPULoopNoUnrollOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUStrengthReductionOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUStrengthReductionOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUWarpBalanceOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUWarpBalanceOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUCodeReorderOptimizer::match(const BlockBlame &block_blame) {
  // Match if for memory dep and exec dep
  std::stringstream ss;
  double blames = 0.0;

  ss << "Apply " << this->_name << " optimization" << std::endl;

  // TODO(Keren): init a template
  for (auto &blame_iter : block_blame.blames) {
    auto blame_name = blame_iter.first;

    if (blame_name.find("STL_GMEM_GMEM") != std::string::npos ||
      blame_name.find("STL_IDEP_DEP") != std::string::npos) {

      auto pairs = 0;
      // Find top latency pairs
      for (size_t i = 0; i < block_blame.inst_blames.size(); ++i) {
        auto &inst_blame = block_blame.inst_blames[i];
        if (inst_blame.blame_name == blame_name) {
          auto src_vma = inst_blame.src->pc;
          auto dst_vma = inst_blame.dst->pc;
          auto *src_struct =inst_blame.src_struct;
          auto *dst_struct =inst_blame.dst_struct;

          blames += inst_blame.blame;

          ss << "Hot " << blame_name << " code (" << inst_blame.blame << "):" << std::endl;

          ss << "From " << src_struct->ancestorFile()->name() << ":" << src_struct->begLine() <<
            std::hex << " 0x" << src_vma << std::dec << std::endl;

          ss << "To" << dst_struct->ancestorFile()->name() << ":" << dst_struct->begLine() <<
            std::hex << " 0x" << dst_vma << std::dec << std::endl << std::endl;

          if (++pairs == _top_pairs) {
            // Find all pairs
            break;
          }
        }
      }
    }
  }

  if (blames != 0.0) {
    // If find any match
    this->_match += ss.str();
  }

  return blames;
}


double GPUCodeReorderOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUKernelMergeOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUKernelMergeOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUFunctionInlineOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUFunctionInlineOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUSharedMemoryCoalesceOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUSharedMemoryCoalesceOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUGlobalMemoryCoalesceOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUGlobalMemoryCoalesceOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUOccupancyIncreaseOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUOccupancyIncreaseOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUOccupancyDecreaseOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUOccupancyDecreaseOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUSMBalanceOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUSMBalanceOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUBlockIncreaseOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUBlockIncreaseOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUBlockDecreaseOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


double GPUBlockDecreaseOptimizer::estimate(const BlockBlame &block_blame) {
  return 0.0;
}


}  // namespace Analysis
