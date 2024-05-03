// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_PACKEDIDS_H
#define HPCTOOLKIT_PROFILE_PACKEDIDS_H

#include "sink.hpp"
#include "finalizer.hpp"
#include "sources/packed.hpp"

namespace hpctoolkit {

class IdPacker : public ProfileSink {
public:
  IdPacker();
  ~IdPacker() = default;

  DataClass accepts() const noexcept override {
    return DataClass::contexts + DataClass::attributes;
  }
  ExtensionClass requirements() const noexcept override {
    return ExtensionClass::identifier;
  }
  DataClass wavefronts() const noexcept override {
    return DataClass::contexts + DataClass::attributes;
  }

  void notifyWavefront(DataClass) override;
  void write() override {};

protected:
  virtual void notifyPacked(std::vector<uint8_t>&&) = 0;
};

class IdUnpacker final : public ProfileFinalizer {
public:
  IdUnpacker(std::vector<uint8_t>&&);
  ~IdUnpacker() = default;

  ExtensionClass provides() const noexcept override {
    return ExtensionClass::identifier + ExtensionClass::classification;
  }
  ExtensionClass requirements() const noexcept override { return {}; }

  std::optional<unsigned int> identify(const Context&) noexcept override;
  std::optional<Metric::Identifier> identify(const Metric&) noexcept override;

private:
  void unpack() noexcept;
  std::vector<uint8_t> ctxtree;

  std::once_flag once;
  unsigned int globalid;
  std::unordered_map<unsigned int, std::pair<std::uint8_t, std::unordered_map<std::uint64_t, unsigned int>>> idmap;
  std::unordered_map<std::string, unsigned int> metmap;
};

}

#endif  // HPCTOOLKIT_PROFILE_PACKEDIDS_H
