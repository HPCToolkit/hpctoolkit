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

#include "dataclass.hpp"

using namespace hpctoolkit;

const DataClass::attributes_c DataClass::attributes;
const DataClass::references_c DataClass::references;
const DataClass::metrics_c    DataClass::metrics;
const DataClass::contexts_c   DataClass::contexts;
const DataClass::trace_c      DataClass::trace;
const ExtensionClass::classification_c ExtensionClass::classification;
const ExtensionClass::identifier_c     ExtensionClass::identifier;

DataClass literals::operator""_dat(const char* s, std::size_t sz) {
  DataClass res;
  for(std::size_t i = 0; i < sz; i++) {
    switch(s[i]) {
    case 'a': res |= DataClass::attributes; break;
    case 'r': res |= DataClass::references; break;
    case 'm': res |= DataClass::metrics; break;
    case 'c': res |= DataClass::contexts; break;
    case 't': res |= DataClass::trace; break;
    default: break;
    }
  }
  return res;
}

ExtensionClass literals::operator""_ext(const char* s, std::size_t sz) {
  ExtensionClass res;
  for(std::size_t i = 0; i < sz; i++) {
    switch(s[i]) {
    case 'c': res |= ExtensionClass::classification; break;
    case 'i': res |= ExtensionClass::identifier; break;
    default: break;
    }
  }
  return res;
}
