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

#ifndef HPCTOOLKIT_STDSHIM_VERSION_H
#define HPCTOOLKIT_STDSHIM_VERSION_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features into C++11, sometimes by using class inheritence tricks,
// and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This file provides the compile-time checks for the various features.

// If the compiler claims a version of the C++ spec, we'll assume its correct.
// For now. Change later when things break on some compiler in the future.
#if __cplusplus >= 201903L
#define _HPCTOOLKIT_STDSHIM_HAS_atomic_wait
#endif  // __cplusplus >= 201903L
#if __cplusplus >= 201703L

#if defined(__has_include)
#if __has_include(<filesystem>)
#define _HPCTOOLKIT_STDSHIM_HAS_filesystem
#else
// We assume experimental/filesystem is available, its close enough to work with
#define _HPCTOOLKIT_STDSHIM_HAS_experimental_filesystem
#endif
#else  // defined(__has_include)
// If we can't test it directly, just assume the compiler has it
#define _HPCTOOLKIT_STDSHIM_HAS_filesystem
#endif

#define _HPCTOOLKIT_STDSHIM_HAS_optional
#ifndef HPCTOOLKIT_SLOW_LIBC
#define _HPCTOOLKIT_STDSHIM_HAS_shared_mutex
#endif
#endif  // __cplusplus >= 201703L

#define _HPCTOOLKIT_STDSHIM_STD_HAS(X) defined(_HPCTOOLKIT_STDSHIM_HAS_ ## X)

#endif  // HPCTOOLKIT_STDSHIM_VERSION_H

// Easier macro for #if clauses, should be undef'd at the end of files.
#define STD_HAS(X) _HPCTOOLKIT_STDSHIM_STD_HAS(X)
