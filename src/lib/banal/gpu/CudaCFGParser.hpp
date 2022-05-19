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
  static void parse(const Graph &graph, std::vector<Function *> &functions);

  static void parse_calls(std::vector<Function *> &functions);

 private:
  static void parse_inst_strings(const std::string &label, std::deque<std::string> &inst_strings);

  static void link_fallthrough_edges(
    const Graph &graph,
    const std::vector<Block *> &blocks,
    std::unordered_map<size_t, Block *> &block_id_map);

  static void split_blocks(std::vector<Block *> &blocks,
    std::unordered_map<size_t, Block *> &block_id_map);

  static void find_block_parent(const std::vector<Block *> &blocks, std::vector<size_t> &block_parent);

  static void unite_blocks(const Block *block, size_t parent,
    std::vector<size_t> &block_parent, bool *visited);

  static TargetType get_target_type(const Inst *inst);

  static TargetType get_fallthrough_type(const Inst *inst);

  static void link_dangling_blocks(
    std::set<Block *> &dangling_blocks,
    std::vector<Function *> &functions);
};

}

#endif
