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

#ifndef BINUTILS_INTEL_CREATE_CFG
#define BINUTILS_INTEL_CREATE_CFG

#include <vector>



//******************************************************************************
// local data
//******************************************************************************

class Edge {
	public:
		int32_t from;	
		int32_t to;
		int32_t from_blockEndOffset;

		Edge(int32_t f, int32_t t, int32_t from_b) {
			from = f;
			to = t;
			from_blockEndOffset = from_b;
		}

		bool operator == (const Edge &that) const 
		{
			return((this->from == that.from) && (this->to == that.to));
		}
		
		bool operator<(const Edge& that) const 
		{
			if (this->from == that.from) {
				return (this->to < that.to);
			} else {
				return (this->from < that.from);
			}
		}
};



//******************************************************************************
// interface operations
//******************************************************************************

void
printCFGInDotGraph
(
	std::vector<uint8_t> intelRawGenBinary
);


std::string
getBlockAndInstructionOffsets
(
 std::vector<uint8_t> intelRawGenBinary
);

#endif
