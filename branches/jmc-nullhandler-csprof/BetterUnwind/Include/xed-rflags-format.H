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
/// @file xed-rflags-format.H
/// @author  Mark Charney   <mark.charney@intel.com>



#ifndef _XED_RFLAGS_FORMAT_H_
# define _XED_RFLAGS_FORMAT_H_

////////////////////////////////////////////////////////////////////////////
namespace XED {
  using namespace std;


/// Raw bit vector of flags.
/// Note the iopl field is stored JUST ONE BIT!
/// This is used to indicate the presence/absence of a bit in 
///  a particular set, whether it is read, written, etc.
union rflags_rw_t {
  struct {
    UINT32 id : 1;
    UINT32 vip : 1;
    UINT32 vif : 1;
    UINT32 ac : 1;
    UINT32 vm : 1;
    UINT32 rf : 1;
    UINT32 nt : 1;
    UINT32 iopl : 1;  // just one bit
    UINT32 of : 1;
    UINT32 df : 1;
    UINT32 _if : 1; // note keyword clash
    UINT32 tf : 1;
    UINT32 sf : 1;
    UINT32 zf : 1;
    UINT32 af : 1;
    UINT32 pf : 1;
    UINT32 cf : 1;
  } s;
  UINT32 f;
};

////////////////////////////////////////////////////////////////////////////
} //namespace

#endif

