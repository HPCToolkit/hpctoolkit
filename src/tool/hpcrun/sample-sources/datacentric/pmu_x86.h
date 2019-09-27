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


#ifndef __PMU_X86_H__
#define __PMU_X86_H__

#include <utilities/arch/cpuid.h>

#include <sample-sources/perf/pmu.h>

#define EVNAME_LATENCY    "LATENCY"
#define EVNAME_STORE      "PRECISE-STORE"

/**
 * attention: the order of the array can be important.
 * A source for pebs taken from:
 * https://github.com/numap-library/numap/blob/master/src/numap.c
 *
 * and the list of hardware counter at:
 * https://oprofile.sourceforge.io/docs/
 * https://download.01.org/perfmon/index/
 */
struct pmu_config_s pmu_events[] = {
    {INTEL_SNB,    EVNAME_LATENCY, "MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD"},
    {INTEL_SNB,    EVNAME_STORE,   "MEM_TRANS_RETIRED:PRECISE_STORE"},

    {INTEL_SNB_EP, EVNAME_LATENCY, "MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD"},
    {INTEL_SNB_EP, EVNAME_STORE,   "MEM_TRANS_RETIRED:PRECISE_STORE"},

    {INTEL_IVB_EX, EVNAME_LATENCY, "MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD"},
    {INTEL_IVB_EX, EVNAME_STORE,   "MEM_TRANS_RETIRED:PRECISE_STORE"},

    {INTEL_ICL,    EVNAME_LATENCY, "MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3"},
    {INTEL_ICL,    EVNAME_STORE,   "MEM_INST_RETIRED:ALL_STORES"},

    {INTEL_SKX,    EVNAME_LATENCY, "MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3"},
    {INTEL_SKX,    EVNAME_STORE,   "MEM_UOPS_RETIRED:ALL_STORES"},

    {INTEL_BDX,    EVNAME_LATENCY, "MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3"},
    {INTEL_BDX,    EVNAME_STORE,   "MEM_UOPS_RETIRED:ALL_STORES"},

    {INTEL_HSX,    EVNAME_LATENCY, "MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3"},
    {INTEL_HSX,    EVNAME_STORE,   "MEM_UOPS_RETIRED:ALL_STORES"},

    {INTEL_NHM_EX, EVNAME_LATENCY, "MEM_INST_RETIRED:LATENCY_ABOVE_THRESHOLD:ldlat=3"},

    {INTEL_WSM_EX, EVNAME_LATENCY, "MEM_INST_RETIRED:LATENCY_ABOVE_THRESHOLD:ldlat=3"},

    {INTEL_KNL,    "L2_MISS_LOADS", "MEM_UOPS_RETIRED:L2_MISS_LOADS"},
    {INTEL_KNL,    EVNAME_STORE,    "MEM_UOPS_RETIRED:ALL_STORES"} 
};

#endif // __PMU_X86_H__
