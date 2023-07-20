// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *


//***************************************************************************

#ifndef _CFG_PARSER_H_
#define _CFG_PARSER_H_

//***************************************************************************
// system includes
//***************************************************************************

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>



//***************************************************************************
// local includes
//***************************************************************************

#include "GPUCFG.hpp"
#include "Graph.hpp"



//***************************************************************************
// begin namespace
//***************************************************************************

namespace GPUParse {


//***************************************************************************
// declarations
//***************************************************************************

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



//***************************************************************************
// end namespace
//***************************************************************************

}

#endif
