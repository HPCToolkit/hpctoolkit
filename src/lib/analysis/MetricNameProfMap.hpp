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
// Copyright ((c)) 2002-2018, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Analysis_MetricNameProfMap_hpp 
#define Analysis_MetricNameProfMap_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>
#include <sstream>

//*************************** User Include Files ****************************

#include <include/uint.h>

//#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Struct-Tree.hpp>


namespace Analysis {

class MetricNameProfMap {
 public:
  MetricNameProfMap(Prof::Metric::Mgr *mgr) : _mgr(mgr) {}

  void init();

  std::vector<int> metric_ids(const std::string &metric_name, bool inclusive = true);

  std::map<std::string, std::pair<int, int> > metrics(size_t mpi_rank, size_t thread_id) {
    return _metric_name_prof_maps[mpi_rank][thread_id];
  }

  int metric_id(size_t mpi_rank, size_t thread_id, const std::string &metric_name, bool inclusive = true);
  
  int num_mpi_ranks() {
    return _metric_name_prof_maps.size();
  }

  int num_thread_ids(size_t mpi_rank) {
    return _metric_name_prof_maps[mpi_rank].size();
  }

  bool add(const std::string &metric_name);

  const std::string name(size_t metric_id) {
    std::string ret;
    if (metric_id < _mgr->size()) {
      ret = _mgr->metric(metric_id)->name();
    }
    return ret;
  }

  const std::string to_string() const {
    std::stringstream ss;
    for (auto &mpi_iter : _metric_name_prof_maps) {
      auto mpi_rank = mpi_iter.first;
      for (auto &thread_iter : mpi_iter.second) {
        auto thread_id = thread_iter.first;
        for (auto &metric_iter : thread_iter.second) {
          auto &metric_name = metric_iter.first;
          ss << "[" << mpi_rank << "," << thread_id << "]: " << metric_name << " (" <<
            metric_iter.second.first << "," << metric_iter.second.second << ")" << std::endl;
        }
      }
    }
    return ss.str();
  }

 private:
  Prof::Metric::Mgr *_mgr;
  std::map<int, std::map<int, std::map<std::string, std::pair<int, int> > > > _metric_name_prof_maps;
};

}  // Analysis

#endif  // Analysis_MetricNameProfMap_hpp
