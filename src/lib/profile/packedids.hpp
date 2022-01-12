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
// Copyright ((c)) 2019-2022, Rice University
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
    return DataClass::references + DataClass::contexts + DataClass::attributes;
  }
  ExtensionClass requires() const noexcept override {
    return ExtensionClass::identifier + ExtensionClass::mscopeIdentifiers;
  }
  DataClass wavefronts() const noexcept override {
    return DataClass::references + DataClass::contexts + DataClass::attributes;
  }

  void notifyPipeline() noexcept override;
  void notifyContextExpansion(const Context&, Scope, const Context&) override;
  void notifyWavefront(DataClass) override;
  void write() override {};

protected:
  virtual void notifyPacked(std::vector<uint8_t>&&) = 0;

private:
  std::atomic<std::size_t> stripcnt;
  struct ctxonce {
    ctxonce(const Context&, IdPacker&) {};
    util::locked_unordered_set<Scope> seen;
  };
  Context::ud_t::typed_member_t<ctxonce> udOnce;

  std::atomic<std::size_t> buffersize;
  std::array<std::pair<std::mutex, std::vector<uint8_t>>, 256> stripbuffers;
};

class IdUnpacker final : public ProfileFinalizer {
public:
  IdUnpacker(std::vector<uint8_t>&&);
  ~IdUnpacker() = default;

  ExtensionClass provides() const noexcept override {
    return ExtensionClass::identifier + ExtensionClass::mscopeIdentifiers + ExtensionClass::classification;
  }
  ExtensionClass requires() const noexcept override { return {}; }

  std::optional<unsigned int> identify(const Context&) noexcept override;
  std::optional<unsigned int> identify(const Metric&) noexcept override;
  std::optional<Metric::ScopedIdentifiers> subidentify(const Metric&) noexcept override;

  util::optional_ref<Context> classify(Context&, Scope&) noexcept override;

private:
  void unpack() noexcept;
  std::vector<uint8_t> ctxtree;

  std::once_flag once;
  const Module* exmod;
  const File* exfile;
  std::unique_ptr<Function> exfunc;
  std::vector<std::reference_wrapper<const Module>> modmap;
  unsigned int globalid;
  std::unordered_map<unsigned int, std::unordered_map<Scope, std::vector<Scope>>> exmap;

  std::unordered_map<std::string, std::pair<unsigned int, Metric::ScopedIdentifiers>> metmap;
};

}

#endif  // HPCTOOLKIT_PROFILE_PACKEDIDS_H
