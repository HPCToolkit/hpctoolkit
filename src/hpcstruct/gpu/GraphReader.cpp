// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
// system includes
//***************************************************************************

#include <vector>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "GraphReader.hpp"



//***************************************************************************
// macros
//***************************************************************************

#define WEAK_NAME "\t.weak\t\t"
#define FUNC_LABEL "@function"
#define TYPE_LABEL ".type"



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace GPUParse {



//***************************************************************************
// interface operations
//***************************************************************************

void GraphReader::read(Graph &graph) {
  // Read dot graph
  struct VertexProps {
    std::string name;
    std::string label;
    std::string blank;
  };
  struct EdgeProps {
    std::string blank;
  };
  typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
    boost::property<boost::vertex_index_t, std::size_t, VertexProps>,
    EdgeProps> graph_t;

  graph_t bgraph(0);
  boost::dynamic_properties dp;
  dp.property("node_id", get(&VertexProps::name, bgraph));
  dp.property("label", get(&VertexProps::label, bgraph));
  auto vid = get(boost::vertex_index, bgraph);

  // These properties we really don't care about, but appear in the DOT output so we need a place
  // for them to go.
  std::string sink;
  boost::ref_property_map<graph_t::vertex_descriptor, std::string> sink_v(sink);
  dp.property("fontname", sink_v);
  dp.property("fontsize", sink_v);
  dp.property("shape", sink_v);
  dp.property("style", sink_v);
  boost::ref_property_map<graph_t::edge_descriptor, std::string> sink_e(sink);
  dp.property("style", sink_e);

  {
    std::ifstream file(_file_name);
    boost::read_graphviz(file, bgraph, dp, "node_id");
  }

  for (auto [bvit, bvend] = boost::vertices(bgraph); bvit != bvend; ++bvit) {
    const auto& bv = *bvit;
    auto* v = new Vertex(get(vid, bv), bgraph[bv].name, bgraph[bv].label);

    // handle nvdisasm bug: sometimes block name is a label like .L_105
    // if the block contains a .weak name indicating it represents a function
    // use the .weak name instead
    if (v->name[0] == '.') {
      std::string tmp = v->label;
      auto weak = tmp.find(WEAK_NAME);
      auto label = tmp.find(FUNC_LABEL);
      if (weak != std::string::npos) {
        tmp = tmp.substr(weak+strlen(WEAK_NAME));
        auto endweak = tmp.find("\\");
        if (endweak != std::string::npos) {
          v->name = tmp.substr(0, endweak);
        }
      } else if (label != std::string::npos) {
        auto type = tmp.find(TYPE_LABEL);
        v->name = tmp.substr(type+strlen(TYPE_LABEL), label - type - strlen(TYPE_LABEL));
        // trim
        v->name.erase(0, v->name.find_first_not_of("\t"));
        v->name.erase(v->name.find_last_not_of("\t") + 1);
        v->name.erase(0, v->name.find_first_not_of(" "));
        v->name.erase(v->name.find_last_not_of(" ") + 1);
        v->name.erase(0, v->name.find_first_not_of(","));
        v->name.erase(v->name.find_last_not_of(",") + 1);
      }
    }

    graph.vertices.push_back(v);
  }
  for (auto [beit, beend] = boost::edges(bgraph); beit != beend; ++beit) {
    const auto& be = *beit;
    auto* e = new Edge(get(vid, boost::source(be, bgraph)), get(vid, boost::target(be, bgraph)));
    graph.edges.push_back(e);
  }
}

//***************************************************************************
// end namespaces
//***************************************************************************

}
