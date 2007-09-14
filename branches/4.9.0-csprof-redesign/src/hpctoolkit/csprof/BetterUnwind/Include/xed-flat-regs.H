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
/// @file xed-flat-regs.H
/// @author Mark Charney   <mark.charney@intel.com>



#ifndef _XED_FLAT_REGS_H_
# define _XED_FLAT_REGS_H_

#include "xed-gheaders.H"
#include "xed-decoded-resource.H"

#include "xed-reg-role.H"
#if XED_PRINT==1
# include <iostream>
#endif

namespace XED { 
using namespace std;



/// The element of the xed_reg_array_t. It has a register name, the index
/// in to the xed_decoded_inst_t* operand array and the role of the
/// register (was it a normal register, or one of the memop regs
/// (base/index/seg).
class XED_DLL_EXPORT xed_mappable_reg_t 
{
  public:
    /// The index in to the XED operand array
    UINT8 xed_opn :8;

    /// where it came from in the XED default operand array
    xed_reg_role_t role :8; 

    /// the actual register
    xedregs_t reg :XED_BIT_FIELD_PSEUDO_WIDTH8;

    /// Constructor
    xed_mappable_reg_t()
        : xed_opn(0),
          role(XED_REG_ROLE_INVALID),
          reg(XEDREG_INVALID)
    {
    }

    /// Constructor
    xed_mappable_reg_t(xedregs_t arg_reg,
                       unsigned int arg_xed_opn,
                       xed_reg_role_t arg_role)
        : xed_opn(static_cast<UINT8>(arg_xed_opn)),
          role(arg_role),
          reg(arg_reg)
    {
    }


    void
    zero() 
    {
        reg = XEDREG_INVALID;
        xed_opn = 0;
        role = XED_REG_ROLE_INVALID;
    }
#if XED_PRINT==1
    void
    print(ostream& o) const
    {
        o << reg << "/" << (int)xed_opn << "/" << role;
    }
#endif
};
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_mappable_reg_t& x);
#endif


/// A simple array of xed_mappable_reg_t's.  These are the read array and
/// written array for the xed_flat_regs_t's.
class XED_DLL_EXPORT    xed_reg_array_t
{
    xed_mappable_reg_t mapregs[XED_MAX_OPERANDS];
    unsigned int nregs :8;

  public:
    /// Constructor
    xed_reg_array_t()
        : nregs(0)
    {
        
    }

    void
    zero()
    {
        for(unsigned int i = 0 ;i < nregs; i++ )
        {
            mapregs[i].zero();
        }
        nregs = 0;
    }

    void
    set_nregs(unsigned int n)
    {
        assert(n <= XED_MAX_OPERANDS);
        nregs = n;
    }
    
    unsigned int
    get_nregs() const
    {
        return nregs;
    }

    xed_mappable_reg_t& 
    get_mapreg_nonconst(unsigned int index)
    {
        assert(index < nregs);
        return mapregs[index];
    }

    const xed_mappable_reg_t& 
    get_mapreg(unsigned int index) const
    {
        assert(index < nregs);
        return mapregs[index];
    }

    void
    add_mapreg(xed_mappable_reg_t& r)
    {
        assert(nregs < XED_MAX_OPERANDS);
        mapregs[nregs++] = r;
    }
#if XED_PRINT==1
    void 
    print(ostream& o) const
    {
        for(unsigned int i = 0 ;i < nregs; i++ )
        {
            o << mapregs[i] << " ";
        }
    }
#endif
};
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_reg_array_t& x);
#endif

class XED_DLL_EXPORT xed_decoded_inst_t; // forward decl for pointers

/// This is two arrays of registers (read and written) with mapping and
/// role information to the xed_decoded_inst_t operands and memop info.
/// @ingroup DEC
class XED_DLL_EXPORT xed_flat_regs_t
{
  public:

    /// array of read registers (and other information for mapping to the
    /// xed_decoded_inst_t)
    xed_reg_array_t read;

    /// array of written registers (and other information for mapping to the
    /// xed_decoded_inst_t)
    xed_reg_array_t written;

    /// Constructor
    xed_flat_regs_t()
    {
    }
    
    void
    zero()
    {
        read.zero();
        written.zero();
    }

#if XED_PRINT==1
    void
    print(ostream& o) const
    {
        o << "read: " << read;
        o << "  ";
        o << "written: " << written;
    }
#endif
    /// This makes flattened register arrays from a decoded XED instruction.
    /// This format is useful for compilers. Unpack clears the read/written
    /// arrays before adding anything.
    void
    unpack(xed_decoded_inst_t* xedd);

    /// This takes the flattened register arrays and puts the registers back in
    /// the decoded XED instruction, in the correct places.
    void
    pack(xed_decoded_inst_t* xedd) const;

  private:

    // used by unpack
    void
    unpack_reg(const xedregs_t reg,
               const xed_opnd_action_t rw,
               const unsigned int xed_opn,
               const xed_reg_role_t role);
    
    // used by pack
    void
    pack_array(const xed_reg_array_t& arry,
               xed_decoded_inst_t* xedd) const;
    
};

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_flat_regs_t& x);
#endif



} // namespace

#endif

////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-flat-regs.cpp"
//End:
