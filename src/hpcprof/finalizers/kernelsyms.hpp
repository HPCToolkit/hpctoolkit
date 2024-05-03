// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_KERNELSYMS_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_KERNELSYMS_H

#include "../finalizer.hpp"

#include "../util/range_map.hpp"

#include "../stdshim/filesystem.hpp"
#include <map>

namespace hpctoolkit::finalizers {

// Some Modules (in particular Linux kernels) don't have symbol tables in the
// ELF format, instead they need to be pulled from an nm-like dump saved in the measurements.
class KernelSymbols final : public ProfileFinalizer {
public:
  KernelSymbols();

  void notifyPipeline() noexcept override;
  ExtensionClass provides() const noexcept override { return ExtensionClass::classification; }
  ExtensionClass requirements() const noexcept override { return {}; }

  std::optional<std::pair<util::optional_ref<Context>, Context&>>
  classify(Context&, NestedScope&) noexcept override;

private:
  struct first final {
    void operator()(Function&, const Function&) const noexcept {}
  };

  struct udModule final {
    util::range_map<uint64_t, Function, first> symbols;
  };

  Module::ud_t::typed_member_t<udModule> ud;
  void load(const Module&, udModule&) noexcept;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_KERNELSYMS_H
