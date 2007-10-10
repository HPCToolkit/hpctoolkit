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
/// @file xed-decoded-resource.H 
/// @author  Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_DECODED_RESOURCE_H_
# define _XED_DECODED_RESOURCE_H_

#include "xed-gheaders.H"
#include "xed-reg-class-intfc.H"

namespace XED {
using namespace std;

#define XED_INVALID_TEMPLATE_INDEX 15
//class xed_encoder_root_table_t;


/// xed_decoded_resource_t is the main operand array element for
/// decoded operands. 
///
/// For decode, this is what is traversed to obtain
/// information about the operands by most users of XED. First check the
/// boolean predicate functions to figure out what kind of resource
/// you have. Then the call the appropriate get_* function to access
/// the specific details of that resource.
///
/// Also, two proprties, the
/// visibility  (
/// #xed_opvis_enum_t 
/// ) of the resource and whether it is read/written (
/// #xed_opnd_action_t
/// ) are
/// associated with this class. The two properties affect how XED
/// treats the operand when encoding instructions. 
////
/// For encode, set the values that
/// you want the encoder to encode.
class XED_DLL_EXPORT xed_decoded_resource_t
{
    //friend class xed_encoder_root_table_t;

    xedregs_t areg :XED_BIT_FIELD_PSEUDO_WIDTH8;
    xed_pseudo_resource_t pres :8 ;//GETS SUBVERTED TO HOLD AN UINT8 for IMM8 resources.
    xed_resource_t res :XED_BIT_FIELD_PSEUDO_WIDTH4;
    xed_opnd_action_t rw :4 ;
    xed_opvis_enum_t opvis :4 ;
    unsigned int template_index :4 ;
  public:

    xed_decoded_resource_t() // CONS
        : 
#if defined(THOROUGH_INIT_FOR_DECODED_RESOURCE)
          areg(XEDREG_INVALID),
          pres(XED_PSEUDO_RES_INVALID),
          res(XED_RESOURCE_INVALID),
          rw(XED_OPND_ACTION_INVALID),
          opvis(XED_OPVIS_INVALID),
          template_index(XED_INVALID_TEMPLATE_INDEX)
#else
          res(XED_RESOURCE_INVALID)
#endif
    {

    }
  
    /// Just zero's the resource type, so that invalid() will return true.
    inline void
    invalidate()
    {
        res = XED_RESOURCE_INVALID;
    }

    inline void
    zero() 
    {
        res = XED_RESOURCE_INVALID;
#if defined(THOROUGH_INIT_FOR_DECODED_RESOURCE)
        areg = XEDREG_INVALID;
        rw = XED_OPND_ACTION_INVALID;
        pres = XED_PSEUDO_RES_INVALID;
        opvis = XED_OPVIS_INVALID;
        template_index = XED_INVALID_TEMPLATE_INDEX;
#endif
    }

    /// @name Setting up values
    //@{

    /// To update the register for a pre-existing register opreand. Must
    /// already be a register resource.
    inline void
    update_reg(xedregs_t new_reg) 
    {
        assert(res == XED_RESOURCE_REG);
        areg = new_reg;
    }

    /// set a register, overloaded based on 2nd arg
    void set(xed_resource_t arg_res, 
             xedregs_t arg_reg,
             xed_opvis_enum_t arg_opvis,
             xed_opnd_action_t arg_rw,
             unsigned int arg_template_index = XED_INVALID_TEMPLATE_INDEX) 
    {
        res = arg_res;
        areg = arg_reg;
        pres = XED_PSEUDO_RES_INVALID;
        opvis = arg_opvis;
        rw = arg_rw;
        template_index = arg_template_index;
    }

    /// set a pseudo-resource, overloaded based on 2nd arg
    void set(xed_resource_t arg_res, 
             xed_pseudo_resource_t arg_pres,
             xed_opvis_enum_t arg_opvis,
             xed_opnd_action_t arg_rw,
             unsigned int arg_template_index = XED_INVALID_TEMPLATE_INDEX) 
    {
        res = arg_res;
        pres = arg_pres;
        areg = XEDREG_INVALID;
        opvis = arg_opvis;
        rw = arg_rw;
        template_index = arg_template_index;
    }

    /// set a immediate or displacement resource
    void set(xed_resource_t arg_res, 
             xed_opvis_enum_t arg_opvis,
             xed_opnd_action_t arg_rw,
             unsigned int arg_template_index = XED_INVALID_TEMPLATE_INDEX) 
    {
        res = arg_res;
        pres = XED_PSEUDO_RES_INVALID;
        areg = XEDREG_INVALID;
        opvis = arg_opvis;
        rw = arg_rw;
        template_index = arg_template_index;
    }

    /// set a second 8b immediate. Only used for the 2nd immediate of the ENTER instruction.
    void set(UINT8 value,
             xed_opvis_enum_t arg_opvis,
             xed_opnd_action_t arg_rw,
             unsigned int arg_template_index = XED_INVALID_TEMPLATE_INDEX) 
    {
        res = XED_RESOURCE_IMM8;
        set_imm8(value);
        areg = XEDREG_INVALID;
        opvis = arg_opvis;
        rw = arg_rw;
        template_index = arg_template_index;
    }

    /// Only used for the 2nd immediate of the ENTER instruction.
    void
    set_imm8(UINT8 value)
    {
        assert(res == XED_RESOURCE_IMM8);
        pres = (xed_pseudo_resource_t)value; // SUBVERTING THE TYPE SYSTEM
    }

    //@}

    /// @name simple predicates
    //@{
    inline bool 
    invalid() const 
    { 
        return (res == XED_RESOURCE_INVALID);
    }

    /// True if there is address-generation information for an LEA
    /// instruction stored in the
    /// xed_common_fields_t.
    inline bool 
    agen() const { 
        return (res == XED_RESOURCE_AGEN);
    }
    /// True if there is address-generation information for a memory accessing
    /// instruction stored in the
    /// xed_common_fields_t.
    inline bool 
    mem() const { 
        return (res == XED_RESOURCE_MEM0) ||
               (res == XED_RESOURCE_MEM1);
    }
    

    /// Maps the memory operation name (AGEN,MEM0,MEM1) to the index in the 
    /// xed_common_fields_t
    /// memory information.
    unsigned int
    memop_index() const;

  
    inline xed_resource_t
    get_res() const
    {
        return res;
    }

    inline bool
    mem_or_agen() const {
        return mem() || agen();
    }

    /// True if there is a register resource. Call get_reg() to obtain the value.
    inline bool 
    reg() const { 
        return (res == XED_RESOURCE_REG);
    }

    /// True if there is an immediate stored in the 
    /// xed_common_fields_t.
    inline bool 
    imm() const { 
        return (res == XED_RESOURCE_IMM);
    }

    /// True if there is a displacement stored in the 
    /// xed_common_fields_t.
    inline bool 
    disp() const { 
        return (res == XED_RESOURCE_DISP);
    }
    
    /// True if there is a pseudo resource. Call get_pseudo_res() to obtain the value.
    inline bool 
    pseudo() const { 
        return (res == XED_RESOURCE_PSEUDO);
    }

    /// This is only used for the 2nd immediate on the ENTER
    /// instruction.
    /// Call get_imm8() to obtain the value.
    inline bool 
    imm8() const { 
        return (res == XED_RESOURCE_IMM8);
    }
    //@}

    /// @name Read/written properties
    //@{

    inline xed_opnd_action_t
    get_opnd_action() const
    {
        return rw;
    }

    /// must read || may read
    inline bool
    read() const 
    {
        return must_read() || may_read();
    }

    /// Things that are conditionally read
    inline bool
    may_read() const
    {
        return rw == XED_OPND_ACTION_CRW;
    }
    /// Things that are always read
    inline bool
    must_read() const
    {
        return rw == XED_OPND_ACTION_R ||
            rw == XED_OPND_ACTION_RW ||
            rw == XED_OPND_ACTION_RCW;
    }
    /// Just read
    inline bool
    read_only() const 
    {
        return rw == XED_OPND_ACTION_R;
    }
    /// Read-and-written (read-and-may-write, read-and-must-write)
    inline bool
    read_and_written() const 
    {
        return read() && written();
    }
    /// Written (may-write, must-write,  read-and-must-written,  read-and-may-write)
    inline bool
    written() const
    {
        return must_write() || may_write();
    }

    /// Just written (may or must write)
    inline bool
    written_only() const 
    {
        return rw == XED_OPND_ACTION_W || rw == XED_OPND_ACTION_CW;
    }

    /// May write (may-write,  read-and-may-write)
    inline bool
    may_write() const
    {
        return rw == XED_OPND_ACTION_CW || rw == XED_OPND_ACTION_RCW;
    }

    /// Must write (must-write, read-and-must-write)
    inline bool
    must_write() const
    {
        return rw == XED_OPND_ACTION_W || rw == XED_OPND_ACTION_RW || rw == XED_OPND_ACTION_CRW;
    }

    //@}

    /// @name Getting at the values
    //@{
    inline xedregs_t
    get_reg() const {
        assert(res == XED_RESOURCE_REG);
        return areg;
    }

    inline xed_pseudo_resource_t
    get_pseudo_res() const {
        assert(res == XED_RESOURCE_PSEUDO);
        return pres;
    }

    /// Only used for the 2nd immediate for the ENTER instruction.
    inline UINT8
    get_imm8() const {
        assert(res == XED_RESOURCE_IMM8);
        return (UINT8)pres; //SUBVERTING THE TYPE SYSTEM
    }

    //@}

    /// @name mappping to template indices for decoded instructions
    //@{
    /// returns the index of the template operand for decoded instructions
    /// only.
    inline  int
    get_template_index() const
    {
        if (template_index != XED_INVALID_TEMPLATE_INDEX)
        {
            return template_index;
        }
        return -1;
    }
#if 0
  protected:
    /// used by the encoder to update the template indices.
    inline  void
    update_template_index(unsigned int arg_template_index) 
    {
        template_index = arg_template_index;
    }
  public:
#endif

    //@}

#if XED_PRINT==1
    /// @name Printing
    //@{
    void 
    print(ostream& o) const {
        o << get_res() << "/";
        if (reg()) {
            o << get_reg();
            o << "(" << xed_reg_class(get_reg()) << ")";
            o << "/";
        }
        if (pseudo()) {
            o << get_pseudo_res() << "/";
        }
        if (imm8()) {
            o << hex 
              << std::setfill('0')
              << (unsigned int)get_imm8() 
              << std::setfill(' ')
              << dec
              << "/";
        }
        o << get_opvis() << "/" << get_opnd_action();
        if (template_index != XED_INVALID_TEMPLATE_INDEX)
        {
            o << "/TI:" << (unsigned int) template_index;
        }
    }
    //@}
#endif

    /// @name Operand Visibility -- implicit/explicit/suppressed
    //@{

    xed_opvis_enum_t 
    get_opvis() const
    {
        return opvis;
    }

    inline void
    set_opvis( xed_opvis_enum_t arg_opvis)
    {
        opvis = arg_opvis;
    }

    /// Implicit-expressed -- helps chose the opcode in encoding
    bool is_implicit() const { // implicit-expressed
        return opvis == XED_OPVIS_IMPLICIT;
    }

    /// Implicit-supressed -- does not help chose the opcode in encoding
    bool is_suppressed() const {
        return opvis == XED_OPVIS_SUPPRESSED;
    }

    /// Explicit (most stuff)
    bool is_explicit() const { 
        return opvis == XED_OPVIS_EXPLICIT;
    }

    /// expressed  -- explicit ops or implicit-expressed ops
    bool expressed() const { 
        return is_explicit() || is_implicit();

    }

    //@}

};

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_decoded_resource_t& x);
#endif
} //namespace
#endif

//Local Variables:
//pref: "../../xed-decoded-resource.cpp"
//End:
