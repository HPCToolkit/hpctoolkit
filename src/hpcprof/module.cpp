// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "module.hpp"

#include "util/stable_hash.hpp"

using namespace hpctoolkit;

Module::Module(ud_t::struct_t& rs)
  : userdata(rs, std::cref(*this)) {}
Module::Module(Module&& m)
  : userdata(std::move(m.userdata), std::cref(*this)),
    u_path(std::move(m.u_path)),
    u_relative_path(std::move(m.u_relative_path)) {}
Module::Module(ud_t::struct_t& rs, stdshim::filesystem::path p)
  : userdata(rs, std::cref(*this)), u_path(std::move(p)) {}
Module::Module(ud_t::struct_t& rs, stdshim::filesystem::path p, stdshim::filesystem::path rp)
  : userdata(rs, std::cref(*this)), u_path(std::move(p)), u_relative_path(std::move(rp)) {}

util::stable_hash_state& hpctoolkit::operator<<(util::stable_hash_state& h, const Module& m) noexcept {
  return h << m.path();
}
