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
/// @file xed-portability.H
/// @author Mark Charney   <mark.charney@intel.com>

#ifndef _XED_PORTABILITY_H_
# define _XED_PORTABILITY_H_
# include "xed-types.H"
namespace XED {
# if defined(_WIN32)
INT64 strtoll(const char* buf, int p, int base);

#  define MY_OSTREAMS_FOR_VC98
#  if defined(MY_OSTREAMS_FOR_VC98)
#   if XED_PRINT == 1
#    if defined(_MSC_VER)
#     if _MSC_VER == 1200 
std::ostream& operator<<(std::ostream& o, UINT64 x) ;
std::ostream& operator<<(std::ostream& o, INT64 x) ;
std::ostream& operator<<(std::ostream& o, INT32 x) ;
std::ostream& operator<<(std::ostream& o, UINT32 x) ;
#     endif //_MSC_VER
#    endif // defined(_MSC_VER)
#   endif // XED_PRINT=1
#  endif // defined(MY_OSTREAMS_FOR_VC98)

# endif // defined(_WIN32)

} //namespace
#endif  // _XED_PORTABILITY_H_

////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-portability.cpp"
//End:
