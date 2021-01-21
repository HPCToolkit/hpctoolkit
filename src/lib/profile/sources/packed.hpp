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
// Copyright ((c)) 2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_SOURCES_PACKED_H
#define HPCTOOLKIT_PROFILE_SOURCES_PACKED_H

#include "../source.hpp"
#include "../finalizer.hpp"
#include "../util/locked_unordered.hpp"

namespace hpctoolkit::sources {

/// Base class for ProfileSources that input byte-packed data. Data can be
/// unpacked after transfer, and emitted into the current Pipeline.
class Packed : public ProfileSource {
public:
  Packed();
  ~Packed() = default;

protected:
  // Everything done during unpacking is marked with an iterator.
  using iter_t = std::vector<std::uint8_t>::const_iterator;

  /// Unpacks and emits a vector's `attributes` data.
  // MT: Externally Synchronized
  iter_t unpackAttributes(iter_t) noexcept;

  /// Unpacks and emits a vector's `references` data.
  // MT: Externally Synchronized
  iter_t unpackReferences(iter_t) noexcept;

  /// Unpacks and emits a vector's `contexts` data.
  // MT: Externally Synchronized
  iter_t unpackContexts(iter_t) noexcept;

public:
  /// Mapping of identifiers to the corrosponding Contexts. For unpackMetrics.
  using ctx_map_t = util::locked_unordered_map<std::uint64_t, std::reference_wrapper<Context>>;

protected:
  /// Unpacks and emits a vector's `metrics` data.
  /// Note that this relies on identifiers being the same as on the writing end.
  // MT: Externally Synchronized
  iter_t unpackMetrics(iter_t, const ctx_map_t&) noexcept;

  /// Unpacks and emits a vector's `timepoints` data.
  // MT: Externally Synchronized
  iter_t unpackTimepoints(iter_t) noexcept;

public:
  /// Helper Finalizer to fill the identifier to Context table.
  class ContextTracker : public ProfileFinalizer {
  public:
    ContextTracker(ctx_map_t& t) : target(t) {};
    ~ContextTracker() = default;

    ExtensionClass provides() const noexcept override {
      return ExtensionClass::identifier;
    }
    ExtensionClass requires() const noexcept override {
      return ExtensionClass::identifier;
    }

    void context(const Context&, unsigned int&) noexcept override;

  private:
    ctx_map_t& target;
  };

private:
  std::vector<std::reference_wrapper<Metric>> metrics;
  std::vector<std::reference_wrapper<Module>> modules;
};

}

#endif  // HPCTOOLKIT_PROFILE_SOURCES_PACKED_H
