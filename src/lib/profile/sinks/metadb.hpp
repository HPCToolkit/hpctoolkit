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
// Copyright ((c)) 2002-2023, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_SINKS_MetaDB_H
#define HPCTOOLKIT_PROFILE_SINKS_MetaDB_H

#include "../sink.hpp"

#include "../util/file.hpp"

#include <mutex>
#include "../stdshim/filesystem.hpp"

namespace hpctoolkit::sinks {

class MetaDB final : public ProfileSink {
public:
  MetaDB() = default;

  MetaDB(stdshim::filesystem::path dir, bool copySources);

  static std::string accumulateFormulaString(const Expression&);

  void notifyContext(const Context&) override;

  void write() override;

  DataClass accepts() const noexcept override {
    using namespace hpctoolkit::literals::data;
    return attributes + references + contexts + metrics;
  }

  ExtensionClass requires() const noexcept override {
    using namespace hpctoolkit::literals::extensions;
    Class ret = classification + identifier;
    if(copySources) ret += resolvedPath;
    return ret;
  }

  void notifyPipeline() noexcept override;

  // Check whether the given Context should be elided from the output
  // TODO: Remove once the Viewer can handle instruction-grain data
  static bool elide(const Context& c) noexcept {
    return c.direct_parent()
           && c.scope().relation() == Relation::enclosure
           && c.scope().flat().type() == Scope::Type::point;
  }

private:
  stdshim::filesystem::path dir;
  std::optional<util::File> metadb;
  bool copySources;

  std::shared_mutex stringsLock;
  std::unordered_map<std::string, std::size_t> stringsTable;
  std::deque<std::string_view> stringsList;

  std::size_t stringsTableLookup(const std::string&);

  struct udFile {
    std::once_flag once;
    std::size_t pathSIdx = std::numeric_limits<std::size_t>::max();
    bool copied : 1;
    uint64_t ptr = std::numeric_limits<uint64_t>::max();
  };
  void instance(const File&);

  struct udModule {
    std::once_flag once;
    std::size_t pathSIdx = std::numeric_limits<std::size_t>::max();
    uint64_t ptr = std::numeric_limits<uint64_t>::max();
  };
  void instance(const Module&);

  struct udFunction {
    std::size_t nameSIdx = std::numeric_limits<std::size_t>::max();
    uint64_t ptr = std::numeric_limits<uint64_t>::max();
  };
  void instance(const Function&);
  void instance(Scope::placeholder_t, const Scope&);

  struct udContext {
    uint64_t szChildren = 0;
    uint64_t pChildren = 0;
    uint16_t propagation = 0;
    std::size_t prettyNameSIdx = std::numeric_limits<std::size_t>::max();
    uint16_t entryPoint = std::numeric_limits<uint16_t>::max();
  };

  struct {
    File::ud_t::typed_member_t<udFile> file;
    const auto& operator()(File::ud_t&) const noexcept { return file; }
    Module::ud_t::typed_member_t<udModule> module;
    const auto& operator()(Module::ud_t&) const noexcept { return module; }
    Context::ud_t::typed_member_t<udContext> context;
    const auto& operator()(Context::ud_t&) const noexcept { return context; }
  } ud;
  util::locked_unordered_map<util::reference_index<const Function>, udFunction> udFuncs;
  util::locked_unordered_map<uint64_t, udFunction> udPlaceholders;
};

}  // namespace hpctoolkit::sinks

#endif  // HPCTOOLKIT_PROFILE_SINKS_MetaDB_H
