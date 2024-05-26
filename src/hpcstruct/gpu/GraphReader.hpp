// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef BANAL_GPU_GRAPH_READER_H
#define BANAL_GPU_GRAPH_READER_H

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
  std::string _file_name;
};



//***************************************************************************
// end namespaces
//***************************************************************************

}

#endif
