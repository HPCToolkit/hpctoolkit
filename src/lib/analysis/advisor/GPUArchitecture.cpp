//************************* System Include Files ****************************

#include <sys/stat.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <typeinfo>
#include <unordered_map>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "GPUArchitecture.hpp"

using std::string;

#include <lib/prof-lean/hpcrun-metric.h>
#include <lib/support/diagnostics.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>
#include <lib/cuda/DotCFG.hpp>
#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Metric-ADesc.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Struct-Tree.hpp>
#include <lib/profxml/PGMReader.hpp>
#include <lib/profxml/XercesUtil.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/xml/xml.hpp>

namespace Analysis {

// TODO(Keren): add more latency components
std::pair<int, int> V100::latency(const std::string &opcode) const {
  if (opcode.find("INTEGER") != std::string::npos) {
    if (opcode.find(".MAD") != std::string::npos) {
      return std::pair<int, int>(5, 5);
    } else if (opcode.find(".POPC") != std::string::npos) {
      return std::pair<int, int>(10, 10);
    } else {
      return std::pair<int, int>(4, 4);
    }
  } else if (opcode.find("PREDICATE") != std::string::npos) {
    return std::pair<int, int>(5, 5);
  } else if (opcode.find("CONVERT") != std::string::npos) {
    return std::pair<int, int>(14, 14);
  } else if (opcode.find("FLOAT") != std::string::npos) {
    if (opcode.find(".MUFU") != std::string::npos) {
      return std::pair<int, int>(14, 14);
    } else if (opcode.find(".16") != std::string::npos) {
      // TODO(Keren): tensor core
      return std::pair<int, int>(6, 6);
    } else if (opcode.find(".32") != std::string::npos) {
      return std::pair<int, int>(4, 4);
    } else if (opcode.find(".64") != std::string::npos) {
      return std::pair<int, int>(8, 8);
    }
  } else if (opcode.find("MEMORY") != std::string::npos) {
    if (opcode.find(".SHARED") != std::string::npos) {
      return std::pair<int, int>(19, 80);
    } else {
      // Hard to estimate memory latency
      // Use TLB miss latency
      // TODO(Keren): classify different level latencies
      return std::pair<int, int>(28, 1024);
    }
  }
  // Use pipeline latency
  // At least 4
  return std::pair<int, int>(4, 4);
}

// XXX: not throughput
int V100::issue(const std::string &opcode) const {
  if (opcode.find("INTEGER") != std::string::npos) {
    // 32 / 16 = 2
    return 2;
  } else if (opcode.find("FLOAT") != std::string::npos) {
    if (opcode.find(".MUFU") != std::string::npos) {
      // 32 / 4 = 8
      return 8;
    } else if (opcode.find(".16") != std::string::npos) {
      // 32 / 32 = 1
      // TODO(Keren): tensor core
      return 1;
    } else if (opcode.find(".32") != std::string::npos) {
      // 32 / 16 = 2
      return 2;
    } else if (opcode.find(".64") != std::string::npos) {
      // 32 / 8 = 4
      return 4;
    }
  } else if (opcode.find("MEMORY") != std::string::npos) {
    if (opcode.find(".32") != std::string::npos) {
      return 4;
    } else if (opcode.find(".64") != std::string::npos) {
      return 8;
    } else if (opcode.find(".128") != std::string::npos) {
      return 16;
    } else {
      return 4;
    }
  }
  // At least 2
  return 2;
}

// TODO(Keren): add more latency components
std::pair<int, int> A100::latency(const std::string &opcode) const {
  if (opcode.find("INTEGER") != std::string::npos) {
    if (opcode.find(".MAD") != std::string::npos) {
      return std::pair<int, int>(5, 5);
    } else if (opcode.find(".POPC") != std::string::npos) {
      return std::pair<int, int>(10, 10);
    } else {
      return std::pair<int, int>(4, 4);
    }
  } else if (opcode.find("PREDICATE") != std::string::npos) {
    return std::pair<int, int>(5, 5);
  } else if (opcode.find("CONVERT") != std::string::npos) {
    return std::pair<int, int>(14, 14);
  } else if (opcode.find("FLOAT") != std::string::npos) {
    if (opcode.find(".MUFU") != std::string::npos) {
      return std::pair<int, int>(14, 14);
    } else if (opcode.find(".16") != std::string::npos) {
      // TODO(Keren): tensor core
      return std::pair<int, int>(6, 6);
    } else if (opcode.find(".32") != std::string::npos) {
      return std::pair<int, int>(4, 4);
    } else if (opcode.find(".64") != std::string::npos) {
      return std::pair<int, int>(8, 8);
    }
  } else if (opcode.find("MEMORY") != std::string::npos) {
    if (opcode.find(".SHARED") != std::string::npos) {
      return std::pair<int, int>(19, 80);
    } else {
      // Hard to estimate memory latency
      // Use TLB miss latency
      // TODO(Keren): classify different level latencies
      return std::pair<int, int>(28, 1024);
    }
  }
  // TODO(Keren): uniform data path instructions
  // Use pipeline latency
  // At least 4
  return std::pair<int, int>(4, 4);
}

// XXX: issue latency
int A100::issue(const std::string &opcode) const {
  if (opcode.find("INTEGER") != std::string::npos) {
    // 32 / 16 = 2
    return 2;
  } else if (opcode.find("FLOAT") != std::string::npos) {
    if (opcode.find(".MUFU") != std::string::npos) {
      // 32 / 4 = 8
      return 8;
    } else if (opcode.find(".16") != std::string::npos) {
      // 32 / 32 = 1
      // TODO(Keren): tensor core
      return 1;
    } else if (opcode.find(".32") != std::string::npos) {
      // 32 / 16 = 2
      return 2;
    } else if (opcode.find(".64") != std::string::npos) {
      // 32 / 8 = 4
      return 4;
    }
  } else if (opcode.find("MEMORY") != std::string::npos) {
    if (opcode.find(".32") != std::string::npos) {
      return 4;
    } else if (opcode.find(".64") != std::string::npos) {
      return 8;
    } else if (opcode.find(".128") != std::string::npos) {
      return 16;
    } else {
      return 4;
    }
  }
  // At least 2
  return 2;
}

}  // namespace Analysis
