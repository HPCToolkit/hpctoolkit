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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef BANAL_GPU_GRAPH_H
#define BANAL_GPU_GRAPH_H

//***************************************************************************
// system includes
//***************************************************************************

#include <string>
#include <unordered_map>
#include <vector>



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace GPUParse {



//***************************************************************************
// type declarations
//***************************************************************************

struct Vertex {
  size_t id;
  std::string name;
  std::string label;

  Vertex(size_t id, const std::string &name, const std::string &label) :
    id(id), name(name), label(label) {}
};


struct Edge {
  size_t source_id;
  size_t target_id;
  // At most two entries for port
  std::vector<std::string> source_port;
  std::vector<std::string> target_port;

  Edge(size_t source_id, size_t target_id,
    const std::vector<std::string> &source_port,
    const std::vector<std::string> &target_port) :
    source_id(source_id), target_id(target_id),
    source_port(source_port), target_port(target_port) {}
};


struct Graph {
  // vertex_id->vertex
  std::vector<Vertex *> vertices;
  std::vector<Edge *> edges;

  ~Graph() {
    for (auto *vertex : vertices) {
      delete vertex;
    }
    for (auto *edge : edges) {
      delete edge;
    }
  }
};



//***************************************************************************
// end namespaces
//***************************************************************************

}

#endif
