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

#ifndef HPCTOOLKIT_PROFILE_SINKS_MetricsYAML_H
#define HPCTOOLKIT_PROFILE_SINKS_MetricsYAML_H

#include "../sink.hpp"

#include <iosfwd>
#include "../stdshim/filesystem.hpp"

namespace hpctoolkit::sinks {

/// ProfileSink to generate the metrics/*.yaml files for an HPCToolkit database.
class MetricsYAML final : public ProfileSink {
public:
  ~MetricsYAML() = default;

  /// Constructor, with a reference to the output database directory.
  MetricsYAML(stdshim::filesystem::path);

  // No-op write(), everything is performed during the attributes wavefront.
  void write() override {};

  void notifyWavefront(DataClass) override;

  DataClass accepts() const noexcept override {
    using namespace hpctoolkit::literals::data;
    return attributes;
  }
  DataClass wavefronts() const noexcept override { return accepts(); }
  ExtensionClass requires() const noexcept override { return {}; }

  static std::string fullName(const Metric&, const StatisticPartial&, MetricScope);

private:
  stdshim::filesystem::path dir;

  std::string anchorName(const Metric&, const StatisticPartial&, MetricScope);
  void raw(std::ostream&);
  void standard(std::ostream&);
};

}

#endif  // HPCTOOLKIT_PROFILE_SINKS_MetricsYAML_H
