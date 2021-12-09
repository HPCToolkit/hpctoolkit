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
// Copyright ((c)) 2021, Rice University
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

#include "hatchet-literal.hpp"

#include <iomanip>
#include <fstream>
#include <stack>

using namespace hpctoolkit;
using namespace sinks;
namespace fs = stdshim::filesystem;

HatchetLiteral::HatchetLiteral(const stdshim::filesystem::path& out)
  : out(out) {};

void HatchetLiteral::write() {
  std::stack<bool, std::vector<bool>> firstStack;
  firstStack.push(true);

  bool ignore_children = false;
  std::ofstream f(out);
  f << "[";
  src.contexts().citerate([&](const Context& c){
    if (ignore_children) {
      return;
    }

    if(!firstStack.top()) f << ",";
    firstStack.top() = false;

    std::ostringstream ss;
    ss << c.scope();

    if (ss.str().find("GPUKernl") != std::string::npos) {
      ignore_children = true; 
    }

    f << "{\"frame\":{\"name\":" << std::quoted(ss.str()) << ","
                       "\"type\":\"function\"},"
          "\"metrics\":{";
    bool first = true;
    for(const auto& [m, val]: c.statistics().citerate()) {
      if (m->name() != "GINS") {
        continue;
      }
      // We only want the :Sum Statistic, so figure out which Partial that is.
      util::optional_ref<const StatisticPartial> part;
      for(const auto& s: m->statistics()) {
        if(s.suffix() == "Sum") {
          part = m->partials().at(std::get<std::size_t>(s.finalizeFormula().front()));
          break;
        }
      }
      assert(part);

      auto acc = val.get(*part);
      auto scopes = m->scopes();
      auto handle = [&](MetricScope ms, const std::string& suffix) {
        if(!scopes.has(ms)) return;
        if(!first) f << ",";
        first = false;
        f << "\"" << m->name() << suffix << "\":" << acc.get(ms).value_or(0);
      };
      if (ss.str().find("GPUKernl") != std::string::npos) {
        handle(MetricScope::execution, " (E)");
        handle(MetricScope::execution, " (I)");
        handle(MetricScope::execution, " (RAW)");
      } else {
        handle(MetricScope::execution, " (I)");
        handle(MetricScope::function, " (E)");
        handle(MetricScope::point, " (RAW)");
      }
    }
    f << "},\"children\":[";

    firstStack.push(true);
  }, [&](const Context& c) {
    std::ostringstream ss;
    ss << c.scope();

    if (ss.str().find("GPUKernl") != std::string::npos) {
      ignore_children = false;
    }

    if (ignore_children) {
      return;
    }

    f << "]}";
    firstStack.pop();
  });
  f << "]";
}
