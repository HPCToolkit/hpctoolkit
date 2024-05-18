// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_MODULE_H
#define HPCTOOLKIT_PROFILE_MODULE_H

#include "scope.hpp"
#include "util/ragged_vector.hpp"
#include "util/locked_unordered.hpp"
#include "util/once.hpp"

#include "stdshim/filesystem.hpp"
#include <forward_list>
#include <future>
#include <map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace hpctoolkit {

namespace util {
class stable_hash_state;
}

// Just a simple load module class, nothing to see here
class Module {
public:
  using ud_t = util::ragged_vector<const Module&>;

  Module(ud_t::struct_t& rs);
  Module(Module&& m);
  Module(ud_t::struct_t& rs, stdshim::filesystem::path p);
  Module(ud_t::struct_t& rs, stdshim::filesystem::path p, stdshim::filesystem::path rp);
  ~Module() = default;

  mutable ud_t userdata;

  // Absolute path to the module in question
  const stdshim::filesystem::path& path() const { return u_path; }

  // Relative path to the module in question
  const stdshim::filesystem::path& relative_path() const { return u_relative_path; }

private:
  util::uniqable_key<stdshim::filesystem::path> u_path;
  const stdshim::filesystem::path u_relative_path;

  friend class util::uniqued<Module>;
  util::uniqable_key<stdshim::filesystem::path>& uniqable_key() {
    return u_path;
  }
};

util::stable_hash_state& operator<<(util::stable_hash_state&, const Module&) noexcept;

}

#endif  // HPCTOOLKIT_PROFILE_MODULE_H
