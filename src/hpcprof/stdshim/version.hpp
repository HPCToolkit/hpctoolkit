// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_STDSHIM_VERSION_H
#define HPCTOOLKIT_STDSHIM_VERSION_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features into C++11, sometimes by using class inheritance tricks,
// and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This file provides the compile-time checks for the various features.

// If the compiler claims a version of the C++ spec, we'll assume its correct.
// For now. Change later when things break on some compiler in the future.
#if __cplusplus >= 201903L
#define HPCTOOLKIT_STDSHIM_STD_HAS_atomic_wait

#if defined(__has_include)
#if __has_include(<bit>)
#define HPCTOOLKIT_STDSHIM_STD_HAS_bit
#endif
#else  // defined(__has_include)
// If we can't test it directly, just assume the compiler has it
#define HPCTOOLKIT_STDSHIM_STD_HAS_bit
#endif

#endif  // __cplusplus >= 201903L
#if __cplusplus >= 201703L

#if defined(__has_include)
#if __has_include(<filesystem>)
#define HPCTOOLKIT_STDSHIM_STD_HAS_filesystem
#else
// We assume experimental/filesystem is available, its close enough to work with
#define HPCTOOLKIT_STDSHIM_STD_HAS_experimental_filesystem
#endif
#else  // defined(__has_include)
// If we can't test it directly, just assume the compiler has it
#define HPCTOOLKIT_STDSHIM_STD_HAS_filesystem
#endif

#ifndef HPCTOOLKIT_SLOW_LIBC
#define HPCTOOLKIT_STDSHIM_STD_HAS_shared_mutex
#endif
#endif  // __cplusplus >= 201703L

#endif  // HPCTOOLKIT_STDSHIM_VERSION_H
