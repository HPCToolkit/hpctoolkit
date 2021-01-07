#ifndef _CUDA_GRAPH_READER_H_
#define _CUDA_GRAPH_READER_H_

#include <boost/graph/graphviz.hpp>
#include <boost/graph/detail/read_graphviz_new.hpp>
#include <string>
#include <unordered_map>

#include "Graph.hpp"

namespace CudaParse {

class GraphReader {
 public:
  GraphReader(const std::string &file_name) : _file_name(file_name) {}

  void read(Graph &graph);

 private:
  void read_vertices(const boost::read_graphviz_detail::parser_result &result,
                     std::unordered_map<std::string, size_t> &vertex_name_to_id, Graph &graph);

  void read_edges(const boost::read_graphviz_detail::parser_result &result,
                  std::unordered_map<std::string, size_t> &vertex_name_to_id, Graph &graph);

 private:
  std::string _file_name;
};

}  // namespace CudaParse

#endif
