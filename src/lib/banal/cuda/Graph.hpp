#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace CudaParse {

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

}

#endif
