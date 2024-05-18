// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
  ExtensionClass requirements() const noexcept override { return {}; }

private:
  stdshim::filesystem::path dir;

  void standard(std::ostream&);
};

}

#endif  // HPCTOOLKIT_PROFILE_SINKS_MetricsYAML_H
