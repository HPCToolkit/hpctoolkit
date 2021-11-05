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
// Copyright ((c)) 2019-2020, Rice University
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

#include "metricsyaml.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>

#include "lib/profile/sinks/METRICS.yaml.inc"

using namespace hpctoolkit;
using namespace sinks;
namespace fs = stdshim::filesystem;

MetricsYAML::MetricsYAML(stdshim::filesystem::path p)
  : dir(std::move(p)) {};

void MetricsYAML::notifyWavefront(DataClass dc) {
  assert(dc.hasAttributes());
  if(dir.empty()) return;  // Dry-run mode
  auto outdir = dir / "metrics";

  // Create the directory and write out all the files
  stdshim::filesystem::create_directories(outdir);
  {
    std::ofstream example(outdir / "METRICS.yaml.ex");
    example << METRICS_yaml;
  }
  {
    std::ofstream out(outdir / "raw.yaml");
    raw(out);
  }
  {
    std::ofstream out(outdir / "default.yaml");
    standard(out);
  }
}

// "Raw" version where all Metrics are output fairly verbatim
void MetricsYAML::raw(std::ostream& os) const {
  YAML::Emitter out(os);
  out << YAML::BeginMap;
  out << YAML::Key << "foo";
  out << YAML::Value << "bar";
  out << YAML::EndMap;
  // TODO
}

// Standard version where we try our best to fit every Metric somewhere reasonable
void MetricsYAML::standard(std::ostream& os) const {
  YAML::Emitter out(os);
  out << "WIP TODO";
}
