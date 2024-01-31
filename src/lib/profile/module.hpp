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
  ~Module() = default;

  mutable ud_t userdata;

  // Absolute path to the module in question
  const stdshim::filesystem::path& path() const { return u_path; }

private:
  util::uniqable_key<stdshim::filesystem::path> u_path;

  friend class util::uniqued<Module>;
  util::uniqable_key<stdshim::filesystem::path>& uniqable_key() {
    return u_path;
  }
};

util::stable_hash_state& operator<<(util::stable_hash_state&, const Module&) noexcept;

}

#endif  // HPCTOOLKIT_PROFILE_MODULE_H
