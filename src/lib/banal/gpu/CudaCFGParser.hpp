#ifndef _CFG_PARSER_H_
#define _CFG_PARSER_H_

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include "DotCFG.hpp"
#include "Graph.hpp"

namespace GPUParse {

class CudaCFGParser {
 public:
  CudaCFGParser() : _block_parent(0) {}

  void parse(const Graph &graph, std::vector<Function *> &functions);

  void parse_calls(std::vector<Function *> &functions);

  ~CudaCFGParser() {}

 private:
  void parse_inst_strings(const std::string &label, std::deque<std::string> &inst_strings);

  void link_fallthrough_edges(
    const Graph &graph,
    const std::vector<Block *> &blocks,
    std::unordered_map<size_t, Block *> &block_id_map);

  void split_blocks(std::vector<Block *> &blocks,
    std::unordered_map<size_t, Block *> &block_id_map);

  void find_block_parent(const std::vector<Block *> &blocks);

  void unite_blocks(const Block *block, bool *visited, size_t parent);

  TargetType get_target_type(const Inst *inst);

  TargetType get_fallthrough_type(const Inst *inst);

  void link_dangling_blocks(
    std::set<Block *> &dangling_blocks,
    std::vector<Function *> &functions);

 private:
  std::vector<size_t> _block_parent;
};

}

#endif
