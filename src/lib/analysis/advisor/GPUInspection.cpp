
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

#include "GPUInspection.hpp"
#include "GPUOptimizer.hpp"

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

namespace Analysis {


std::string SimpleInspectionFormatter::format(const Inspection &inspection) {
  std::stringstream ss;

  // Overview
  ss << "Apply " << inspection.optimization << " optimization,";
  
  if (inspection.ratio != -1.0) {
    ss << " ratio " << inspection.ratio * 100 << "%,";
  } 

  if (inspection.speedup != -1.0) {
    ss << " estimate speedup " << inspection.speedup << "x";
  }

  ss << std::endl;

  // Specific suggestion
  if (inspection.warp_count.first != -1) {
    ss << "Adjust #warps from " << inspection.warp_count.first << " to "
      << inspection.warp_count.second << std::endl;
  }

  if (inspection.block_count.first != -1) {
    ss << "Adjust #blocks from " << inspection.block_count.first << " to "
      << inspection.block_count.second << std::endl;
  }

  if (inspection.reg_count.first != -1) {
    ss << "Adjust #regs from " << inspection.reg_count.first << " to "
      << inspection.reg_count.second << std::endl;
  }

  // Hot regions
  for (auto &inst_blame : inspection.top_regions) {
    ss << "Hot " << inst_blame.blame_name << " code (" << inst_blame.stall_blame << "):" << std::endl;

    auto *src_struct = inst_blame.src_struct;
    auto *dst_struct = inst_blame.dst_struct;
    auto *src_func = src_struct->ancestorProc();
    auto *dst_func = dst_struct->ancestorProc();
    auto src_vma = (inst_blame.src)->pc - src_func->vmaSet().begin()->beg();
    auto dst_vma = (inst_blame.dst)->pc - dst_func->vmaSet().begin()->beg();

    ss << "From " << src_struct->ancestorFile()->name() << ":" << src_struct->begLine() << std::endl;
    ss << std::hex << "0x" << src_vma << std::dec << " in Function " << src_func->name() << std::endl;

    ss << "To " << dst_struct->ancestorFile()->name() << ":" << dst_struct->begLine() << std::endl;
    ss << std::hex << "0x" << dst_vma << std::dec << " in Function " << src_func->name() << std::endl;
  }

  ss << std::endl;

  return ss.str();
};

}  // namespace analysis
