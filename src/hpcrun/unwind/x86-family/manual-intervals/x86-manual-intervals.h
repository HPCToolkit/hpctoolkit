// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __x86_manual_intervals_h_
#define __x86_manual_intervals_h_

//------------------------------------------------------------------------------
// macros
//------------------------------------------------------------------------------

#define FORALL_X86_INTERVAL_FIXUP_ROUTINES(MACRO) \
        MACRO(x86_adjust_icc_variant_intervals) \
        MACRO(x86_adjust_32bit_main_intervals) \
        MACRO(x86_adjust_gcc_main64_intervals) \
        MACRO(x86_adjust_intel_align32_intervals) \
        MACRO(x86_adjust_intel_align64_intervals) \
        MACRO(x86_adjust_intelmic_intervals) \
        MACRO(x86_adjust_intel11_f90main_intervals) \
        MACRO(x86_adjust_dl_runtime_resolve_unwind_intervals) \
        MACRO(x86_adjust_pgi_mp_pexit_intervals) \
        MACRO(x86_adjust_gcc_stack_intervals) \
        MACRO(x86_fail_intervals)


#endif
