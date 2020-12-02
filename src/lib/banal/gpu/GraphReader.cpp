#include "GraphReader.hpp"
#include <vector>

#define WEAK_NAME "\t.weak\t\t"
#define FUNC_LABEL "@function"
#define TYPE_LABEL ".type"

namespace GPUParse {

void GraphReader::read(Graph &graph) {
  // Read dot graph
  std::ifstream file(_file_name);
  std::stringstream dotfile;

  dotfile << file.rdbuf();
  file.close();

  boost::read_graphviz_detail::parser_result result;
  boost::read_graphviz_detail::parse_graphviz_from_string(dotfile.str(), result, true);

  std::unordered_map<std::string, size_t> vertex_name_to_id;
  read_vertices(result, vertex_name_to_id, graph);  
  read_edges(result, vertex_name_to_id, graph);
}


void GraphReader::read_vertices(
  const boost::read_graphviz_detail::parser_result &result,
  std::unordered_map<std::string, size_t> &vertex_name_to_id,
  Graph &graph) {
  size_t vertex_id = 0;
  for (auto node : result.nodes) {
    std::string vertex_name = node.first;
    // vertex name in dot graph is different with the function name in
    // call instructions. For example, a vertex name may start with symbols
    // like .weak or .text, which a callee function name does not have
    vertex_name_to_id[vertex_name] = vertex_id;

    const std::string &vertex_label = (node.second)["label"];
    // handle nvdisasm bug: sometimes block name is a label like .L_105 
    // if the block contains a .weak name indicating it represents a function
    // use the .weak name instead
    if (vertex_name[0] == '.') {
      std::string tmp = vertex_label;
      auto weak = tmp.find(WEAK_NAME);
      auto label = tmp.find(FUNC_LABEL);
      if (weak != std::string::npos) {
        tmp = tmp.substr(weak+strlen(WEAK_NAME));
        auto endweak = tmp.find("\\");
        if (endweak != std::string::npos) {
          vertex_name = tmp.substr(0, endweak);
        }
      } else if (label != std::string::npos) {
        auto type = tmp.find(TYPE_LABEL);
        vertex_name = tmp.substr(type+strlen(TYPE_LABEL), label - type - strlen(TYPE_LABEL));
        // trim
        vertex_name.erase(0, vertex_name.find_first_not_of("\t"));
        vertex_name.erase(vertex_name.find_last_not_of("\t") + 1);
        vertex_name.erase(0, vertex_name.find_first_not_of(" "));
        vertex_name.erase(vertex_name.find_last_not_of(" ") + 1);
        vertex_name.erase(0, vertex_name.find_first_not_of(","));
        vertex_name.erase(vertex_name.find_last_not_of(",") + 1);
      }
    }

    graph.vertices.push_back(new Vertex(vertex_id, vertex_name, vertex_label));
    ++vertex_id;
  }
}


void GraphReader::read_edges(
  const boost::read_graphviz_detail::parser_result &result,
  std::unordered_map<std::string, size_t> &vertex_name_to_id,
  Graph &graph) {
  for (auto einfo : result.edges) {
    size_t source_id = vertex_name_to_id[einfo.source.name];
    size_t target_id = vertex_name_to_id[einfo.target.name];
    std::vector<std::string> &source_port = einfo.source.location;
    std::vector<std::string> &target_port = einfo.target.location;
    graph.edges.push_back(new Edge(source_id, target_id, source_port, target_port));
  }
}

}
