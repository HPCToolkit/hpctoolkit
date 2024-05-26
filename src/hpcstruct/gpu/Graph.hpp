// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

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

  Edge(size_t source_id, size_t target_id) :
    source_id(source_id), target_id(target_id) {}
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
