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
/// @file xed-types.H
/// @author Mark Charney   <mark.charney@intel.com> 


#ifndef _XED_TYPES_H_
# define _XED_TYPES_H_

////////////////////////////////////////////////////////////////////////////

#include "xed-common-hdrs.H"

#if defined(XED_SCALARS)
# if defined(__GNUC__) && !defined(_WIN32)
// do not include stdint.h in the namespace or it'll mess up everything that wants to have unqualified base types.
#  include <stdint.h>
# endif
#endif

namespace XED {

#if defined(XED_SCALARS)
# if defined(_WIN32)
#  define UINT8 unsigned __int8
#  define UINT16 unsigned __int16
#  define UINT32 unsigned __int32
#  define UINT64 unsigned __int64
#  define INT8 __int8
#  define INT16 __int16
#  define INT32 __int32
#  define INT64 __int64
#  define UINT unsigned int
# else
// use stuff from stdint.h
typedef uint8_t  UINT8;   //LINUX HOSTS
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

typedef int8_t  INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;

//typedef uint32_t UINT;
typedef unsigned int  UINT;
# endif
#else
// PIN PROVIDES ITS OWN SCALARS
# if defined(LEVEL_BASE_H) 
   // for Charm/Pin  
   using namespace LEVEL_BASE;
# endif 
#endif

} // namespace
////////////////////////////////////////////////////////////////////////////
#endif
