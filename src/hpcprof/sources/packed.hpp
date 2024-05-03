// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_SOURCES_PACKED_H
#define HPCTOOLKIT_PROFILE_SOURCES_PACKED_H

#include "../source.hpp"
#include "../sink.hpp"
#include "../util/locked_unordered.hpp"

namespace hpctoolkit::sources {

/// Base class for ProfileSources that input byte-packed data. Data can be
/// unpacked after transfer, and emitted into the current Pipeline.
class Packed : public ProfileSource {
public:
  Packed();
  ~Packed() = default;

  /// Helper Sink to fill the identifier to Context table.
  class IdTracker : public ProfileSink {
  public:
    IdTracker() = default;
    ~IdTracker() = default;

    DataClass accepts() const noexcept override {
      return DataClass::attributes + DataClass::contexts;
    }
    ExtensionClass requirements() const noexcept override {
      return ExtensionClass::identifier;
    }

    void write() override;

    void notifyContext(const Context&) noexcept override;
    void notifyMetric(const Metric&) noexcept override;

  private:
    friend class Packed;
    util::locked_unordered_map<std::uint64_t, std::reference_wrapper<Context>> contexts;
    util::locked_unordered_map<std::uint64_t, std::reference_wrapper<const Metric>> metrics;
  };

  /// The given IdTracker should be in the same Pipeline as this Source, and
  /// IdPacker/IdUnpacker should be used to keep ids consistent.
  Packed(const IdTracker&);

protected:
  // Everything done during unpacking is marked with an iterator.
  using iter_t = std::vector<std::uint8_t>::const_iterator;

  /// Unpacks and emits a vector's `attributes` data.
  // MT: Externally Synchronized
  iter_t unpackAttributes(iter_t) noexcept;

  /// Unpacks and emits a vector's `references` data.
  // MT: Externally Synchronized
  iter_t unpackReferences(iter_t) noexcept;

  /// Unpacks and emits a vector's `contexts` data.
  // MT: Externally Synchronized
  iter_t unpackContexts(iter_t) noexcept;

  /// Unpacks and emits a vector's `metrics` data.
  /// Note that this relies on identifiers being the same as on the writing end.
  // MT: Externally Synchronized
  iter_t unpackMetrics(iter_t) noexcept;

  /// Unpacks and emits a vector's `*Timepoints` data.
  /// Specifically the bounds, not much else.
  // MT: Externally Synchronized
  iter_t unpackTimepoints(iter_t) noexcept;

private:
  util::optional_ref<const IdTracker> tracker;
  std::vector<std::pair<std::reference_wrapper<Metric>, std::uint8_t>> metrics;
  std::vector<std::reference_wrapper<Module>> modules;
};

}

#endif  // HPCTOOLKIT_PROFILE_SOURCES_PACKED_H
