// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef BANAL_GPU_GRAPH_READER_H
#define BANAL_GPU_GRAPH_READER_H

//***************************************************************************
// system includes
//***************************************************************************

#include <unordered_map>
#include <string>



//***************************************************************************
// Boost includes
//***************************************************************************

#include <boost/graph/graphviz.hpp>
#include <boost/graph/detail/read_graphviz_new.hpp>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "Graph.hpp"



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace GPUParse {



//***************************************************************************
// type declarations
//***************************************************************************

class GraphReader {
 public:
  GraphReader(const std::string &file_name) : _file_name(file_name) {}

  void read(Graph &graph);

 private:
  void read_vertices(
    const boost::read_graphviz_detail::parser_result &result,
    std::unordered_map<std::string, size_t> &vertex_name_to_id,
    Graph &graph);

  void read_edges(
    const boost::read_graphviz_detail::parser_result &result,
    std::unordered_map<std::string, size_t> &vertex_name_to_id,
    Graph &graph);

 private:
  std::string _file_name;
};



//***************************************************************************
// end namespaces
//***************************************************************************

}

#endif
