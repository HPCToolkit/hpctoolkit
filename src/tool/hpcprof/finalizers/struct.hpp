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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_STRUCT_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_STRUCT_H

#include "../finalizer.hpp"

#include "../util/range_map.hpp"

#include <deque>
#include <mutex>
#include <map>

namespace hpctoolkit::finalizers {

namespace detail {
struct LMData;
class StructFileParser;
}

// When a struct file is around, this draws data from it to Classify a Module.
class StructFile final : public ProfileFinalizer {
public:
  class RecommendationStore {
  public:
    RecommendationStore(bool measDir, std::string hpcstructArgs = "")
      : measDir(measDir), hpcstructArgs(std::move(hpcstructArgs)) {}
    ~RecommendationStore() = default;

  private:
    friend class StructFile;

    // If true, the Structfile is part of a measurements dir listed in hpcstructArgs.
    // If false, the binary path should be appended to any recommendation.
    bool measDir;
    // hpcstruct argument(s) to recommend along
    std::string hpcstructArgs;
    // Flag to ensure any recommendation made from this is only recommended once.
    std::once_flag once;
  };

  StructFile(stdshim::filesystem::path path, stdshim::filesystem::path meas, std::shared_ptr<RecommendationStore> store);
  ~StructFile();

  void notifyPipeline() noexcept override;
  ExtensionClass provides() const noexcept override {
    return ExtensionClass::classification;
  }
  ExtensionClass requirements() const noexcept override {
    return ExtensionClass::resolvedPath;
  }

  std::optional<std::pair<util::optional_ref<Context>, Context&>>
  classify(Context&, NestedScope&) noexcept override;
  bool resolve(ContextFlowGraph&) noexcept override;

  std::vector<stdshim::filesystem::path> forPaths() const;

private:
  std::shared_ptr<RecommendationStore> recstore;

  /// Current status of the call graph data from a Structfile
  enum class CallGraphStatus {
    NONE,  ///< Default status
    NOT_PRESENT,  ///< Call graph data was never requested or generated
    VALID,  ///< Call graph data was found and parsed successfully
    ERRORED,  ///< Call graph data was generated but failed to parse somehow
  };

  struct udModule final {
    // Storage for Function data
    std::deque<Function> funcs;
    using trienode = std::pair<std::pair<Scope, Relation>, const void* /* const trienode* */>;
    // Trie of Scopes, for efficiently storing nested Scopes
    std::deque<trienode> trie;
    // Bounds-map (instruction -> nested Scope and top Function)
    std::map<util::interval<uint64_t>, std::pair<
        std::reference_wrapper<const trienode>,
        std::reference_wrapper<const Function>>> leaves;

    // Status of the call graph data for
    CallGraphStatus cfgStatus = CallGraphStatus::NONE;
    // Reversed call graph (callee Function -> caller instruction and top Function)
    std::unordered_multimap<util::reference_index<const Function>,
        std::pair<uint64_t, std::reference_wrapper<const Function>>> rcg;
  };
  friend class hpctoolkit::finalizers::detail::StructFileParser;

  stdshim::filesystem::path path;
  stdshim::filesystem::path measDirPath;
  Module::ud_t::typed_member_t<udModule> ud;
  void load(const Module&, udModule&) noexcept;

  // Structfiles can have data on multiple load modules (LM tags), this maps
  // each binary path with the properly initialized Parser for that tag.
  std::mutex lms_lock;
  std::unordered_map<stdshim::filesystem::path,
      std::pair<std::unique_ptr<finalizers::detail::LMData>,
          std::unique_ptr<finalizers::detail::StructFileParser>>,
      stdshim::hash_path> lms;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_STRUCT_H
