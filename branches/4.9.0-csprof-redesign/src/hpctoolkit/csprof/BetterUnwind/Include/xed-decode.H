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
/// @file xed-decode.H 
/// @author Mark Charney   <mark.charney@intel.com> 


#ifndef _XED_DECODE_H_
# define _XED_DECODE_H_


#include "xed-decoded-inst.H"
#include "xed-error-enum.H"
////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
namespace XED { 
using namespace std;

////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////

/// Opaque type for per-thread decode variables. Allocate them with #XED::xed_get_new_decoder_impl(). 
/// @ingroup DEC
class xed_decoder_impl_t;


/// For thread stafety, one can pass an opaque xed_decoder_impl_t*
/// (opaque pointer) as the last argument to #XED::xed_decode() . This function is
/// the only function in XED that allocates storage using 'new'. If you
/// allocate one of these structures, you are responsible for deleting it
/// by calling #XED::xed_delete_decoder_impl() . 
/// @ingroup DEC
XED_DLL_EXPORT xed_decoder_impl_t*
xed_get_new_decoder_impl();

/// Since the pointer is opaque, XED provides a mechanism to free pointers
/// obtained via #XED::xed_get_new_decoder_impl() .
/// @ingroup DEC
XED_DLL_EXPORT void
xed_delete_decoder_impl(xed_decoder_impl_t* p);


///   This is the main interface to the decoder.
///
///  @param xedd the decoded instruction of type #XED::xed_decoded_inst_t . Mode/state sent in via xedd; See the #XED::xed_state_t
///
///  @param array the pointer to the instruction text.
///
///  @param len the length of the input array. 1 to 15 bytes, anything more
///  is ignored.
///
///  @param thread_safe_state defaults to 0. This could be a pointer to
///  storage allocated with #XED::xed_get_new_decoder_impl() for thread
///  safety. If zero, then XED uses a single internal object and is not
///  thread safe.
///
///  @return XED::xed_error_enum_t indiciating success (XED_ERROR_NONE) or
///  failure. Note failure can be due to not enough bytes in the input
///  array.  
///
/// @ingroup DEC
XED_DLL_EXPORT xed_error_enum_t
xed_decode(xed_decoded_inst_t* xedd, 
           const UINT8* array, 
           const unsigned int len,
           xed_decoder_impl_t* thread_safe_state=0);


////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////


} //namespace

#endif
////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-decode.cpp"
//End:
