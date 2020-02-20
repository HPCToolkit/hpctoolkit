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

#include "CallPath-CudaOptimizer.hpp"
#include "CallPath-CudaAdvisor.hpp"

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

namespace CallPath {

CudaOptimizer *CudaOptimizerFactory(CudaOptimizerType type) {
  CudaOptimizer *optimizer = NULL;

  switch (type) {
    case LOOP_UNROLL: {
      optimizer = new CudaLoopUnrollOptimizer();
      break;
    }
    case MEMORY_LAYOUT: {
      optimizer = new CudaMemoryLayoutOptimizer();
      break;
    }
    case STRENGTH_REDUCTION: {
      optimizer = new CudaStrengthReductionOptimizer();
      break;
    }
    case ADJUST_THREADS: {
      optimizer = new CudaAdjustThreadsOptimizer();
      break;
    }
    case ADJUST_REGISTERS: {
      optimizer = new CudaAdjustRegistersOptimizer();
      break;
    }
    default:
      break;
  }

  return optimizer;
}



double CudaLoopUnrollOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string CudaLoopUnrollOptimizer::advise() {
  std::string ret;
  return ret;
}


double CudaMemoryLayoutOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string CudaMemoryLayoutOptimizer::advise() {
  std::string ret;
  return ret;
}


double CudaStrengthReductionOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string CudaStrengthReductionOptimizer::advise() {
  std::string ret;
  return ret;
}


double CudaAdjustThreadsOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string CudaAdjustThreadsOptimizer::advise() {
  std::string ret;
  return ret;
}


double CudaAdjustRegistersOptimizer::match(const BlockBlame &block_blame) {
  return 0.0;
}


std::string CudaAdjustRegistersOptimizer::advise() {
  std::string ret;
  return ret;
}

}  // namespace CallPath

}  // namespace Analysis
