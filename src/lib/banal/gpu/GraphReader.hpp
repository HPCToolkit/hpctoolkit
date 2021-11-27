#ifndef BANAL_GPU_GRAPH_READER_H
#define BANAL_GPU_GRAPH_READER_H

#include <unordered_map>
#include <string>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/detail/read_graphviz_new.hpp>
#include "Graph.hpp"

namespace GPUParse {

class GraphReader {
 public:
  static void read(const std::string &file_name, Graph &graph);

 private:
  static void read_vertices(
    const boost::read_graphviz_detail::parser_result &result,
    std::unordered_map<std::string, size_t> &vertex_name_to_id,
    Graph &graph);

  static void read_edges(
    const boost::read_graphviz_detail::parser_result &result,
    std::unordered_map<std::string, size_t> &vertex_name_to_id,
    Graph &graph);
};

}

#endif
