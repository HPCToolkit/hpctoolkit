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

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_DENSEIDS_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_DENSEIDS_H

#include "../finalizer.hpp"

namespace hpctoolkit::finalizers {

// Simple dense-id generating Finalizer.
class DenseIds final : public ProfileFinalizer {
public:
  DenseIds();
  ~DenseIds() = default;

  ExtensionClass provides() const noexcept override {
    return ExtensionClass::identifier + ExtensionClass::mscopeIdentifiers;
  }
  ExtensionClass requires() const noexcept override { return {}; }
  void module(const Module&, unsigned int& id) noexcept override;
  void file(const File&, unsigned int& id) noexcept override;
  void metric(const Metric&, unsigned int& ids) noexcept override;
  void metric(const Metric&, Metric::ScopedIdentifiers& ids) noexcept override;
  void context(const Context&, unsigned int& id) noexcept override;
  void thread(const Thread&, unsigned int& id) noexcept override;

private:
  std::atomic<unsigned int> mod_id;
  std::atomic<unsigned int> file_id;
  std::atomic<unsigned int> met_id;
  std::atomic<unsigned int> smet_id;
  std::atomic<unsigned int> stat_id;
  std::atomic<unsigned int> sstat_id;
  std::atomic<unsigned int> ctx_id;
  std::atomic<unsigned int> t_id;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_DENSEIDS_H
