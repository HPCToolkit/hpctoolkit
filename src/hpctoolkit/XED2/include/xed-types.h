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
/// @file xed-types.h
/// @author Mark Charney   <mark.charney@intel.com> 


#ifndef _XED_TYPES_H_
# define _XED_TYPES_H_

////////////////////////////////////////////////////////////////////////////

#include "xed-common-hdrs.h"

#if defined(__GNUC__) || defined(__ICC)
#  include <stdint.h>
#endif

/* use these one win32 native compilations */
#if !defined(__GNUC__) &&  !defined(__ICC) && defined(_WIN32)
#  define uint8_t  unsigned __int8
#  define uint16_t unsigned __int16
#  define uint32_t unsigned __int32
#  define uint64_t unsigned __int64
#  define int8_t  __int8
#  define int16_t __int16
#  define int32_t __int32
#  define int64_t __int64
#  define uint_t unsigned int
#else
typedef unsigned int  uint_t;
#endif

typedef uint_t bits_t;

#if !defined(__cplusplus)
# if !defined(bool)
typedef int bool;
# endif
# if !defined(true)
#  define true 1
# endif
# if !defined(false)
#  define false 0
# endif
#endif

////////////////////////////////////////////////////////////////////////////
#endif
