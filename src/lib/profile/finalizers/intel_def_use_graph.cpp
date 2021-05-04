// -*-Mode: C++;-*-

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
// Copyright ((c)) 2019-2020, Rice University
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

#include <fstream>                  // ifstream
#include <map>
#include <iostream>   //cout



//******************************************************************************
// local includes
//******************************************************************************

#include "intel_def_use_graph.hpp"
#include "pipeline.hpp"
#include <lib/support-lean/demangle.h>


using namespace hpctoolkit;
using namespace finalizers;

IntelDefUseGraphClassification::IntelDefUseGraphClassification() {}


void IntelDefUseGraphClassification::readDefUseGraphEdges
(
 std::string fileName,
 Classification &c
)
{
  std::ifstream infile(fileName);
  int from, to;
  std::string edge;
  while ((infile >> from >> edge >> to) && (edge == "->")) {
    uint32_t path_length = 0;
    if (c._def_use_graph.find(from) == c._def_use_graph.end()) {
      path_length = 1;
    } else {
      std::map<uint64_t, uint32_t> &from_incoming_edges = c._def_use_graph[from];
      for (auto edge: from_incoming_edges) {
        if (edge.second > path_length) {
          path_length = edge.second;
        }
      }
    }
    c._def_use_graph[to][from] = path_length + 1;
  }
  // std::cout << "graph creation: " << fileName << ", size: " << c._def_use_graph.size() << std::endl;
}


void IntelDefUseGraphClassification::module(const Module& m, Classification& c) noexcept {
  const auto& rpath = m.userdata[sink.resolvedPath()];
  const auto& mpath = rpath.empty() ? m.path() : rpath;
  std::string mpath_str = mpath.string();
  if (strstr(mpath_str.c_str(), "gpubins")) {

    const char *delimiter = ".gpubin";
    size_t delim2_loc = mpath_str.find(delimiter);
    std::string filePath = mpath_str.substr(0, delim2_loc + 7);
    std::string du_filePath = filePath + ".du";

    readDefUseGraphEdges(du_filePath, c);
  }
}

