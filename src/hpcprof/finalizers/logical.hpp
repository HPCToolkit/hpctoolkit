// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_LOGICAL_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_LOGICAL_H

#include "../finalizer.hpp"

namespace hpctoolkit::finalizers {

class LogicalFile final : public ProfileFinalizer {
public:
  LogicalFile();
  ~LogicalFile() = default;

  ExtensionClass provides() const noexcept override { return ExtensionClass::classification; }
  ExtensionClass requirements() const noexcept override { return ExtensionClass::resolvedPath; }

  void notifyPipeline() noexcept override;

  std::optional<std::pair<util::optional_ref<Context>, Context&>>
  classify(Context&, NestedScope&) noexcept override;

private:
  using funcTable = util::locked_unordered_set<Function>;
  util::locked_unordered_map<std::string, funcTable, std::mutex> funcTableMap;

  struct udModule {
    bool isLogical = false;
    std::unordered_map<uint32_t,
        std::variant<std::reference_wrapper<const File>,
                     std::reference_wrapper<const Function>>
    > map;
  };
  Module::ud_t::typed_member_t<udModule> ud;

  void load(const Module&, udModule&) noexcept;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_LOGICAL_H
