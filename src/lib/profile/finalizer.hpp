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

#ifndef HPCTOOLKIT_PROFILE_FINALIZER_H
#define HPCTOOLKIT_PROFILE_FINALIZER_H

#include "profile.hpp"
#include "dataclass.hpp"

#include "stdshim/filesystem.hpp"
#include <atomic>

namespace hpctoolkit {

class ProfArgs;

/// Finalizers expand on the data structures after the merge done by the
/// Pipeline. Since they represent a sort of "middling" step of the Pipeline,
/// their actual operations are highly limited.
// For now, they aren't actually limited just to make the refactor go faster.
class Finalizer {
public:
  virtual ~Finalizer() = default;

  /// Bind this Finalizer to a Pipeline to actually get stuff done.
  // MT: Externally Synchronized
  virtual void bindPipeline(ProfilePipeline::SourceEnd&& se) noexcept {
    sink = std::move(se);
  }

  /// Query for the ExtensionClass this Finalizer provides to the Pipeline.
  // MT: Safe (const)
  virtual ExtensionClass provides() const noexcept = 0;

  /// Classify the given Module, filling in the Classification.
  // MT: Internally Synchronized
  virtual void module(const Module&, Classification&) {};

  /// Filter path for module(). If not empty, this Finalizer will only be
  /// used on Modules with the returned path.
  // MT: Safe (const)
  virtual stdshim::filesystem::path filterModule() const noexcept { return ""; }

  /// Assign a (unique, dense) ID to the given Module.
  // MT: Internally Synchronized
  virtual void module(const Module&, unsigned int&) {};

  /// Assign a (unique, dense) ID to the given File.
  // MT: Internally Synchronized
  virtual void file(const File&, unsigned int&) {};

  /// Assign a (unique, dense) ID to the given Function.
  // MT: Internally Synchronized
  virtual void function(const Module&, const Function&, unsigned int&) {};

  /// Assign a (unique, dense) ID to the given Metric.
  // MT: Internally Synchronized
  virtual void metric(const Metric&, std::pair<unsigned int, unsigned int>&) {};

  /// Assign a (unique, dense) ID to the given Context.
  // MT: Internally Synchronized
  virtual void context(const Context&, unsigned int&) {};

  /// Assign a (unique, dense) ID to the given Thread.
  // MT: Internally Synchronized
  virtual void thread(const Thread&, unsigned int&) {};

protected:
  // This is a base class, don't construct it directly.
  Finalizer() = default;

  ProfilePipeline::SourceEnd sink;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZER_H
