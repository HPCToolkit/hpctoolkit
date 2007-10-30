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
/// @file xed-decoded-inst.H 
/// @author Mark Charney   <mark.charney@intel.com> 


#ifndef _XED_DECODED_INST_H_
# define _XED_DECODED_INST_H_

#include "xed-raw.H"
#include "xed-gheaders.H"
#include "xed-state.H"
#include "xed-decoded-resource.H"
#include "xed-common-fields.H"
#include "xed-func-util.H"
////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
namespace XED { 
using namespace std;

class xed_decoder_impl_t;

/// This is the class passed to/from the decoder #XED::xed_decode(). It gets filled in
/// with decode info and pointers to tables that have more information
/// about the instruction. Look in the parent
/// class #XED::xed_common_fields_t 
/// for
/// most of the operand information, etc.
///
/// The #XED::xed_decoded_inst_t also contains the raw bytes from decoding
/// in the #XED::xed_raw_inst_t raw member variable. 
///
/// Helpful hint: that base class #XED::xed_common_fields_t is also used by
/// the 
/// class #XED::xed_encoder_request_t 
/// for encoder requests.  This makes it easy to re-encode an instruction that you've decoded.
///@ingroup DEC
class XED_DLL_EXPORT xed_decoded_inst_t :  public xed_common_fields_t
{

    /// The decoder istelf is a friend, allowing me to hide much of the interface.
    friend class xed_decoder_impl_t;

  public:


    /// @name Constructors and other primary initializers
    //@{
    /// Constructor
    xed_decoded_inst_t() 
    {
        zero();
    }

    /// Constructor
    xed_decoded_inst_t(const xed_state_t& arg_dstate) 
    //   : xed_common_fields_t(arg_dstate)
    {
        init(arg_dstate);
        //   zero();
        //set_effective_address_width(get_address_width());
    }
    
    /// Helper function for re-initializing a 
    /// xed_decoded_inst_t .
    void
    init(const xed_state_t& arg_dstate)
    {
        zero();
        set_state(arg_dstate); // calls set_effective_address_width()
    }
    //@}

    /// Clear the class. Note: does not clear out the base class.
    void 
    zero();
    /////////////////////////////////////////////

    /// @name Accessing the raw instruction fields
    //@{
    // Access the raw info about the instruction bytes.
    inline const xed_raw_inst_t& 
    get_raw() const {
        return raw;
    }

    /// Get the raw instruction length, after decoding
    inline unsigned int
    get_length() const { 
        return get_raw().get_length();
    }

    inline UINT8 get_modrm_mod() const {
        return static_cast<UINT8>(raw.get_modrm().get_mod());
    }
    inline UINT8 get_modrm_reg() const {
        return static_cast<UINT8>(raw.get_modrm().get_reg());
    }
    inline UINT8 get_modrm_rm() const {
        return static_cast<UINT8>(raw.get_modrm().get_rm());
    }

    inline UINT8 get_sib_scale() const {
        return static_cast<UINT8>(raw.get_sib().get_scale());
    }
    inline UINT8 get_sib_base() const {
        return static_cast<UINT8>(raw.get_sib().get_base());
    }
    inline UINT8 get_sib_index() const {
        return static_cast<UINT8>(raw.get_sib().get_index());
    }
    inline bool
    get_rex() const {
        return raw.rex();
    }

    inline xed_rexx_t 
    get_rexx() const {
        if (raw.rex()) {
            UINT8 x = raw.get_rex().get_rexx();
            return (x ? XED_REXX_1 : XED_REXX_0);
        }
        return XED_REXX_NONE;
    }

    inline xed_rexb_t 
    get_rexb() const 
    {
        if (raw.rex()) 
        {
            UINT8 x = raw.get_rex().get_rexb();
            return (x ? XED_REXB_1 : XED_REXB_0);
        }
        return XED_REXB_NONE;
    }

    inline xed_rexr_t 
    get_rexr() const {
        if (raw.rex()) {
            UINT8 x = raw.get_rex().get_rexr();
            return (x ? XED_REXR_1 : XED_REXR_0);
        }
        return XED_REXR_NONE;
    }

    inline xed_rexw_t 
    get_rexw() const {
        if (raw.rex()) {
            UINT8 x = raw.get_rex().get_rexw();
            return (x ? XED_REXW_1 : XED_REXW_0);
        }
        return XED_REXW_NONE;
    }

    //@}


    /// @name Internal operand template representation
    //@{

    /// These are the internal operand structures from the opcode
    /// template. Ignore unless you want to get the reasons for some
    /// of the operands.
    inline const xed_operand_template_t&
    get_operand_template(unsigned int n) const {
        return find_opcode()->get_operand_template(n);
    }

    /// The split code for the i'th operand gives further information
    /// about the operand template, breaking it down for further
    /// procesing. Most split codes indicate a register type and a
    /// width. But there are many special ones that require special
    /// handling.
    const xed_operand_code_split_t&
    get_real_opnd_split(unsigned int i) const;
    const xed_operand_code_split_t&
    get_effective_opnd_split(unsigned int i) const;

    //@}
    

    /// @name Register info based on the internal instruction templates
    //@{

    /// Returns true if the base register is updated because of
    /// string-instruction processing.
    bool 
    updates_base_register(unsigned int memop_indx) const;

    /// Returns true iff the corresponding operand register
    /// is mutable (changeable) or required as-is for this instruction
    /// encoding.
    bool
    operand_is_mutable(unsigned int operand_indx) const;

    /// Returns true iff the corresponding memory operand register is
    /// mutable (changeable) or required as-is for this instruction encoding.
    bool
    memory_operand_is_mutable(unsigned int memop_indx) const;

  private:
    /// Returns true iff the template operand is mutable. This is a lower
    /// level function that works with template indices and is called by
    /// the other _is_mutable() functions.
    bool
    get_mutable(int template_operand_index) const;

  protected:

    inline void
    set_fpstate(bool v)
    {
        fpstate = v;
    }

  public:

    //@}
  
    /// @name Widths and Modes
    //@{

    addr_width_t
    compute_effective_addr_width() const;

    addr_width_t
    compute_effective_stack_addr_width() const;

  private:
    /// Based on the mode and some opcode width information, compute the
    /// effetive operand width. This is private because the value gets
    /// stashed in the common fields base class.
    opnd_width_t
    compute_operand_width() const;

  public:

    inline bool 
    long64_mode() const {
        return get_machine_mode() == XED_MACHINE_MODE_LONG_64;
    }
    //@}


    /// @name Accessing the derived opcode table classes
    //@{

    /// return the name of the subtable from which the instruction ultimately decoded.
    inline  xed_encoder_subtable_enum_t
    get_opcode_table_name() const {
        return subtable;
    }


    inline const xed_one_byte_opcode_t* get_opcode1() const {
        const xed_one_byte_opcode_t* xp = 
            XED_CASTER<const xed_one_byte_opcode_t*>(opcode_base);
        return xp;
    }
    inline const xed_two_byte_opcode_t* get_opcode2() const {
        const xed_two_byte_opcode_t* xp = 
            XED_CASTER<const xed_two_byte_opcode_t*>(opcode_base);
        return xp;
    }

    inline const xed_two_byte_modrm_opcode_t* get_opcode2m() const {
        const xed_two_byte_modrm_opcode_t* xp = 
            XED_CASTER<const xed_two_byte_modrm_opcode_t*>(opcode_base);
        return xp;

    }

    inline const xed_three_byte_opcode_t* get_opcode3() const {
        const xed_three_byte_opcode_t* xp = 
            XED_CASTER<const xed_three_byte_opcode_t*>(opcode_base);
        return xp;

    }
    inline const x87_opcode_info_t* get_x87_opcode() const {
        const x87_opcode_info_t* xp =
            XED_CASTER<const x87_opcode_info_t*>(opcode_base);
        return xp;
    }
    inline const group_modrm_opcode_t* get_group_opcode() const {
        const group_modrm_opcode_t* xp = 
            XED_CASTER<const group_modrm_opcode_t*>(opcode_base);
        return xp;

    }
  
    /// This is for the "main" 3DNow opcodes that start of "0F0F" and
    /// have their "real" opcode in the immediate field. The other more
    /// normal 3DNow opcodes are not found here. If you really want to
    /// be sure if you have a 3DNow instruction, check the
    /// xed_extension_t in the opcode base pointer.
    inline const xed_one_byte_opcode_t* get_amd_3dnow_opcode() const {
        const xed_one_byte_opcode_t* xp =
            XED_CASTER<const xed_one_byte_opcode_t*>(opcode_base);
        return xp;

    }

    //@}

    /// @name Accessing the base opcode table class
    //@{

    /// This will assert if no valid instruction has decoded.
    inline const xed_opcode_base_t* 
    find_opcode() const 
    {
        const xed_opcode_base_t* p = find_opcode_or_zero();
    
        if (!p) 
        {
            xed_derror("Did not find opcode pointer.");
            assert(0);
            return 0;
        }

        return p;
    }

    /// This will return 0 if no valid instruction has decoded.
    inline const xed_opcode_base_t* 
    find_opcode_or_zero() const {
        return opcode_base;
    }
  
    //@}


    /// @name Simple functions and predicates
    //@{
    /// return the instruction category 
    inline xed_category_t
    get_category() const
    {
        const xed_opcode_base_t* p = find_opcode_or_zero();
        if (p) 
        {
            return p->get_category();
        }
        return XED_CATEGORY_INVALID;
    }

    /// return the class ISA xed_extension_t
    inline xed_extension_t
    get_extension() const
    {
        const xed_opcode_base_t* p = find_opcode_or_zero();
        if (p) 
        {
            return p->get_extension();
        }
        return XED_EXTENSION_INVALID;
    }

    /// Return the iclass (XEDICLASS_ADD, etc.)
    inline xed_iclass_t
    get_decoded_iclass() const
    {
        const xed_opcode_base_t* p = find_opcode();
        return p->get_iclass();
    }

    inline bool
    stringop() const 
    {
        return (get_category() == XED_CATEGORY_STRINGOP);
    }

    /// This reports if the instruction has a rep prefex and it is not
    /// just for opcode extension.
    inline bool
    has_real_rep() const 
    {
        const xed_opcode_base_t* p = find_opcode_or_zero();
        if (p && p->rep_able_opcode()) 
        {
            if (get_raw().some_rep_prefix()) 
            {
                return true;
            }
        }
        return false;
    }

    /// True if the instruction is one of several kinds of nops. This
    /// captures the common case of 0x90 (xchg eax, eax) as well as when
    /// that is prefixed in various ways. It also catches the "true" 2B
    /// nops.
    bool
    is_nop() const;
    

    /// True if the instruction loads or all or most of the X87/MMX/XMM
    /// register state. This is a helper for register allocation.
    inline bool
    accesses_lots_of_fp_state() const
    {
        return fpstate;
    }
    //@}

    /// @name Flags information
    //@{

    /// returns a pointer to the class containing flags info for this
    /// instruction OR zero if the instruction does not read or write
    /// the flags.
    const xed_rflag_t*
    get_rflags_info() const;


    /// returns true if the instruciton uses the rFLAGS.
    inline bool 
    uses_rflags() const {
        const xed_rflag_t* p = get_rflags_info();
        return (p && p->get_nflags() > 0);
    }

    //@}

#if XED_PRINT==1
    /// @name Printing and debugging
    //@{
    void 
    print_operand_templates(ostream& o) const;
    void
    print_flags_info(ostream& o) const;
    
    /// One liner (includes prefixes)
    void
    print_short(ostream& o, UINT64 instruction_address=0) const;

    /// Multi-line printout. Gory detail
    void 
    dump(ostream& o) const;

    //@}
#endif

    /// @name Atomicity / Locking
    //@{
    /// Returns true if the instruction was (a) lockable, (b) had a LOCK
    /// prefix (or was an XCHG that accesses memory), and (c) accessed memory.
    inline bool get_atomic() const { return atomic; }
    //@}

    void
    set_effective_operand(unsigned int arg_operand_index,
                          xed_operand_enum_t arg_opnd)
    {
        overridden_operand = arg_operand_index;
        effective_operand = arg_opnd;
    }

  private:
    inline void set_atomic()  { atomic = true; }

    //////////////////////////////////////////////////////

  private:

    /// The parent opcode pointer.  It contains the common information
    /// about decoded opcodes.
    const xed_opcode_base_t* opcode_base;
    xed_raw_inst_t raw;
    xed_encoder_subtable_enum_t subtable : XED_BIT_FIELD_PSEUDO_WIDTH4 ;
    bool atomic :1 ;
    bool fpstate :1 ; // touches lots of FP registers. Summary bit for regalloc
    unsigned int overridden_operand :8;
    xed_operand_enum_t effective_operand : 16;

////////////////////////////////////////////////////////////////////////////

    /// Needed for the decoder itself to write the raw structure
    inline xed_raw_inst_t*
    get_raw_nonconst()  {
        return &raw;
    }

    inline void
    set_opcode_base(const xed_opcode_base_t*  p,
                    const xed_encoder_subtable_enum_t arg_subtable) {
        opcode_base = p;
        subtable = arg_subtable;
    }

}  ; // __attribute__((packed)); // xed_decode_inst_t

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_decoded_inst_t& x);
#endif


} //namespace

#endif
////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-decoded-inst.cpp"
//End:
