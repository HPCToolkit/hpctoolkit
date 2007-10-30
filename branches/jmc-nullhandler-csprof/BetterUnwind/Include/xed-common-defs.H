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
/// @file xed-common-defs.H 
/// @author Mark Charney   <mark.charney@intel.com> 
/// @brief some pervasive defines



#ifndef _XED_COMMON_DEFS_H_
# define _XED_COMMON_DEFS_H_

////////////////////////////////////////////////////////////////////////////
namespace XED { 
//  using namespace std;

#if defined(XED_DYNAMIC_CASTING)
# define XED_CASTER dynamic_cast
#else
# define XED_CASTER reinterpret_cast
#endif

#define XED_MAX_OPERANDS 11

#define XED_MAX_OPCODE_BYTES  3

#define XED_MAX_DISPLACEMENT_BYTES  8 // for most things it is 4, but one 64b mov allows 8
#define XED_MAX_IMMEDIATE_BYTES  8 // for most things it is max 4, but one 64b mov allows 8.

#define XED_MAX_INSTRUCTION_BYTES  15

#define XED_BYTE_MASK(x) static_cast<UINT8>((x) & 0xFF)

// some table sizes used by the parser to make sure we don't over-run any limits.
#define XED_MAX_TWO_BYTE_MODRM_OPCODES 100  //FIXME: somewhat arbitrary
#define XED_MAX_THREE_BYTE_OPCODES 50  //FIXME: somewhat arbitrary
#define XED_MAX_DFA_OPCODES 100  //FIXME: somewhat arbitrary
#define XED_MAX_X87_DENSE_TABLE 850  //FIXME: somewhat arbitrary

#define XED_MAX_REQUIRED_RFLAGS_ENTRIES 220
#define XED_MAX_REQUIRED_COMPLEX_RFLAGS_ENTRIES 20
#define XED_MAX_TMP_REGS 16
////////////////////////////////////////////////////////////////////////////
// used for defining bit-field widths
// Microsoft's compiler treats enumerations as signed and if you pack
// the bit-field with values, when you assign it to a full-width enumeration,
// you get junk-- a big negative number. This compensates for cases that I've
// encountered
#if defined(__GNUC__)
#  define XED_BIT_FIELD_PSEUDO_WIDTH4 4
#  define XED_BIT_FIELD_PSEUDO_WIDTH8 8
#else
#  define XED_BIT_FIELD_PSEUDO_WIDTH4 8
#  define XED_BIT_FIELD_PSEUDO_WIDTH8 16
#endif

////////////////////////////////////////////////////////////////////////////
} // namespace
#endif









