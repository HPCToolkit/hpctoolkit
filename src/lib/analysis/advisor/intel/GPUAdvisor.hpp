#include <lib/banal/gpu/DotCFG.hpp>                            // GPUParse
//#include <lib/prof/Struct-Tree.hpp>                            // ACodeNode
//#include <lib/prof/CCT-Tree.hpp>                               // ADynNode
#include <lib/isa/ISATypes.hpp>
#include "../../CCTGraph.hpp"                           // CCTGraph
#include <lib/analysis/advisor/intel/GPUArchitecture.hpp>      // GPUArchitecture
//#include <lib/analysis/MetricNameProfMap.hpp>                  // CCTGraph

struct InstructionBlame {
  GPUParse::InstructionStat *src_inst, *dst_inst;
  GPUParse::Block *src_block, *dst_block;
  // TODO(Keren): consider only intra procedural optimizations for now
  GPUParse::Function *src_function, *dst_function;
  // src->dst instructions
  // 0: same instruction
  // -1: very long distance
  // >0: exact distance
  double distance;
  double lat_blame;
  // TODO(Keren): only care about src efficiency
  double efficiency;
  double pred_true;
  std::string blame_name;

  InstructionBlame(GPUParse::InstructionStat *src_inst, GPUParse::InstructionStat *dst_inst,
                   double distance, double lat_blame, double efficiency,
                   double pred_true, const std::string &blame_name)
      : src_inst(src_inst),
        dst_inst(dst_inst),
        distance(distance),
        lat_blame(lat_blame),
        efficiency(efficiency),
        pred_true(pred_true),
        blame_name(blame_name) {}

  InstructionBlame(GPUParse::InstructionStat *src_inst, GPUParse::InstructionStat *dst_inst,
                   GPUParse::Block *src_block, GPUParse::Block *dst_block,
                   GPUParse::Function *src_function, GPUParse::Function *dst_function,
                   double distance, double lat_blame, double efficiency,
                   double pred_true, const std::string &blame_name)
      : src_inst(src_inst),
        dst_inst(dst_inst),
        src_block(src_block),
        dst_block(dst_block),
        src_function(src_function),
        dst_function(dst_function),
        distance(distance),
        lat_blame(lat_blame),
        efficiency(efficiency),
        pred_true(pred_true),
        blame_name(blame_name) {}

  InstructionBlame()
      : src_inst(NULL),
        dst_inst(NULL),
        src_block(NULL),
        dst_block(NULL),
        src_function(NULL),
        dst_function(NULL),
        distance(0),
        lat_blame(0),
        efficiency(1.0),
        pred_true(-1.0) {}
};


struct InstructionBlameLatComparator {
  bool operator()(const InstructionBlame *l, const InstructionBlame *r) const {
    return l->lat_blame > r->lat_blame;
  }
};

struct InstructionBlameSrcComparator {
  bool operator()(const InstructionBlame *l, const InstructionBlame *r) const {
    return l->src_inst->pc < r->src_inst->pc;
  }
};

struct InstructionBlameDstComparator {
  bool operator()(const InstructionBlame *l, const InstructionBlame *r) const {
    return l->dst_inst->pc < r->dst_inst->pc;
  }
};

typedef std::vector<InstructionBlame> InstBlames;
typedef std::vector<InstructionBlame *> InstBlamePtrs;

struct KernelBlame {
  InstBlames inst_blames;
  InstBlamePtrs lat_inst_blame_ptrs;
  std::map<std::string, double> lat_blames;
  double lat_blame;

  KernelBlame() : lat_blame(0) {}
};


class GPUAdvisor {
 public:
  explicit GPUAdvisor() {}

  void init(const std::string &gpu_arch);

  void configInst(const std::string &lm_name, const std::vector<GPUParse::Function *> &functions,
                  std::map<int, std::pair<int, int>> kernel_latency_frequency_map);

  void blame(KernelBlame &kernel_blame);

  //void advise(const CCTBlames &cct_blames);



 private:
  GPUArchitecture *_arch;
  typedef std::map<int, std::map<int, std::vector<std::vector<GPUParse::Block *>>>> CCTEdgePathMap;

  typedef std::map<VMA, CCTNode> CCTNodeMap;

  // lat
  std::string _lat_metric;

  // inst
  std::string _inst_exe_pred_metric;


  std::map<int, int> _function_offset;
  //Analysis::CCTGraph<GPUParse::InstructionStat *> _inst_dep_graph;
  CCTNodeMap _node_map;

  enum TrackType {
    TRACK_REG = 0,
    TRACK_PRED_REG = 1,
    TRACK_PREDICATE = 2,
    TRACK_UNIFORM = 3,
    TRACK_BARRIER = 4
  };


 private:
  void initCCTDepGraph(CCTGraph &cct_dep_graph);

  void
  trackDep
  (
   int from_vma,
   int to_vma,
   int id,
   GPUParse::Block *from_block,
   GPUParse::Block *to_block,
   int latency_issue,
   int latency,
   std::set<GPUParse::Block *> &visited_blocks,
   std::vector<GPUParse::Block *> &path,
   std::vector<std::vector<GPUParse::Block *>> &paths,
   TrackType track_type,
   bool fixed,
   int barrier_threshold
  );

  void trackDepInit(int to_vma, int from_vma, int dst, CCTEdgePathMap &cct_edge_path_map,
                    TrackType track_type, bool fixed, int barrier_threshold);

  void pruneCCTDepGraphLatency(CCTGraph &cct_dep_graph,
                               CCTEdgePathMap &cct_edge_path_map);

  void blameCCTDepGraph(CCTGraph &cct_dep_graph,
                        CCTEdgePathMap &cct_edge_path_map, InstBlames &inst_blames);

  void overlayInstBlames(InstBlames &inst_blames, KernelBlame &kernel_blame);

  void printNodes(CCTGraph cct_dep_graph);
};
