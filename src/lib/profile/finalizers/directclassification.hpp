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

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_DIRECTCLASSIFICATION_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_DIRECTCLASSIFICATION_H

#include "../finalizer.hpp"

namespace hpctoolkit::finalizers {

// For Module Classification, drawing the data mostly directly from the file
// itself. This handles the little details.
class DirectClassification final : public ProfileFinalizer {
public:
  // `dwarfThreshold` is in the units of bytes.
  // If dwarfThreshold == std::numeric_limits<uintmax_t>::max(), no limit.
  DirectClassification(uintmax_t dwarfThreshold);

  ExtensionClass provides() const noexcept override { return ExtensionClass::classification; }
  ExtensionClass requires() const noexcept override { return ExtensionClass::resolvedPath; }
  void module(const Module&, Classification&) noexcept override;

private:
  uintmax_t dwarfThreshold;
  bool fullDwarf(void* dw, const Module&, Classification&);
  bool symtab(void* elf, const Module&, Classification&);
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_DIRECTCLASSIFICATION_H
