#ifndef _CFG_PARSER_H_
#define _CFG_PARSER_H_

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include "DotCFG.hpp"
#include "Graph.hpp"

namespace CudaParse {

class CFGParser {
 public:
  CFGParser() : _block_parent(0) {}
  void parse(const Graph &graph, std::vector<Function *> &functions);

  ~CFGParser() {}

 private:
  void parse_calls(std::vector<Function *> &functions);

  void parse_inst_strings(const std::string &label, std::deque<std::string> &inst_strings);

  void split_blocks(std::vector<std::pair<size_t, size_t> > &edges,
    std::vector<Block *> &blocks, std::unordered_map<size_t, Block *> &block_map);

  size_t find_block_parent(size_t node);

  void unite_blocks(size_t l, size_t r);

  TargetType get_target_type(const Block *source_block, Inst *inst);

 private:
  std::vector<size_t> _block_parent;
};

}

#endif
