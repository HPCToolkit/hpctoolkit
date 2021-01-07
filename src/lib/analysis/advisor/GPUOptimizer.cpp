//************************* System Include Files ****************************

#include <sys/stat.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <typeinfo>
#include <unordered_map>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/gpu-metric-names.h>
#include <include/uint.h>

#include "GPUAdvisor.hpp"
#include "GPUEstimator.hpp"
#include "GPUOptimizer.hpp"

using std::string;

#include <lib/prof-lean/hpcrun-metric.h>
#include <lib/support/diagnostics.h>

#include <iostream>
#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>
#include <lib/cuda/AnalyzeInstruction.hpp>
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
#include <vector>

#define MIN2(x, y) (x > y ? y : x)
#define BLAME_GPU_INST_METRIC_NAME "BLAME " GPU_INST_METRIC_NAME

namespace Analysis {

GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type, GPUArchitecture *arch) {
  GPUOptimizer *optimizer = NULL;

  switch (type) {
#define DECLARE_OPTIMIZER_CASE(TYPE, CLASS, VALUE) \
  case TYPE: {                                     \
    optimizer = new CLASS(#CLASS, arch);           \
    break;                                         \
  }
    FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CASE)
#undef DECLARE_OPTIMIZER_CASE
    default:
      break;
  }

  return optimizer;
}

double GPUOptimizer::match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Debugging
  // std::cout << "optimizer " << this->_name << std::endl;
  _inspection.clear();
  _inspection.optimization = this->_name;
  _inspection.total = kernel_stats.total_samples;
  _inspection.callback = NULL;

  // Regional blame and stats
  std::vector<BlameStats> blame_stats = this->match_impl(kernel_blame, kernel_stats);

  std::tie(_inspection.ratios, _inspection.speedups) =
      _estimator->estimate(blame_stats, kernel_stats);

  return _inspection.speedups.back();
}

std::vector<BlameStats> GPURegisterIncreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                 const KernelStats &kernel_stats) {
  // Match if for local memory dep
  std::vector<BlameStats> blame_stats_vec;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto &blame_name = inst_blame->blame_name;

    if (blame_name == BLAME_GPU_INST_METRIC_NAME ":LAT_GMEM_LMEM") {
      BlameStats blame_stats(inst_blame->lat_blame, kernel_stats.active_samples,
                             kernel_stats.total_samples);
      blame_stats_vec.push_back(blame_stats);

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.hint =
      "Too many registers are allocated in the kernel that exceeds the upper bound. Register "
      "spills to local memory causes extra instruction cycles.\n"
      "    To eliminate these cycles:\n"
      "    1. Increase the upper bound of regster count in a kernel.\n"
      "    2. Simplify computations to reuse registers.\n"
      "    3. Split loops to reduce register usage.\n";

  _inspection.stall = false;
  _inspection.loop = true;

  return blame_stats_vec;
}

std::vector<BlameStats> GPURegisterDecreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                 const KernelStats &kernel_stats) {
  // Match if occupancy is low
  // TODO: if register is the limitation factor of occupancy
  // increase it to increase occupancy

  // TODO: Extend use liveness analysis to find high pressure regions
  return std::vector<BlameStats>{BlameStats{}};
}

std::vector<BlameStats> GPULoopUnrollOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                           const KernelStats &kernel_stats) {
  // Match if inst_blame->from and inst_blame->to are in the same loop
  std::map<Prof::Struct::ACodeNode *, BlameStats> region_stats;
  auto kernel_active_samples = 0.0;

  // Find top latency pairs that are in the same loop
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    auto *src_struct = inst_blame->src_struct;
    auto *dst_struct = inst_blame->dst_struct;

    if (src_struct->ancestorLoop() != NULL && dst_struct->ancestorLoop() != NULL &&
        src_struct->ancestorLoop() == dst_struct->ancestorLoop()) {
      Prof::Struct::ACodeNode *region = inst_blame->src_struct->ancestorLoop();
      while (region != NULL) {
        region_stats[region].total_samples += inst_blame->lat_blame;
        region_stats[region].active_samples += inst_blame->lat_blame - inst_blame->stall_blame;
        if (region->parent() != NULL) {
          region = region->parent()->ancestorLoop();
        } else {
          break;
        }
      }
      kernel_active_samples += inst_blame->lat_blame - inst_blame->stall_blame;
      region = inst_blame->src_struct->ancestorLoop();

      auto &blame_name = inst_blame->blame_name;
      if (blame_name.find(":LAT_IDEP") != std::string::npos ||
          blame_name.find(":LAT_GMEM") != std::string::npos) {
        region_stats[region].blame += inst_blame->stall_blame;
      }
    }
  }

  _inspection.hint =
      "Instructions within loops form long dependency edges and cause arithmetic, memory, and "
      "stall latencies.\n"
      "    To eliminate these stalls:\n"
      "    1. Unroll the loops several times with pragma unroll or manual unrolling. Sometimes the "
      "compiler fails to automatically unroll loops\n"
      "    2. Use vector instructions to achieve higher dependency (e.g., float4 load/store, "
      "tensor "
      "operations).";
  _inspection.stall = true;
  _inspection.loop = true;

  std::vector<BlameStats> blame_stats_vec;
  for (auto &iter : region_stats) {
    blame_stats_vec.push_back(iter.second);

    InstructionBlame region_blame;
    region_blame.src_struct = iter.first;
    region_blame.dst_struct = iter.first;
    region_blame.blame_name = BLAME_GPU_INST_METRIC_NAME ":LAT_DEP";
    region_blame.stall_blame = iter.second.blame;
    _inspection.regions.push_back(region_blame);
  }

  std::sort(blame_stats_vec.begin(), blame_stats_vec.end());
  std::sort(_inspection.regions.begin(), _inspection.regions.end(),
            [](const InstructionBlame &l, const InstructionBlame &r) {
              return l.stall_blame > r.stall_blame;
            });

  if (_inspection.regions.size() > _top_regions) {
    _inspection.regions.erase(_inspection.regions.begin() + _top_regions,
                              _inspection.regions.end());
  }

  blame_stats_vec.push_back(BlameStats(0.0, kernel_active_samples, 0.0));

  return blame_stats_vec;
}

std::vector<BlameStats> GPULoopNoUnrollOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                             const KernelStats &kernel_stats) {
  auto blame = 0.0;
  // Find top instruction fetch latencies
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto &blame_name = inst_blame->blame_name;

    Prof::Struct::ACodeNode *region = inst_blame->src_struct->ancestorLoop();
    if (region != NULL && blame_name == BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") {
      blame += inst_blame->lat_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.hint =
      "Large loops are unrolled aggressively.\n"
      "    To eliminate these stalls:\n"
      "    1. Manually unroll loops to reduce redundant instructions.\n"
      "    2. Reduce loop unroll factor.";
  _inspection.stall = false;
  _inspection.loop = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUStrengthReductionOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                  const KernelStats &kernel_stats) {
  // Match if for exec dep
  const int LAT_UPPER = 10;
  std::vector<BlameStats> blame_stats_vec;

  // Find top non-memory latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *src_inst = inst_blame->src_inst;

    if (_arch->latency(src_inst->op).first >= LAT_UPPER &&
        src_inst->op.find("MEMORY") == std::string::npos) {
      BlameStats blame_stats(inst_blame->lat_blame, kernel_stats.active_samples,
                             kernel_stats.total_samples);
      blame_stats_vec.push_back(blame_stats);

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.hint =
      "Long latency non-memory instructions are used. Look for improvements that are "
      "mathematically equivalent but the compiler is not intelligent to do so.\n"
      "    1. Avoid integer division. An integer division requires the usage of SFU to "
      "perform floating point transforming. One can use a multiplication of reciprocal instead.\n"
      "    2. Avoid conversion. A float constant by default is 64-bit. If the constant is "
      "multiplied by a 32-bit "
      "float value, the compiler transforms the 32-bit value to a 64-bit value first.\n";
  _inspection.stall = false;
  _inspection.loop = true;

  return blame_stats_vec;
}

std::vector<BlameStats> GPUWarpBalanceOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                            const KernelStats &kernel_stats) {
  // Match if sync latency
  std::vector<BlameStats> blame_stats_vec;

  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_SYNC") != std::string::npos) {
      BlameStats blame_stats(inst_blame->lat_blame, kernel_stats.active_samples,
                             kernel_stats.total_samples);
      blame_stats_vec.push_back(blame_stats);

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.hint =
      "Threads within the same block are waiting for all to synchronize after a barrier "
      "instruction (e.g., __syncwarp or __threadfence). To reduce sync stalls:\n"
      "    1. Try to balance the work before synchronization points.\n"
      "    2. Use warp shuffle operations to reduce synchronization cost.\n"
      "    3. On new GPU architectures, looking for splitting arrive/wait barrier to enable"
      "    hardware acceleration.\n";
  _inspection.stall = false;
  _inspection.loop = true;

  return blame_stats_vec;
}

std::vector<BlameStats> GPUCodeReorderOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                            const KernelStats &kernel_stats) {
  // Match if for memory dep and exec dep
  std::map<Prof::Struct::ACodeNode *, BlameStats> region_stats;
  std::map<Prof::Struct::ACodeNode *, std::vector<InstructionBlame *>> region_blames;
  auto blame = 0.0;
  // auto DIST_UPPER = 18;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    Prof::Struct::ACodeNode *region = inst_blame->src_struct->ancestorLoop();
    while (region != NULL) {
      region_stats[region].total_samples += inst_blame->lat_blame;
      region_stats[region].active_samples += inst_blame->lat_blame - inst_blame->stall_blame;
      if (region->parent() != NULL) {
        region = region->parent()->ancestorLoop();
      } else {
        break;
      }
    }
    region = inst_blame->src_struct->ancestorProc();
    region_stats[region].total_samples += inst_blame->lat_blame;
    region_stats[region].active_samples += inst_blame->lat_blame - inst_blame->stall_blame;

    region = inst_blame->src_struct->ancestorLoop();
    if (region == NULL) {
      region = inst_blame->src_struct->ancestorProc();
    }

    if (inst_blame->blame_name.find(":LAT_GMEM") != std::string::npos ||
        inst_blame->blame_name.find(":LAT_IDEP") != std::string::npos) {
      blame += inst_blame->stall_blame;
      region_stats[region].blame += inst_blame->stall_blame;
      region_blames[region].push_back(inst_blame);
    }
  }

  _inspection.hint =
      "Compiler fails to schedule instructions properly to hide latencies.\n"
      "    To reduce latency: \n"
      "    1. Reorder statements manually. For example, if a memory load latency is outstanding, "
      "the "
      "load can be put a few lines before the first usage\n";
  _inspection.stall = true;
  // TODO(Keren): nested regions
  _inspection.loop = true;

  std::vector<BlameStats> blame_stats_vec;
  for (auto &iter : region_blames) {
    auto *region = iter.first;
    auto &region_blame_stats = region_stats[region];
    std::sort(iter.second.begin(), iter.second.end(), InstructionBlameStallComparator());

    blame_stats_vec.push_back(BlameStats(region_blame_stats.blame,
                                         region_blame_stats.active_samples,
                                         region_blame_stats.total_samples));

    // regions
    InstructionBlame region_blame;
    region_blame.src_struct = region;
    region_blame.dst_struct = region;
    region_blame.blame_name = BLAME_GPU_INST_METRIC_NAME ":LAT_DEP";
    region_blame.stall_blame = region_blame_stats.blame;

    _inspection.regions.push_back(region_blame);
  }

  // Sort by blame
  std::sort(blame_stats_vec.begin(), blame_stats_vec.end());
  std::sort(_inspection.regions.begin(), _inspection.regions.end(),
            [](const InstructionBlame &l, const InstructionBlame &r) {
              return l.stall_blame > r.stall_blame;
            });

  if (_inspection.regions.size() > _top_regions) {
    _inspection.regions.erase(_inspection.regions.begin() + _top_regions,
                              _inspection.regions.end());
  }

  blame_stats_vec.push_back(
      BlameStats(0.0, kernel_stats.active_samples, kernel_stats.total_samples));

  for (auto &region_blame : _inspection.regions) {
    auto *region = region_blame.src_struct;
    auto &inst_blames = region_blames[region];

    double hotspot_blame = 0.0;
    double total_blame = 0.0;
    // hotspots
    std::vector<InstructionBlame> inst_blame_vec;
    for (auto *inst_blame : inst_blames) {
      if (inst_blame_vec.size() < _top_hotspots) {
        inst_blame_vec.push_back(*inst_blame);
        hotspot_blame += inst_blame->stall_blame;
      }
      total_blame += inst_blame->stall_blame;
    }
    _inspection.hotspots.push_back(inst_blame_vec);
    _inspection.density.push_back(hotspot_blame / total_blame);
  }

  return blame_stats_vec;
}

std::vector<BlameStats> GPUKernelMergeOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                            const KernelStats &kernel_stats) {
  // Match if ifetch and small kernel invoked many times
  // TODO(Keren): count number of instructions
  const size_t KERNEL_COUNT_LIMIT = 10;
  const double KERNEL_TIME_LIMIT = 100 * 1e-6;  // 100us

  auto blame = 0.0;
  if (kernel_stats.time <= KERNEL_TIME_LIMIT && kernel_stats.count >= KERNEL_COUNT_LIMIT) {
    if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") !=
        kernel_blame.stall_blames.end()) {
      blame += kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET");
    }
  }

  _inspection.stall = true;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUFunctionInlineOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                               const KernelStats &kernel_stats) {
  const double IFET_UPPER = 0.3;
  // Do not inline large functions
  const size_t FUNCTION_SIZE_LIMIT = 3000;

  // Both caller and callee can be rescheduled
  auto blame = 0.0;

  std::map<VMA, Prof::Struct::ACodeNode *> vma_function;
  std::map<CudaParse::Block *, std::set<VMA>> caller_blocks;
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->dst_inst->op.find("CALL") != std::string::npos) {
      if (inst_blame->dst_struct->type() == Prof::Struct::ANode::TyStmt) {
        auto *stmt = dynamic_cast<Prof::Struct::Stmt *>(inst_blame->dst_struct);
        if (stmt->stmtType() == Prof::Struct::Stmt::STMT_CALL) {
          caller_blocks[inst_blame->dst_block].insert(stmt->target());
        }
      }
    }
    Prof::Struct::ACodeNode *proc = inst_blame->src_struct->ancestorProc();
    auto vma = proc->vmaSet().begin()->beg();
    vma_function[vma] = proc;
  }

  auto ifetch_stall = 0.0;
  if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") !=
      kernel_blame.stall_blames.end()) {
    ifetch_stall = kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET");
  }

  // Match if instruction fetch latency is low
  std::map<Prof::Struct::ACodeNode *, BlameStats> function_blame;
  if (ifetch_stall / kernel_blame.stall_blame <= IFET_UPPER) {
    for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
      auto *function = inst_blame->src_function;
      Prof::Struct::ACodeNode *proc = inst_blame->src_struct->ancestorProc();

      if (function->global == false && inst_blame->src_function->size < FUNCTION_SIZE_LIMIT) {
        blame += inst_blame->stall_blame;
        function_blame[proc].blame += inst_blame->stall_blame;
        function_blame[proc].total_samples += inst_blame->lat_blame;
        function_blame[proc].active_samples += inst_blame->lat_blame - inst_blame->stall_blame;

        if (caller_blocks.find(inst_blame->dst_block) != caller_blocks.end()) {
          for (auto vma : caller_blocks[inst_blame->dst_block]) {
            if (vma_function.find(vma) != vma_function.end()) {
              auto *caller_proc = vma_function.at(vma);
              function_blame[caller_proc].blame += inst_blame->stall_blame;
              function_blame[caller_proc].total_samples += inst_blame->lat_blame;
              function_blame[caller_proc].active_samples +=
                  inst_blame->lat_blame - inst_blame->stall_blame;
            }
          }
        }
      }
    }
  }

  _inspection.stall = true;
  _inspection.loop = true;

  std::vector<BlameStats> blame_stats_vec;
  for (auto &iter : function_blame) {
    blame_stats_vec.push_back(iter.second);

    InstructionBlame region_blame;
    region_blame.src_struct = iter.first;
    region_blame.dst_struct = iter.first;
    region_blame.blame_name = BLAME_GPU_INST_METRIC_NAME ":LAT_DEP";
    region_blame.stall_blame = iter.second.blame;
    _inspection.regions.push_back(region_blame);
  }

  std::sort(blame_stats_vec.begin(), blame_stats_vec.end());
  std::sort(_inspection.regions.begin(), _inspection.regions.end(),
            [](const InstructionBlame &l, const InstructionBlame &r) {
              return l.stall_blame > r.stall_blame;
            });

  if (_inspection.regions.size() > _top_regions) {
    _inspection.regions.erase(_inspection.regions.begin() + _top_regions,
                              _inspection.regions.end());
  }

  blame_stats_vec.push_back(
      BlameStats(0.0, kernel_stats.active_samples, kernel_stats.total_samples));
  return blame_stats_vec;
}

std::vector<BlameStats> GPUFastMathOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                         const KernelStats &kernel_stats) {
  const double IFET_UPPER = 0.3;

  // Both caller and callee can be rescheduled
  auto blame = 0.0;

  std::map<VMA, Prof::Struct::ACodeNode *> vma_function;
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    Prof::Struct::ACodeNode *proc = inst_blame->src_struct->ancestorProc();
    auto vma = proc->vmaSet().begin()->beg();
    vma_function[vma] = proc;
  }

  auto ifetch_stall = 0.0;
  if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") !=
      kernel_blame.stall_blames.end()) {
    ifetch_stall = kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET");
  }

  // Match if instruction fetch latency is low
  std::map<Prof::Struct::ACodeNode *, BlameStats> function_blame;
  if (ifetch_stall / kernel_blame.stall_blame <= IFET_UPPER) {
    for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
      auto *function = inst_blame->src_function;
      Prof::Struct::ACodeNode *proc = inst_blame->src_struct->ancestorProc();
      if (function->global == false && function->name.find("cuda_sm") != std::string::npos) {
        blame += inst_blame->lat_blame;
        function_blame[proc].blame += inst_blame->lat_blame;
      }
    }
  }

  _inspection.stall = false;
  _inspection.loop = true;

  std::vector<BlameStats> blame_stats_vec;
  for (auto &iter : function_blame) {
    blame_stats_vec.push_back(iter.second);

    InstructionBlame region_blame;
    region_blame.src_struct = iter.first;
    region_blame.dst_struct = iter.first;
    region_blame.blame_name = BLAME_GPU_INST_METRIC_NAME ":LAT_DEP";
    region_blame.stall_blame = iter.second.blame;
    _inspection.regions.push_back(region_blame);
  }

  std::sort(blame_stats_vec.begin(), blame_stats_vec.end());
  std::sort(_inspection.regions.begin(), _inspection.regions.end(),
            [](const InstructionBlame &l, const InstructionBlame &r) {
              return l.stall_blame > r.stall_blame;
            });

  if (_inspection.regions.size() > _top_regions) {
    _inspection.regions.erase(_inspection.regions.begin() + _top_regions,
                              _inspection.regions.end());
  }

  blame_stats_vec.push_back(
      BlameStats(0.0, kernel_stats.active_samples, kernel_stats.total_samples));
  return blame_stats_vec;
}

std::vector<BlameStats> GPUFunctionSplitOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                              const KernelStats &kernel_stats) {
  // Match if ifetch and device function
  auto blame = 0.0;

  std::map<Prof::Struct::ACodeNode *, BlameStats> inlined_functions;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_IFET") != std::string::npos) {
      auto *src_struct = inst_blame->src_struct;
      auto *alien = src_struct->ancestorAlien();
      if (alien != NULL) {
        blame += inst_blame->lat_blame;
        inlined_functions[alien].total_samples += inst_blame->lat_blame;
        inlined_functions[alien].active_samples += inst_blame->lat_blame - inst_blame->stall_blame;
        inlined_functions[alien].blame += inst_blame->lat_blame;
      }
    }
  }

  _inspection.stall = false;
  _inspection.loop = true;

  std::vector<BlameStats> blame_stats_vec;
  for (auto &iter : inlined_functions) {
    blame_stats_vec.push_back(iter.second);

    InstructionBlame region_blame;
    region_blame.src_struct = iter.first;
    region_blame.dst_struct = iter.first;
    region_blame.blame_name = BLAME_GPU_INST_METRIC_NAME ":LAT_IFET";
    region_blame.lat_blame = iter.second.blame;
    _inspection.regions.push_back(region_blame);
  }

  std::sort(blame_stats_vec.begin(), blame_stats_vec.end());
  std::sort(_inspection.regions.begin(), _inspection.regions.end(),
            [](const InstructionBlame &l, const InstructionBlame &r) {
              return l.lat_blame > r.lat_blame;
            });

  if (_inspection.regions.size() > _top_regions) {
    _inspection.regions.erase(_inspection.regions.begin() + _top_regions,
                              _inspection.regions.end());
  }

  return blame_stats_vec;
}

std::vector<BlameStats> GPUSharedMemoryCoalesceOptimizer::match_impl(
    const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if shared memory latency is high and access is not coalseced
  const double COALSECE_LIMIT = 0.8;
  auto blame = 0.0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_IDEP_SMEM") != std::string::npos &&
        inst_blame->efficiency < COALSECE_LIMIT) {
      blame += inst_blame->lat_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.stall = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUAsyncCopyOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                          const KernelStats &kernel_stats) {
  auto blame = 0.0;
  std::vector<BlameStats> blame_stats_vec;
  std::set<CudaParse::InstructionStat *> smem_insts;

  // Find top latency pairs
  BlameStats stall_blame_stats;
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *from_inst = inst_blame->src_inst;
    auto *to_inst = inst_blame->dst_inst;
    if (from_inst->op.find(".GLOBAL") != std::string::npos &&
        to_inst->op.find(".SHARED") != std::string::npos) {
      bool find = false;
      for (auto dst : from_inst->dsts) {
        for (auto src : to_inst->srcs) {
          if (src == dst) {
            stall_blame_stats.blame += inst_blame->stall_blame;
            // direct load-store pairs
            smem_insts.insert(to_inst);
            find = true;
            break;
          }
        }
        if (find) {
          break;
        }
      }
    }
  }

  BlameStats lat_blame_stats;
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *from_inst = inst_blame->src_inst;
    if (smem_insts.find(from_inst) != smem_insts.end()) {
      lat_blame_stats.blame += inst_blame->lat_blame;
      lat_blame_stats.active_samples += inst_blame->lat_blame;
      lat_blame_stats.total_samples += inst_blame->lat_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  stall_blame_stats.active_samples = kernel_stats.active_samples - lat_blame_stats.active_samples;
  stall_blame_stats.total_samples = kernel_stats.total_samples - lat_blame_stats.total_samples;

  blame_stats_vec.push_back(lat_blame_stats);
  blame_stats_vec.push_back(stall_blame_stats);
  blame_stats_vec.push_back(BlameStats(blame,
                                       kernel_stats.active_samples - lat_blame_stats.active_samples,
                                       kernel_stats.total_samples - lat_blame_stats.total_samples));

  _inspection.loop = false;
  _inspection.stall = true;

  return blame_stats_vec;
}

std::vector<BlameStats> GPUGlobalMemoryCoalesceOptimizer::match_impl(
    const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if global memory throttle is high and efficiency is low
  auto blame = 0.0;
  const double COALSECE_LIMIT = 0.5;
  std::vector<BlameStats> blame_stats_vec;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *inst = inst_blame->src_inst;
    if (inst->op.find(".GLOBAL") != std::string::npos &&
        inst_blame->blame_name.find(":LAT_MTHR") != std::string::npos &&
        inst_blame->efficiency < COALSECE_LIMIT) {
      blame += inst_blame->lat_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.loop = false;
  _inspection.stall = false;

  _inspection.hint =
      "Too many non-colaseced global memory requests are issued at the following PCs.\n"
      "    To eliminate abundant memory requests:\n"
      "    1. Coalsece memory access among threads within the same warp.\n";

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUGlobalMemoryReductionOptimizer::match_impl(
    const KernelBlame &kernel_blame, const KernelStats &kernel_stats) {
  // Match if global memory throttle is high and efficiency is high
  const double COALSECE_LIMIT = 0.5;
  auto blame = 0.0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_MTHR") != std::string::npos &&
        inst_blame->efficiency >= COALSECE_LIMIT) {
      blame += inst_blame->lat_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.loop = false;
  _inspection.stall = false;

  _inspection.hint =
      "Too many global memory requests are issued at the following PCs.\n"
      "    To eliminate abundant memory requests:\n"
      "    1. Use constant/texture memory to reduce memory transactions.\n";

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUDivergeReductionOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                 const KernelStats &kernel_stats) {
  const double DIV_LIMIT = 0.5;
  std::vector<BlameStats> blame_stats_vec;
  auto blame = 0.0;

  // Match if high execution latency for diverged branch instructions
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *src_inst = inst_blame->src_inst;

    if (src_inst->op.find("BRANCH") != std::string::npos && inst_blame->efficiency < DIV_LIMIT) {
      blame += inst_blame->lat_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.hint =
      "Diverged branch conditions encountered in execution. Look for improvements to "
      "reduce branch conditions or replicate similar computations.\n";
  _inspection.stall = false;
  _inspection.loop = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUBranchEliminationOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                  const KernelStats &kernel_stats) {
  const double PRED_LIMIT_UPPER = 0.999;
  const double PRED_LIMIT_LOWER = 0.001;
  std::vector<BlameStats> blame_stats_vec;
  std::set<CudaParse::Block *> match_blocks;

  BlameStats lat_blame_stats;
  // Match if high execution latency for diverged branch instructions
  for (auto *inst_blame : kernel_blame.lat_inst_blame_ptrs) {
    auto *src_inst = inst_blame->src_inst;

    if (src_inst->op.find("BRANCH") != std::string::npos &&
        (inst_blame->pred_true != -1.0 && (inst_blame->pred_true >= PRED_LIMIT_UPPER ||
         inst_blame->pred_true <= PRED_LIMIT_LOWER))) {
      // Ensure lat_blame_stats is hidden
      lat_blame_stats.blame += inst_blame->lat_blame;
      lat_blame_stats.active_samples += inst_blame->lat_blame;
      lat_blame_stats.total_samples += inst_blame->lat_blame;

      auto *src_block = inst_blame->src_block;
      auto *dst_block = inst_blame->dst_block;
      match_blocks.insert(src_block);
      match_blocks.insert(dst_block);

      if (_inspection.regions.size() < _top_regions) {
        auto b = *inst_blame;
        // Fake stall to let it sort by lat_blame
        b.stall_blame = b.lat_blame;
        _inspection.regions.push_back(std::move(b));
      }
    }
  }

  BlameStats stall_blame_stats;
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (match_blocks.find(inst_blame->src_block) != match_blocks.end() ||
        match_blocks.find(inst_blame->dst_block) != match_blocks.end()) {
      stall_blame_stats.blame += inst_blame->stall_blame;
      stall_blame_stats.active_samples += inst_blame->lat_blame - inst_blame->stall_blame;
      stall_blame_stats.total_samples += inst_blame->lat_blame;
    }
  }
  stall_blame_stats.active_samples -= lat_blame_stats.active_samples;
  stall_blame_stats.total_samples -= lat_blame_stats.total_samples;

  blame_stats_vec.push_back(lat_blame_stats);
  blame_stats_vec.push_back(stall_blame_stats);
  blame_stats_vec.push_back(BlameStats(0.0,
                                       kernel_stats.active_samples - lat_blame_stats.active_samples,
                                       kernel_stats.total_samples));

  _inspection.hint =
      "Determined branch conditions encountered in execution. Look for improvements to "
      "remove branch conditions.\n";
  _inspection.stall = true;
  _inspection.loop = false;

  return blame_stats_vec;
}

std::vector<BlameStats> GPUOccupancyIncreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                  const KernelStats &kernel_stats) {
  auto blame = 0.0;

  if (kernel_blame.stall_blame != 0.0) {
    _inspection.active_warp_count.first = kernel_stats.active_warps;
    _inspection.active_warp_count.second = _arch->warps();

    for (auto &lat_blame_iter : kernel_blame.lat_blames) {
      auto blame_name = lat_blame_iter.first;
      auto blame_metric = lat_blame_iter.second;

      if (blame_name.find(":LAT_NONE") != std::string::npos) {
        // This is a special case that we iterate lat_blames
        // lat_none is 0 in stall_blames
        blame += blame_metric;
      }
    }
  }

  _inspection.stall = true;
  _inspection.loop = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUOccupancyDecreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                                  const KernelStats &kernel_stats) {
  // Match if not selected if high
  auto blame = 0.0;

  // Find top latency pairs
  for (auto *inst_blame : kernel_blame.stall_inst_blame_ptrs) {
    if (inst_blame->blame_name.find(":LAT_NSEL") != std::string::npos) {
      blame += inst_blame->stall_blame;

      if (_inspection.regions.size() < _top_regions) {
        _inspection.regions.push_back(*inst_blame);
      }
    }
  }

  _inspection.stall = true;
  _inspection.loop = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUSMBalanceOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                          const KernelStats &kernel_stats) {
  // Match if blocks are large while SM efficiency is low
  auto blame = kernel_stats.sm_efficiency;

  _inspection.stall = false;
  _inspection.loop = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUBlockIncreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                              const KernelStats &kernel_stats) {
  int cur_blocks = kernel_stats.blocks;
  int cur_threads = kernel_stats.threads;
  int cur_warps = kernel_stats.active_warps;

  auto blame = 0.0;
  if (cur_blocks < this->_arch->sms() && cur_threads > _arch->schedulers() * _arch->warp_size()) {
    double warp_ratio = 1.0 / (static_cast<double>(cur_warps) / _arch->schedulers());

    double issue_samples = 0;
    if (kernel_blame.lat_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_NONE") !=
        kernel_blame.lat_blames.end()) {
      issue_samples = kernel_blame.lat_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_NONE");
    }
    double issue = issue_samples / static_cast<double>(kernel_stats.total_samples);
    double warp_issue = 1 - pow(1 - issue, cur_warps / _arch->schedulers());
    double new_warp_issue = issue;

    double issue_ratio = warp_issue / new_warp_issue;

    double mthr_ratio = 0;
    double nsel_ratio = 0;
    double pipe_ratio = 0;
    if (kernel_blame.lat_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_MTHR") !=
        kernel_blame.lat_blames.end()) {
      mthr_ratio = kernel_blame.lat_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_MTHR") /
                   kernel_stats.total_samples;
    }
    if (kernel_blame.lat_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_NSEL") !=
        kernel_blame.lat_blames.end()) {
      nsel_ratio = kernel_blame.lat_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_NSEL") /
                   kernel_stats.total_samples;
    }
    if (kernel_blame.lat_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_PIPE") !=
        kernel_blame.lat_blames.end()) {
      pipe_ratio = kernel_blame.lat_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_PIPE") /
                   kernel_stats.total_samples;
    }

    blame = warp_ratio * issue_ratio * (1 - mthr_ratio - nsel_ratio - pipe_ratio);

    _inspection.block_count.first = cur_blocks;
    _inspection.thread_count.first = cur_threads;
  }

  _inspection.stall = false;
  _inspection.loop = false;

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

std::vector<BlameStats> GPUBlockDecreaseOptimizer::match_impl(const KernelBlame &kernel_blame,
                                                              const KernelStats &kernel_stats) {
  // Match if threads are few, increase number of threads per block. i.e.
  // threads coarsen
  const size_t WARP_COUNT_LIMIT = 2;

  auto blame = 0.0;
  auto warps = (kernel_stats.threads - 1) / _arch->warp_size() + 1;

  _inspection.block_count.first = kernel_stats.blocks;
  _inspection.thread_count.first = kernel_stats.threads;
  _inspection.stall = false;
  _inspection.loop = false;

  // blocks are enough, while threads are few
  // Concurrent (not synchronized) blocks may introduce instruction cache
  // latency Reduce the number of blocks keep warps fetch the same instructions
  // at every cycle
  if (static_cast<int>(kernel_stats.blocks) > _arch->sms() && warps <= WARP_COUNT_LIMIT) {
    if (kernel_blame.stall_blames.find(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET") !=
        kernel_blame.stall_blames.end()) {
      blame += kernel_blame.stall_blames.at(BLAME_GPU_INST_METRIC_NAME ":LAT_IFET");
    }
  }

  BlameStats blame_stats(blame, kernel_stats.active_samples, kernel_stats.total_samples);
  return std::vector<BlameStats>{blame_stats};
}

}  // namespace Analysis
