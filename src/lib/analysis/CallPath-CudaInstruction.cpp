//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>

#include <typeinfo>
#include <unordered_map>

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

#include <vector>
#include <iostream>

#define DEBUG_CALLPATH_CUDAINSTRUCTION 1

namespace Analysis {

namespace CallPath {

static int gpu_isample_index = -1;

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
  // Read instruction files

  // Create new metrics

  // Iterate over the prof-tree and lay over static metrics
}


}

}
