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

namespace Analysis {

GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type) {
  GPUOptimizer *optimizer = NULL;

  switch (type) {
    case LOOP_UNROLL: {
      optimizer = new GPULoopUnrollOptimizer();
      break;
    }
    case MEMORY_LAYOUT: {
      optimizer = new GPUMemoryLayoutOptimizer();
      break;
    }
    case STRENGTH_REDUCTION: {
      optimizer = new GPUStrengthReductionOptimizer();
      break;
    }
    case ADJUST_THREADS: {
      optimizer = new GPUAdjustThreadsOptimizer();
      break;
    }
    case ADJUST_REGISTERS: {
      optimizer = new GPUAdjustRegistersOptimizer();
      break;
    }
    default:
      break;
  }

  return optimizer;
}



double GPULoopUnrollOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string GPULoopUnrollOptimizer::advise() {
  std::string ret;
  return ret;
}


double GPUMemoryLayoutOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string GPUMemoryLayoutOptimizer::advise() {
  std::string ret;
  return ret;
}


double GPUStrengthReductionOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string GPUStrengthReductionOptimizer::advise() {
  std::string ret;
  return ret;
}


double GPUAdjustThreadsOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string GPUAdjustThreadsOptimizer::advise() {
  std::string ret;
  return ret;
}


double GPUAdjustRegistersOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string GPUAdjustRegistersOptimizer::advise() {
  std::string ret;
  return ret;
}

}  // namespace Analysis
