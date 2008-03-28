/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/// @file xed-portability.h
/// @author Mark Charney   <mark.charney@intel.com>

#ifndef _XED_PORTABILITY_H_
# define _XED_PORTABILITY_H_
# include "xed-types.h"

#define STATIC_CAST(x,y) ((x) (y))
#define REINTERPRET_CAST(x,y) ((x) (y))

XED_DLL_EXPORT uint_t xed_strlen(const char* s);
XED_DLL_EXPORT void xed_strcat(char* dst, const char* src);
XED_DLL_EXPORT void xed_strcpy(char* dst, const char* src);
/// returns the number of bytes remaining for the next use of #xed_strncpy() or #xed_strncat() .
XED_DLL_EXPORT int xed_strncpy(char* dst, const char* src,  int len);
/// returns the number of bytes remaining for the next use of #xed_strncpy() or #xed_strncat() .
XED_DLL_EXPORT int xed_strncat(char* dst, const char* src,  int len);


/* recognize VC98 */
#if !defined(__INTEL_COMPILER)
# if defined(_WIN32) && defined(_MSC_VER)
#   if _MSC_VER == 1200
#     define XED_MSVC6 1
#   endif
# endif
# if defined(_WIN32) && defined(_MSC_VER)
#   if _MSC_VER == 1310
#     define XED_MSVC7 1
#   endif
# endif
# if defined(_WIN32) && defined(_MSC_VER)
#   if _MSC_VER == 1400
#     define XED_MSVC8 1
#   endif
# endif
#endif

/* I've had compatibilty problems here so I'm using a trivial indirection */
#if defined(__GNUC__)
#  if defined(__CYGWIN__)
      /* cygwin's gcc 3.4.4 on windows  complains */
#    define XED_FMT_X "%lx"
#    define XED_FMT_08X "%08lx"
#    define XED_FMT_D "%ld"
#    define XED_FMT_U "%lu"
#    define XED_FMT_9U "%9lu"
#  else
#    define XED_FMT_X "%x"
#    define XED_FMT_08X "%08x"
#    define XED_FMT_D "%d"
#    define XED_FMT_U "%u"
#    define XED_FMT_9U "%9u"
#  endif
#else
#  define XED_FMT_X "%x"
#  define XED_FMT_08X "%08x"
#  define XED_FMT_D "%d"
#  define XED_FMT_U "%u"
#  define XED_FMT_9U "%9u"
#endif

#if defined(__GNUC__) && defined(__LP64__)
# define XED_FMT_LX "%lx"
# define XED_FMT_LU "%lu"
# define XED_FMT_LD "%ld"
# define XED_FMT_LX16 "%016lx"
#else
# define XED_FMT_LX "%llx"
# define XED_FMT_LU "%llu"
# define XED_FMT_LD "%lld"
# define XED_FMT_LX16 "%016llx"
#endif

#if defined(_M_IA64)
# define XED_IPF
# define XED_FMT_SIZET "%ld"
#else
# define XED_FMT_SIZET "%d"
#endif

#if defined(__GNUC__) 
# define XED_INLINE inline
# define XED_NORETURN __attribute__ ((noreturn))
# if __GNUC__ == 2
#   define XED_NOINLINE 
# else
#   define XED_NOINLINE __attribute__ ((noinline))
# endif
#else
# define XED_INLINE __inline
# if defined(XED_MSVC6)
#   define XED_NOINLINE 
# else
#   define XED_NOINLINE __declspec(noinline)
# endif
# define XED_NORETURN __declspec(noreturn)
#endif

#endif  // _XED_PORTABILITY_H_

////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-portability.c"
//End:
