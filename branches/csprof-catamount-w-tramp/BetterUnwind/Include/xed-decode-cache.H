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
/// @file xed-decode-cache.H 
/// @author Mark Charney   <mark.charney@intel.com>



#ifndef _XED_DECODE_CACHE_H_
# define _XED_DECODE_CACHE_H_

#include "xed-decode.H"

namespace XED { 
  using namespace std;

////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////

/// To allocate the decode cache; Users must use this once before calling
/// xed_decode_cache(). Allocates memory.
/// @param power_of_two_limit The size of the decode cache; must be a power of two.
/// @ingroup INIT
XED_DLL_EXPORT void
xed_decode_cache_init(int power_of_two_limit = 64*1024);

/// To delete the decode cache's allocated memory for whatever reason. Not
/// required for typical XED use.
///  @ingroup INIT
XED_DLL_EXPORT void
xed_decode_cache_delete();

///   This is an alternate interface to the decoder that tries to cache
///   decode results. The arguments are the same as xed_decode(). You must
///   call
///  xed_decode_cache_init()
///   prior to invoking this function.
///  @param xedd the decoded instruction of type #XED::xed_decoded_inst_t . Mode/state sent in via xedd; See the #XED::xed_state_t
///  @param array the pointer to the instruction text.
///  @param len the length of the input array. 1 to 15 bytes, anything more is ignored.
///  @return XED::xed_error_enum_t indiciating success (XED_ERROR_NONE) or failure. Note failure can be due to not
///  enough bytes in the input array.
/// @ingroup DEC
XED_DLL_EXPORT xed_error_enum_t
xed_decode_cache(xed_decoded_inst_t* xedd, 
                 const UINT8* array, 
                 const unsigned int len);

////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////




} // namespace

#endif
////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-decode-cache.cpp"
//End:
