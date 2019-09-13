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
// Copyright ((c)) 2019-2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_METRIC_H
#define HPCTOOLKIT_PROFILE_METRIC_H

#include "context.hpp"

#include "util/atomic_unordered.hpp"
#include "util/locked_unordered.hpp"
#include "util/uniqable.hpp"
#include "util/ragged_vector.hpp"

#include <atomic>
#include <vector>

namespace hpctoolkit {

// Just a simple metric class, nothing to see here
class Metric {
private:
  struct Datum {
    Datum() : exclusive(0), inclusive(0), shared(0) {};
    ~Datum() = default;
    std::atomic<double> exclusive;
    std::atomic<double> inclusive;
    std::atomic<double> shared;  // For reducing contention
  };

public:
  using ud_t = internal::ragged_vector<const Metric&>;

  Metric(ud_t::struct_t& rs, Context::met_t::struct_t& ms, const std::string& n, const std::string& d)
    : Metric(rs, ms, std::string(n), std::string(d)) {};
  Metric(ud_t::struct_t& rs, Context::met_t::struct_t& ms, std::string&& n, std::string&& d)
    : userdata(rs, std::cref(*this)), u_ident(std::move(n), std::move(d)),
      member(ms.add<Datum>()) {};
  Metric(Metric&& m)
    : userdata(std::move(m.userdata), std::cref(*this)),
      u_ident(std::move(m.u_ident)), member(std::move(m.member)) {};

  const std::string& name() const { return u_ident().name; }
  const std::string& description() const { return u_ident().description; }

  struct ident {
    ident(const std::string& n, const std::string& d) : name(n), description(d) {};
    ident(std::string&& n, std::string&& d) : name(std::move(n)), description(std::move(d)) {};
    std::string name;
    std::string description;
    bool operator==(const ident& o) const noexcept {
      return name == o.name && description == o.description;
    }
  };

  mutable ud_t userdata;

  // Add metric data to a Context, as per the usual.
  void add(Context&, double) const noexcept;

  // Get the data for this metric for a particular Context.
  std::pair<double, double> getFor(const Context&) const noexcept;

private:
  internal::uniqable_key<ident> u_ident;

  Context::met_t::typed_member_t<Datum> member;

  friend class internal::uniqued<Metric>;
  internal::uniqable_key<ident>& uniqable_key() { return u_ident; }
};

}

namespace std {
  using namespace hpctoolkit;
  template<> struct hash<Metric::ident> {
    std::size_t operator()(const Metric::ident&) const noexcept;
  };
}

#endif  // HPCTOOLKIT_PROFILE_METRIC_H
