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
// Copyright ((c)) 2002-2017, Rice University
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

/* 
 * File:   TCT-Semantic-Label.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on August 13, 2018, 7:25 PM
 */

#ifndef TCT_SEMANTIC_LABEL_HPP
#define TCT_SEMANTIC_LABEL_HPP

#include <string>
using std::string;

namespace TraceAnalysis {
    // A function frame on a call path can be classified as computation or communication.
  const uint SEMANTIC_LABEL_COMPUTATION   = 0x0;
  const uint SEMANTIC_LABEL_COMMUNICATION = 0x1;
  
  // Break down of communication.
  const uint SEMANTIC_LABEL_SYNC          = 0x10 | SEMANTIC_LABEL_COMMUNICATION; // e.g. MPI barrier / allreduce, OpenMP barriers.
    // Synchronization is defined as functions where all processes and threads entering them will leave at the same time.
  const uint SEMANTIC_LABEL_DATA_TRANSFER = 0x20 | SEMANTIC_LABEL_COMMUNICATION; // Internal functions in communication libraries that transfer data.
  const uint SEMANTIC_LABEL_WAIT          = 0x30 | SEMANTIC_LABEL_COMMUNICATION; // Internal functions in communication libraries that wait for various reasons. 

  // Further break downs.
  const uint SEMANTIC_LABEL_MESSAGE_TRANSFER  = 0x100 | SEMANTIC_LABEL_DATA_TRANSFER; // message transfer.
  const uint SEMANTIC_LABEL_IN_MEMORY_COPY    = 0x200 | SEMANTIC_LABEL_DATA_TRANSFER; // in-memory data copy.
  const uint SEMANTIC_LABEL_DEVICE_TRANSFER   = 0x300 | SEMANTIC_LABEL_DATA_TRANSFER; // data transfer with devices.
  const uint SEMANTIC_LABEL_IO                = 0x400 | SEMANTIC_LABEL_DATA_TRANSFER; // I/O
  
  const uint SEMANTIC_LABEL_WAIT_SEND_RECV  = 0x100 | SEMANTIC_LABEL_WAIT; // wait for sender/receiver
  const uint SEMANTIC_LABEL_WAIT_LOCK       = 0x200 | SEMANTIC_LABEL_WAIT; // wait for lock
  const uint SEMANTIC_LABEL_WAIT_RESOURCE   = 0x300 | SEMANTIC_LABEL_WAIT; // wait for various resources (e.g. message send buffers)
  
  typedef struct SEMANTIC_LABEL_ENTRY {
    const uint label;
    const string label_name;
  } SEMANTIC_LABEL_ENTRY;
  
  const SEMANTIC_LABEL_ENTRY SEMANTIC_LABEL_ARRAY[] = {
    {SEMANTIC_LABEL_COMPUTATION,      "computation"},
    {SEMANTIC_LABEL_COMMUNICATION,    "communication"},
    {SEMANTIC_LABEL_SYNC,             "synchronization"},
    {SEMANTIC_LABEL_DATA_TRANSFER,    "data transfer"},
    {SEMANTIC_LABEL_WAIT,             "wait"},
    {SEMANTIC_LABEL_MESSAGE_TRANSFER, "message transfer"},
    {SEMANTIC_LABEL_IN_MEMORY_COPY,   "in-memory data copy"},
    {SEMANTIC_LABEL_DEVICE_TRANSFER,  "data transfer with devices"},
    {SEMANTIC_LABEL_IO,               "I/O"},
    {SEMANTIC_LABEL_WAIT_SEND_RECV,   "wait for sender/receiver"},
    {SEMANTIC_LABEL_WAIT_LOCK,        "wait for lock"},
    {SEMANTIC_LABEL_WAIT_RESOURCE,    "wait for resource"}
  };
  
  const uint SEMANTIC_LABEL_ARRAY_SIZE = sizeof(SEMANTIC_LABEL_ARRAY) / sizeof(SEMANTIC_LABEL_ENTRY);
  
  inline string semanticLabelToString(uint label) {
    for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
      if (SEMANTIC_LABEL_ARRAY[i].label == label)
        return SEMANTIC_LABEL_ARRAY[i].label_name;
    return "invalid semantic label";
  }
  
  /*******************************************/
  
  typedef struct FUNC_SEMANTIC_INFO {
    const uint semantic_label; // semantic label for a function.
    const bool ignore_child;   // if children of the function should be ignored in trace analysis.
  } FUNC_SEMANTIC_INFO;
  
  FUNC_SEMANTIC_INFO getFuncSemanticInfo(string func_name);
}

#endif /* TCT_SEMANTIC_LABEL_HPP */

