//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
#include <climits>
#include <cstring>
#include <cstdio>

#include <typeinfo>
#include <unordered_map>
#include <algorithm>
#include <queue>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>

#include "MetricNameProfMap.hpp"

using std::string;

#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Struct-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Metric-ADesc.hpp>

#include <lib/profxml/XercesUtil.hpp>
#include <lib/profxml/PGMReader.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/banal/gpu/DotCFG.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>

#define DEBUG_METRIC_NAME_PROF_MAP 0

namespace Analysis {

void MetricNameProfMap::init() {
  for (size_t i = 0; i < _mgr->size(); ++i) {
    auto suffix = _mgr->metric(i)->nameSfx();
    auto prefix = _mgr->metric(i)->namePfxBase();
    auto mpi_rank = 0;
    auto thread_id = 0;
    auto pos = suffix.find(",");
    // [xx,xx]
    // 0123456
    if (pos != std::string::npos) {
      auto s = suffix.substr(1, pos - 1);
      mpi_rank = std::stoi(s);
      s = suffix.substr(pos + 1, suffix.size() - pos - 2);
      thread_id = std::stoi(s);
    }

    if (_mgr->metric(i)->type() == Prof::Metric::ADesc::TyIncl) {
      _metric_name_prof_maps[mpi_rank][thread_id][prefix].first = i;
    } else {
      _metric_name_prof_maps[mpi_rank][thread_id][prefix].second = i;
    }

    if (DEBUG_METRIC_NAME_PROF_MAP) {
      std::cout << "Init metric [" << mpi_rank << ", " <<
        thread_id << "] " << prefix << std::endl;
    }
  }
}


std::vector<int> MetricNameProfMap::metric_ids(const std::string &metric_name, bool inclusive) {
  std::vector<int> ids;
  for (auto mpi_iter = _metric_name_prof_maps.begin(); mpi_iter != _metric_name_prof_maps.end();
    ++mpi_iter) {
    for (auto thread_iter = mpi_iter->second.begin(); thread_iter != mpi_iter->second.end(); ++thread_iter) {
      auto &metric_name_prof_map = thread_iter->second;
      auto iter = metric_name_prof_map.find(metric_name);
      if (iter != metric_name_prof_map.end()) {
        if (inclusive) {
          ids.push_back(iter->second.first);
        } else {
          ids.push_back(iter->second.second);
        }
      }
    }
  }
  return ids;
}


int MetricNameProfMap::metric_id(size_t mpi_rank, size_t thread_id, const std::string &metric_name, bool inclusive) {
  auto mpi_iter = _metric_name_prof_maps.find(mpi_rank);
  if (mpi_iter == _metric_name_prof_maps.end()) {
    return -1;
  }
  
  auto &thread_metric_prof_map = mpi_iter->second;
  auto thread_iter = thread_metric_prof_map.find(thread_id);
  if (thread_iter == thread_metric_prof_map.end()) {
    return -1;
  }

  auto &metric_name_prof_map = thread_iter->second;
  auto metric_iter = metric_name_prof_map.find(metric_name);
  if (metric_iter == metric_name_prof_map.end()) {
    return -1;
  }

  if (inclusive) {
    return metric_iter->second.first;
  } else {
    return metric_iter->second.second;
  }
}


bool MetricNameProfMap::add(const std::string &metric_name) {
  bool res = false;
  for (auto mpi_iter = _metric_name_prof_maps.begin(); mpi_iter != _metric_name_prof_maps.end();
    ++mpi_iter) {
    int mpi_rank = mpi_iter->first; 
    for (auto thread_iter = mpi_iter->second.begin(); thread_iter != mpi_iter->second.end(); ++thread_iter) {
      auto &metric_name_prof_map = thread_iter->second;
      int thread_id = thread_iter->first;
      auto suffix = "[" + std::to_string(mpi_rank) + "," + std::to_string(thread_id) + "]";
      if (metric_name_prof_map.find(metric_name) == metric_name_prof_map.end()) {
        auto *in_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
        auto *ex_desc = new Prof::Metric::ADesc(metric_name.c_str(), NULL);
        in_desc->type(Prof::Metric::ADesc::TyIncl);
        ex_desc->type(Prof::Metric::ADesc::TyExcl);
        in_desc->partner(ex_desc);
        ex_desc->partner(in_desc);
        in_desc->nameSfx(suffix);
        ex_desc->nameSfx(suffix);

        if (_mgr->insertIf(in_desc) && _mgr->insertIf(ex_desc)) {
          auto in_id = in_desc->id();
          auto ex_id = ex_desc->id();
          // Add new metric names
          metric_name_prof_map[metric_name] = std::pair<int, int>(in_id, ex_id);
          res = true;
        }
      }
    }
  }
  return res;
}

}  // namespace Analysis
