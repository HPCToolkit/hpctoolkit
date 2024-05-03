// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_DIRECTCLASSIFICATION_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_DIRECTCLASSIFICATION_H

#include "../finalizer.hpp"

#include "../util/range_map.hpp"

#include <map>

namespace hpctoolkit::finalizers {

// For Module Classification, drawing the data mostly directly from the file
// itself. This handles the little details.
class DirectClassification final : public ProfileFinalizer {
public:
  // `dwarfThreshold` is in the units of bytes.
  // If dwarfThreshold == std::numeric_limits<uintmax_t>::max(), no limit.
  DirectClassification(uintmax_t dwarfThreshold);

  void notifyPipeline() noexcept override;
  ExtensionClass provides() const noexcept override { return ExtensionClass::classification; }
  ExtensionClass requirements() const noexcept override { return ExtensionClass::resolvedPath; }

  std::optional<std::pair<util::optional_ref<Context>, Context&>>
  classify(Context&, NestedScope&) noexcept override;

private:
  struct udModule final {
    // Storage for DWARF function data
    std::unordered_map<uint64_t, Function> functions;
    using trienode = std::pair<std::pair<Scope, Relation>, const void* /* const trienode* */>;
    std::deque<trienode> trie;
    std::map<util::interval<uint64_t>, const trienode&> leaves;

    // Storage for DWARF linemap data
    using line = std::pair<util::reference_index<const File>, uint64_t>;
    util::range_map<uint64_t, std::optional<line>,
                    util::range_merge::truthy<void,
                      util::range_merge::min<>>> lines;

    // Storage for ELF symbols
    std::multimap<util::interval<uint64_t>, Function> symbols;
  };

  uintmax_t dwarfThreshold;
  Module::ud_t::typed_member_t<udModule> ud;
  void load(const Module&, udModule&) noexcept;
  bool fullDwarf(void* dw, const Module&, udModule&);
  bool symtab(void* elf, const Module&, udModule&);
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_DIRECTCLASSIFICATION_H
