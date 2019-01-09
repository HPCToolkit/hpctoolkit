// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2018, Rice University
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


#ifndef SRC_TOOL_HPCRUN_UTILITIES_CPUID_H_
#define SRC_TOOL_HPCRUN_UTILITIES_CPUID_H_

struct cpuid_type_s {
  char vendor[13];
  int  family;
  int  model;
  int  step;
};

typedef enum {
  CPU_UNSUP = 0,

  INTEL_WSM_EX,
  INTEL_SNB,
  INTEL_SNB_EP,
  INTEL_NHM_EX,
  INTEL_NHM_EP,
  INTEL_WSM_EP,
  INTEL_IVB_EX,
  INTEL_HSX,
  INTEL_BDX,
  INTEL_SKX,

  AMD_MGN_CRS
} cpu_type_t;


enum cpu_type_t cpu_type_get(void);


#endif /* SRC_TOOL_HPCRUN_UTILITIES_CPUID_H_ */
