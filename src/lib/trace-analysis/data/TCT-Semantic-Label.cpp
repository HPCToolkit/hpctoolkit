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
 * File:   TCT-Semantic-Label.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on August 15, 2018, 12:20 AM
 */

#include "TCT-Semantic-Label.hpp"
#include "../TraceAnalysisCommon.hpp"

#include <string>
using std::string;

#include <regex>
using std::regex;
using std::regex_match;
using std::regex_constants::icase;
using std::regex_constants::optimize;

namespace TraceAnalysis {
  // A rule that maps a function to its semantic label.
  typedef struct FUNC_TO_LABEL_RULE {
    const regex func_name_regex;  // regular expression of function name
    const uint label;             // semantic label for functions matching the regular expression
    const uint priority;          // when a function matches multiple rules, only the one with the highest priority is picked.
    const bool ignore_child;      // if children of the function should be ignored in trace analysis.
  } FUNC_TO_LABEL_RULE;
  
  // An array of such rules
  const FUNC_TO_LABEL_RULE FUNC_TO_LABEL_RULE_ARRAY[] = {
    {regex("(.*)"), SEMANTIC_LABEL_COMPUTATION, 0, false},
    //  *********  Calls to MPI  *********
    {regex("P?MPI_(.+)", icase | optimize), SEMANTIC_LABEL_COMMUNICATION, 1,  false},
    
    // Calls to MPI collective operations where all ranks entering them will leave at the same time.
    {regex("P?MPI_Barrier",       icase | optimize),  SEMANTIC_LABEL_SYNC, 2, true},
    {regex("P?MPI_Allgather(.*)", icase | optimize),  SEMANTIC_LABEL_SYNC, 2, true},
    {regex("P?MPI_Allreduce",     icase | optimize),  SEMANTIC_LABEL_SYNC, 2, true},
    {regex("P?MPI_Alltoall(.*)",  icase | optimize),  SEMANTIC_LABEL_SYNC, 2, true},
    
    // Calls to MPI parallel I/O
    {regex("P?MPI_File_(.+)",             icase | optimize),  SEMANTIC_LABEL_IO,    2, true},
    {regex("P?MPI_File_open",             icase | optimize),  SEMANTIC_LABEL_SYNC,  3, true},
    {regex("P?MPI_File_close",            icase | optimize),  SEMANTIC_LABEL_SYNC,  3, true},
    {regex("P?MPI_File_read_all(.*)",     icase | optimize),  SEMANTIC_LABEL_SYNC,  3, true},
    {regex("P?MPI_File_read_at_all(.*)",  icase | optimize),  SEMANTIC_LABEL_SYNC,  3, true},
    {regex("P?MPI_File_write_all(.*)",    icase | optimize),  SEMANTIC_LABEL_SYNC,  3, true},
    {regex("P?MPI_File_write_at_all(.*)", icase | optimize),  SEMANTIC_LABEL_SYNC,  3, true},
    
    // Implementation in MPICH that corresponds to wait for sender/receiver.
    {regex("MPIDI_CH3I_Progress(.*)",       icase | optimize),  SEMANTIC_LABEL_WAIT_SEND_RECV,  1, true},
    {regex("MPID_nem_mpich_test_recv(.*)",  icase | optimize),  SEMANTIC_LABEL_WAIT_SEND_RECV,  1, true},
    {regex("MPID_nem_mpich_blocking_recv",  icase | optimize),  SEMANTIC_LABEL_WAIT_SEND_RECV,  1, true},
    
    // Implementation in MPICH that corresponds to message send/recv.
    {regex("MPID_nem_lmt_shm_start_send", icase | optimize),  SEMANTIC_LABEL_MESSAGE_TRANSFER,  1, true},
    {regex("MPID_nem_lmt_shm_start_recv", icase | optimize),  SEMANTIC_LABEL_MESSAGE_TRANSFER,  1, true}
  };
  
  const uint FUNC_TO_LABEL_ARRAY_SIZE = sizeof(FUNC_TO_LABEL_RULE_ARRAY) / sizeof(FUNC_TO_LABEL_RULE);
  
  FUNC_SEMANTIC_INFO getFuncSemanticInfo(string func_name) {
    uint idx = 0;
    for (uint i = 1; i < FUNC_TO_LABEL_ARRAY_SIZE; i++)
      if (regex_match(func_name, FUNC_TO_LABEL_RULE_ARRAY[i].func_name_regex)
            && FUNC_TO_LABEL_RULE_ARRAY[i].priority >= FUNC_TO_LABEL_RULE_ARRAY[idx].priority) {
        if (FUNC_TO_LABEL_RULE_ARRAY[i].priority == FUNC_TO_LABEL_RULE_ARRAY[idx].priority)
          print_msg(MSG_PRIO_HIGH, "ERROR: func %s is mapped to two semantic labels.\n", func_name.c_str());
        idx = i;
      }
    FUNC_SEMANTIC_INFO ret = {FUNC_TO_LABEL_RULE_ARRAY[idx].label, FUNC_TO_LABEL_RULE_ARRAY[idx].ignore_child};
    return ret;
  }
}