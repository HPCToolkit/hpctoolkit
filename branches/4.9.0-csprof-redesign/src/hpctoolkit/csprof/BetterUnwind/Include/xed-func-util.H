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
/// @file xed-func-util.H 
/// @author Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_FUNC_UTIL_H_
# define _XED_FUNC_UTIL_H_

#include "xed-common-hdrs.H"
#include "xed-types.H"

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
#if XED_PRINT==1
/// Set the ostream for XED's log msgs
XED_DLL_EXPORT void xed_set_log_ostream(ostream* o);
#endif

/// Set the verbosity level for XED
XED_DLL_EXPORT void xed_set_verbosity(int v);


XED_DLL_EXPORT void print_hex_xed(const UINT8* array, const int length); // for single line things. // w/ endl
void print_hex(const UINT8* array, const int length); // breaks lines at  16 bytes.
#if XED_PRINT==1
XED_DLL_EXPORT void print_hex_line(ostream& o, const UINT8* array, const int length);  // no endl
#endif
void convert_ascii_hex(char* p, UINT8* array, int maxlen,  int* len);


bool
convert_binary_with_wildcards_to_mask_and_mark(const char* input,
                                               UINT8& mask,
                                               UINT8& mark);

XED_DLL_EXPORT std::string upcase(const std::string& s);

/// read the system timer 
XED_DLL_EXPORT UINT64 get_time();
  
/// Accepts K / M / G (or B) qualifiers ot multiply
XED_DLL_EXPORT INT64 atoi_general(char* buf, int mul);


#if XED_PRINT==1
XED_DLL_EXPORT void 
xed_derror(const char* s);
XED_DLL_EXPORT void 
xed_derror(const string& o);
XED_DLL_EXPORT void 
xed_derror(ostringstream& o);

XED_DLL_EXPORT void 
xed_dwarn(const char* s);
XED_DLL_EXPORT void 
xed_dwarn(const string& o);
XED_DLL_EXPORT void 
xed_dwarn(ostringstream& o);
#endif

////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////
#endif
//Local Variables:
//pref: "../../xed-func-util.cpp"
//End:
