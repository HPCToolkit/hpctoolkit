/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2005 Intel Corporation 
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
/*
  @ORIGINAL_AUTHOR: Robert Muth
  This header file ensures that pin and the pintool will be compiled with compatible
  compilers.
  It relies on the generated file cc_used_XXX.H
*/

/*
 * Check CPU target of compiler
 */
#if defined(TARGET_IA32) && defined(__GNUG__) && !defined(__i386__)
#  error "Pin kit is for IA-32, but compiler targets a different ISA"
#endif
#if defined(TARGET_IA32E) && defined(__GNUG__) && !defined(__x86_64__)
#  error "Pin kit is for Intel64, but compiler targets a different ISA"
#endif
#if defined(TARGET_IPF) && defined(__GNUG__) && !defined(__ia64__)
#  error "Pin kit is for Itanium, but compiler targets a different ISA"
#endif

#if defined(TARGET_IA32)

# if defined(TARGET_WINDOWS)
#  if defined(TARGET_IA32)
#   include "cc_used_ia32_w.CVH"
#  else
#   error "Need IA32 or EM64T on windows"
#  endif
# elif defined(TARGET_LINUX)
#  include "cc_used_ia32_l.CVH"
# elif defined(TARGET_MAC)
#  include "cc_used_ia32_m.CVH"
# else
#  error "bad target os for IA32"
# endif

#elif defined(TARGET_IA32E)

# if defined(TARGET_WINDOWS)
#   include "cc_used_ia32e_w.CVH"
# elif defined(TARGET_LINUX)
#  include "cc_used_ia32e_l.CVH"
# else
#  error "bad target os for EM64T"
#endif

#elif defined(TARGET_IPF)
#include "cc_used_ipf_l.CVH"
#else
#error "bad target"
#endif

#if defined(_MSC_VER)
#elif defined(__INTEL_COMPILER)
#elif defined(__GNUC__)
#else
#error "pin must be compiled with Microsoft CL, Intel ICC/ICL  or with gcc/g++"
#endif

#if defined(_MSC_VER)
/* FIXME skipping compiler checks for now */
#else

#if !defined(CC_USED__) || !defined(CC_USED_MINOR__) || !defined(CC_USED_PATCHLEVEL__)
#error "internal error pin compiler configuration not available "
#endif

#if CC_USED__ != __GNUC__
#error "pin compiler and pin tool compiler differ in major version number"
#endif

// 3.3 linker is ok for 3.2 library
#if CC_USED__ == 3 && CC_USED_MINOR__ == 2 && __GNUC__ == 3 && __GNUC_MINOR__ == 3
// 4.1 linker is ok for 4.0 library
#elif CC_USED__ == 4 && CC_USED_MINOR__ == 0 && __GNUC__ == 4 && __GNUC_MINOR__ == 1
#else

#if CC_USED_MINOR__ != __GNUC_MINOR__
#error "pin compiler and pin tool compiler differ in minor version number. See cc_used_*.CVH for kit compiler version."
#endif

#if __GNUC_PATCHLEVEL__ < CC_USED_PATCHLEVEL__
#error "pin compiler has higher patch level than pin tool compiler"
#endif
#endif
#endif

