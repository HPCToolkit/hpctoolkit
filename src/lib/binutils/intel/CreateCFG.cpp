// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <kv.hpp>
#include <igc_binary_decoder.h>

#include <iostream>
#include <stack>
#include <algorithm>
#include <vector>
#include <sstream>
#include <set>
#include <map> 



//******************************************************************************
// local includes
//******************************************************************************

#include "CreateCFG.hpp"



//******************************************************************************
// local data
//******************************************************************************

#define MAX_STR_SIZE 1024

std::vector<int32_t> block_offsets;
std::map<int32_t, bool> visitedBlockOffsets;



//******************************************************************************
// private operations
//******************************************************************************

static std::set<Edge>
get_cfg_edges
(
	std::vector<uint8_t> binary,
	size_t binary_size
)
{
	KernelView kv(IGA_GEN9, binary.data(), binary.size(),
			iga::SWSB_ENCODE_MODE::SingleDistPipe);
	std::set<Edge> cfg_edges;

	int32_t offset = 0;
	int32_t size;
	while (offset < binary_size) {
		int32_t prev_block_start_offset;
		int32_t prev_block_end_offset;
		int32_t block_start_offset;
		bool isStartOfBasicBlock = kv.isInstTarget(offset);
		if (isStartOfBasicBlock) {
			block_offsets.push_back(offset);
			visitedBlockOffsets.insert({offset, false});
			block_start_offset = offset;	
		}
		size = kv.getInstSize(offset);
		while (!kv.isInstTarget(offset + size) && (offset + size < binary_size)) {
			offset += size;	
			size = kv.getInstSize(offset);
			if (size == 0) {
				// this is a weird edge case, what to do?
				break;
			}
		}

		int32_t *jump_targets = new int32_t[KV_MAX_TARGETS_PER_INSTRUCTION];
		size_t jump_targets_count = kv.getInstTargets(offset, jump_targets);
		int32_t next_block_start_offset = offset + size;
		bool isFallThroughEdgeAdded = false;

		for (size_t i = 0; i < jump_targets_count; i++) {
			if (jump_targets[i] == next_block_start_offset) {
				isFallThroughEdgeAdded = true;
			} else if (jump_targets[i] == block_start_offset) {
				if (block_offsets.size() >= 2) {
					int32_t from = block_offsets[block_offsets.size() - 2];
					int32_t from_blockEndOffset;
					for (Edge edge: cfg_edges) {
						if (edge.from == from && edge.to == block_start_offset) {
							from_blockEndOffset	 = edge.from_blockEndOffset;
						}
					}	
					cfg_edges.insert(Edge(block_offsets[block_offsets.size() - 2], block_start_offset, from_blockEndOffset));
				}
			}
			cfg_edges.insert(Edge(block_start_offset, jump_targets[i], next_block_start_offset - size));
		}
		if(!isFallThroughEdgeAdded) {
			cfg_edges.insert(Edge(block_start_offset, next_block_start_offset, next_block_start_offset - size));
		}
		prev_block_start_offset = block_start_offset;
		prev_block_end_offset = offset; 
		offset += size;
	}
	cfg_edges.insert(Edge(block_offsets[block_offsets.size() - 1], binary_size, binary_size - size));
	return cfg_edges;
}


static void
printCFGEdges
(
	std::set<Edge> cfg_edges
)
{
	for (Edge edge: cfg_edges) {
		std::cout << edge.from << "->" << edge.to << std::endl;	
	}	
}


static void
printBasicBlocks
(
	std::vector<uint8_t> binary,
	std::set<Edge> cfg_edges
)
{
	KernelView kv(IGA_GEN9, binary.data(), binary.size(), iga::SWSB_ENCODE_MODE::SingleDistPipe);
	int32_t offset;
	char text[MAX_STR_SIZE] = { 0 };
	size_t length;
	int32_t size;

	for (Edge edge: cfg_edges) {
		offset = edge.from;
		if(edge.from == edge.to) {
			// skip self-loops
			continue;
		}
		auto it = visitedBlockOffsets.find(offset);
		if (it->second) {
			continue;
		} else {
			it->second = true;
		}
		std::cout << offset << " [ label=\"\\\n"; 
		while (offset < edge.to) {
			size = kv.getInstSize(offset);
			length = kv.getInstSyntax(offset, text, MAX_STR_SIZE);
			assert(length > 0);
			std::cout << offset << ": " << text << "\\\l";
			offset += size;
		}
		std::cout << "\" shape=\"box\"]; \n" << std::endl;
	}	
}



//******************************************************************************
// interface operations
//******************************************************************************

// pass Intel kernel's raw gen binary
// kernel's text region is a raw gen binary
// you  can find kernel nested in [debug section of GPU binary/separate debug section dump]
void printCFGInDotGraph(std::vector<uint8_t> intelRawGenBinary) {
	std::cout << "digraph GEMM_iga {" << std::endl;
	std::set<Edge> edges = get_cfg_edges(intelRawGenBinary, intelRawGenBinary.size());
	printBasicBlocks(intelRawGenBinary, edges);
	printCFGEdges(edges);
	std::cout << "}" << std::endl;
}
