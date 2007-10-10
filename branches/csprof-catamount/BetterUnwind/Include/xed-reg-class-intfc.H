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
/// @file xed-reg-class-intfc.H 
/// @author Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_REG_CLASS_INTFC_H_
# define _XED_REG_CLASS_INTFC_H_
#endif

#include "xed-types.H"
//#include "xed-util.H"
#include "regs.H"
#include "xed-reg-class.H"
#include "xed-gheaders.H"
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


/// return the xed_reg_class_t for the given register. @ingroup REGINTFC
XED_DLL_EXPORT  xed_reg_class_t 
xed_reg_class( const xedregs_t reg);

/// return true if the register if a GPR. @ingroup REGINTFC
XED_DLL_EXPORT  bool 
xed_reg_class_gpr( const xedregs_t reg );

/// Convert a register to a generic register. For example, convert AL,
/// AH, AX, EAX or RAX in to rAX. @ingroup REGINTFC
XED_DLL_EXPORT xed_genreg_t 
xed_reg_to_genreg(xedregs_t r);
    
/// Obtain a structure that describes the nesting relation of the GPRs
/// for a given generic register. @ingroup REGINTFC
XED_DLL_EXPORT const xed_gpr_t& 
xed_gen_reg_nesting_info(const xed_genreg_t reg_index);
    

#if 0
/// return the register that is the appropriate width for the given
/// register. There is some ambiguity about AH vs AL. 
XED_DLL_EXPORT xedregs_t
xed_reg_cast(xedregs_t reg, xed_reg_class_t rclass);
#endif



////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////





} // namespace

////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-reg-class-intfc.cpp"
//End:
