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
/// @file xed-common-fields.H
/// @author Mark Charney   <mark.charney@intel.com>



#ifndef _XED_COMMON_FIELDS_H_
# define _XED_COMMON_FIELDS_H_

#include "xed-raw.H"
#include "xed-gheaders.H"
#include "xed-state.H"
#include "xed-decoded-resource.H"
#include "xed-immdis.H"


namespace XED { 
using namespace std;

#define XED_MAX_MEMOPS_PER_INST 2

/// This holds common fields used by both the encoder
/// #XED::xed_encoder_request_t 
/// and the decoder 
/// #XED::xed_decoded_inst_t. 
///
/// The decoder fills this in when decoding pre-existing instructions. 
/// Users of the encoder also fill this in to describe an instruction to be
/// encoded.  It holds:
/// -# The instruction class (ADD, SUB, etc.) as
///  a type
/// XED::xed_iclass_t .
/// -# the operands array opnd_res[] whose elements are of
/// type 
/// #XED::xed_decoded_resource_t .
/// -# The memory operand information, also contained in this class, is stored
/// outside of the operands array
/// and is set up separately. There is storage for two memory operations, one of
///  which can have an index and scale. Also only one memory operation can have 
///  a displacement.
/// -# The immediate storage ( #XED::xed_immdis_t )
/// -# The displacement storage (for memory operations or relative branches) ( #XED::xed_immdis_t )
/// -# The effective operand width as 
/// a type
/// #opnd_width_t .
/// -# The effective addressing width as 
/// a type
/// #addr_width_t .
class XED_DLL_EXPORT xed_common_fields_t : public xed_state_t, public xed_legacy_prefix_t
{
  public:

    /// @name Constructors and initalization
    //@{ 
    /// Constructor. If you use this constructor, you must make sure
    /// taht the set_state() function is called.
    xed_common_fields_t() 
        : disp(XED_MAX_DISPLACEMENT_BYTES),
          immed(XED_MAX_IMMEDIATE_BYTES)
    { 
        // This call to zero() is commented out because the decoder and
        // encoder both zero this when doing their own zero-ing.

        // zero();
    }

    /// Constructor
    xed_common_fields_t(const xed_state_t& arg_dstate) 
        : xed_state_t(arg_dstate),
          disp(XED_MAX_DISPLACEMENT_BYTES),
          immed(XED_MAX_IMMEDIATE_BYTES)
    { 
        zero();
    }

    /// Set the machine state. Required initialization.
    void
    set_state(const xed_state_t& x) ;

    //@}

  protected:
    /// The instruction class.
    xed_iclass_t iclass:16; 

    /// counter for accessing opnd_res[] array
    UINT8 opnd_res_fill:4;

    /// counter for accessing base[], seg_reg[] and mem_rw_action[] arrays.
    UINT8 memop_fill:4;

    /// The decoded operands.
    xed_decoded_resource_t opnd_res[XED_MAX_OPERANDS]; 

    /// Which memory reference corresponds to a particular operand (if any).
    /// -1 indicates no memory reference.
    INT8 map_template_operand_to_memref[XED_MAX_OPERANDS];

    /// Which operand corresponds to a particular memory reference (if any).
    /// -1 indicates corresponding operand -- also indicates no memory info.
    INT8 map_memref_to_template_operand[XED_MAX_MEMOPS_PER_INST];

    /// The memory references

    /// The segment registers associated with the memory references
    xedregs_t seg_reg[XED_MAX_MEMOPS_PER_INST];

    /// The base register for memory references, if any.
    xedregs_t base[XED_MAX_MEMOPS_PER_INST]; 
    
    /// The index register for memory references, if any.
    xedregs_t index :8;

    /// The scale register for memory references, if any.  Defaults to 1.
    UINT8     scale :4 ; 

    bool displacement_for_memop :1 ;
    bool default_seg_reg0 :1 ;
    bool default_seg_reg1 :1 ;

    /// Stores the actual displacement
    xed_immdis_t disp; 

    /// The memory reference R/W action
    xed_opnd_action_t mem_rw_action[XED_MAX_MEMOPS_PER_INST];

    /// Stores the actual immediate
    xed_immdis_t immed;

    /// effective operand width
    opnd_width_t effective_operand_width : XED_BIT_FIELD_PSEUDO_WIDTH4; // 8, 16, 32, 64, 128...
    /// effective address width
    addr_width_t effective_address_width : XED_BIT_FIELD_PSEUDO_WIDTH4; // see address-width.H 


    /// The length of the memory operand in bytes
    UINT16  memop_length;


  public:
    
    /// @name The instruction class 
    //@{ 
    /// Set the instruction class. Required for encoding.
    /// @param arg_iclass The instruction class of the instruction. 
    inline void 
    set_iclass(xed_iclass_t arg_iclass) {
        iclass = arg_iclass;
    }

    /// The instruction class (ADD, SUB, etc. )
    /// @return the iclass of the instruction
    inline xed_iclass_t 
    get_iclass() const { 
        return iclass;
    }
    //@}

    /// @name Adding information about the instruction to encode.
    //@{

    /// Set the effective operand width. This is an input to the
    /// encoder and an output of the decoder. The value is ignored
    /// by decode.
    inline void 
    set_effective_operand_width(opnd_width_t arg_opnd_width) 
    {
        effective_operand_width = arg_opnd_width;
    }


    /// Return the XED::opnd_width_t describing the overall operation
    /// width, 8b, 16b, 32b, 64b. The byte multiplies are considered to be
    /// 8b operations even though their result is a 16b
    /// operation to enable distingiushing them from the scalable width
    /// multiply opcodes. Extending moves are considered to be the width of
    /// their destination operand.
    inline opnd_width_t
    get_effective_operand_width() const 
    {
        return effective_operand_width;
    }

    inline void 
    set_effective_address_width(addr_width_t arg_addr_width) 
    {
        effective_address_width = arg_addr_width;
    }

    inline addr_width_t
    get_effective_address_width() const 
    {
        return effective_address_width;
    }

    //@}

    /// @name Access the memory addressing information
    //@{
    /// The base reg for memory refs. Could be invalid.
    inline xedregs_t 
    get_seg_reg(int i=0) const { 
        assert(i < XED_MAX_MEMOPS_PER_INST);
        return seg_reg[i];
    }
 
    /// Return true if the segment register for memop i is the default segment register.
    inline bool
    default_seg_reg(int i) const {
        if (i==0)
            return default_seg_reg0;
        return default_seg_reg1;
    }

    /// @param memop_idx The index of the memory operation: 0 or 1.
    /// @return true if the memory operand is just a base register (no
    /// index or displacement).
    bool
    just_base_reg(int memop_idx=0) const;
    

    /// Returns the base register.
    /// @param memop_idx The index of the memory operation: 0 or 1.
    /// @return Returns the base register as a
    /// xedregs_t.
    inline xedregs_t 
    get_base_reg(int memop_idx=0) const 
    { 
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        return base[memop_idx];
    }

    /// The index reg for memory refs. Could be invalid. Only the first memory
    /// operation has an index register. If you pass in 1, you get XEDREG_INVALID.
    /// @param memop_idx The index of the memory operation: 0 or 1.
    /// @return Returns the index register.
    inline xedregs_t
    get_index_reg(int memop_idx=0) const 
    { 
        if (memop_idx == 0) 
        {
#if defined(__GNUC__)
            return index;
#else
            return static_cast<xedregs_t>( XED_BYTE_MASK(index) );
#endif
        }
        return XEDREG_INVALID;
    }

    /// The scale for indexed addressing. Default is 1.
    /// @return the scale value: 1,2, 4 or 8.
    inline unsigned int
    get_scale() const { 
        return scale;
    }
    //@}

    /// @name Displacement accessors
    //@{
    /// Only returns true if the displacement is for the memory operation.
    /// Note there must be a valid displacement for this to return true.
    /// @return True if the displacement is for a memory operation, and false
    /// if the displacement is for a relative control transfer.
    inline  bool
    get_displacement_for_memop() const
    {
        const unsigned int disp_bytes = get_disp().get_bytes();
        if (disp_bytes != 0)
        {
            return displacement_for_memop;
        }
        return false;
    }

    /// Indicate if the displacement is for the memory access or for a
    /// branch displacement.
    /// @param for_memop Set to true if the displacement is for a memory operation, or 
    /// false if the displacement is for a relative control transfer.
    inline void
    set_displacement_for_memop(bool for_memop = true) 
    {
        displacement_for_memop = for_memop;
    }
    /// Set the actual dispalcement value
    /// @param d a container for the displacmenet bytes
    inline void
    set_disp(const xed_immdis_t& d) {
        disp = d;
    }

    /// Get the displacement value. 
    /// @return a constant reference to the #XED::xed_immdis_t holding the displacement value
    inline const xed_immdis_t& 
    get_disp() const {
        return disp;
    }

    /// Returns true if there is a valid displacement that has been filled in
    inline bool
    has_disp() const {
        return disp.is_present();
    }
    
    /// Internal function; Ignore.
    inline void 
    set_disp_start(unsigned int byte_offset)
    {
        disp.set_start_offset(byte_offset);
    }

    /// Update the displacement value of an already encoded instruction at the
    /// given location without re-encoding.
    ///
    /// @param new_displacement the new displacement value to insert in to the itext
    /// @param nbytes the length of the displacement
    /// @param itext the start of the encoded instruction text in memory. Must be writable.
    ///
    /// @return true iff the nbytes size matches the existing displacement
    ///         size. if false, the instruction is not updated and a re-encode is
    ///         required.

    bool
    update_displacement(UINT64 new_displacement, unsigned int nbytes, UINT8* itext);

    //@}

    /// @name Immediate accessors
    //@{
    /// Set the actual immediate value
    inline void
    set_immed(const xed_immdis_t& i) {
        immed = i;
    }

    /// Get the  immediate value
    inline const xed_immdis_t& 
    get_immed() const {
        return immed;
    }

    /// @return True if the immediate is  present
    inline  bool
    has_immed() const {
        return immed.is_present();
    }

    /// @return True if the immediate is unsigned
    inline bool
    immed_unsigned() const
    {
        return immed.is_unsigned();
    }
    
    /// @return True if the immediate is signed
    inline bool
    immed_signed() const
    {
        return immed.is_signed();
    }
  
    /// Internal function; Ignore.
    inline void 
    set_immed_start(unsigned int byte_offset)
    {
        immed.set_start_offset(byte_offset);
    }

    /// Update the immediate value of an already encoded instruction at the
    /// given location without re-encoding.
    ///
    /// @param new_immediate the new immediate value to insert in to the itext
    /// @param nbytes the length of the immediate
    /// @param itext the start of the encoded instruction text in memory. Must be writable.
    ///
    /// @return true iff the nbytes size matches the existing immediate
    ///         size. if false, the instruction is not updated and a re-encode is
    ///         required.

    bool
    update_immediate(UINT64 new_immediate, unsigned int nbytes, UINT8* itext);
    //@}


    /// @name Pointers between memory operations, request operands and template operands
    //@{
  private:
    void
    set_operand_to_memop_links(int memop_idx, int operand_idx) 
    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        assert(operand_idx < XED_MAX_OPERANDS);

        //XTMSG("Linking memop " << memop_idx << " to tmplt operand " << operand_idx);
        map_memref_to_template_operand[ memop_idx ] = static_cast<INT8>(operand_idx);
        map_template_operand_to_memref[ operand_idx ] = static_cast<INT8>(memop_idx);
    }

  public:
    /// Return an index in to the operand array for the given memory
    /// operand. Note: -1 means no memory reference.
    int
    get_template_operand_index_for_memop(int memop_idx) const 
    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        return map_memref_to_template_operand[ memop_idx ];
    }

    /// Return an index in to the memory operands given an operand
    /// array index. Note: -1 means invalid operand. There should not
    /// be any memory reference in that case.
    int
    get_memop_index_for_template_operand(int template_operand_idx) const 
    {
        assert(template_operand_idx < XED_MAX_OPERANDS);
        return map_template_operand_to_memref[ template_operand_idx ];
    }

    /// Return the memory opreration index for a given request operand, or -1 if there
    /// is no associated memory operation.
    int
    get_memop_index_for_request_operand(unsigned int request_operand_idx) const;

    /// Return the request operand index for a given memory operation, or
    /// -1. Note: Uses a potentially slow scan of all the operands.
    int
    get_request_operand_index_for_memop(unsigned int memop_index) const;


    //@}

    /// @name Setting up the memory access information for a specific memory operation
    //@{

    /// Set the segment register information for memop 0 or 1.
    /// @param memop_idx the index of the memop: 0 or 1
    /// @param arg_seg the segment register.
    /// @param default_seg true iff the segment register is the default segment register for this operation.
    inline void
    set_seg_reg(int memop_idx, 
                xedregs_t arg_seg,
                bool default_seg=true) 
    { 
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        seg_reg[memop_idx] = arg_seg;

        if (memop_idx == 0)
            default_seg_reg0 = default_seg;
        else
            default_seg_reg1 = default_seg;
    }

    ///////////////////////////////////////////////////////////////
    
    /// This is for register reallocation. The base register and memop must
    /// already be set up.
    /// @param memop_idx the index of the memop: 0 or 1
    /// @param arg_base the base register, could be XEDREG_INVALID
    inline void
    update_base(unsigned int memop_idx,
             const xedregs_t arg_base)
    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        base[memop_idx] = arg_base;
    }

    /// This is for register realloation. The memop must already be setup.
    /// @param arg_index the index register, could be XEDREG_INVALID
    inline void
    update_index(xedregs_t arg_index)
    {
        index = arg_index;
    }

    /// Set the base register information for memop 0 or 1. Link it to 
    /// the operand array with the arg_operand_idx.
    /// @param memop_idx the index of the memop: 0 or 1
    /// @param arg_base the base register, could be XEDREG_INVALID
    /// @param arg_rw the operand action (r/w, etc.)
    /// @param arg_operand_idx the index of the operand in the operand array
    inline void
    set_ith_base(unsigned int memop_idx,
                 xedregs_t arg_base,
                 const xed_opnd_action_t arg_rw,
                 const int arg_operand_idx)
    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        base[memop_idx] = arg_base;
        mem_rw_action[memop_idx] = arg_rw;
        set_seg_reg(memop_idx, XEDREG_DS);
        set_operand_to_memop_links(memop_idx, arg_operand_idx);
    }

    /// Set the base and segment information for memop 0 or 1. 
    /// @param memop_idx the index of the memop: 0 or 1
    /// @param arg_base the base register, could be XEDREG_INVALID
    /// @param arg_seg the segment register, if not the default
    /// @param arg_rw the operand action (r/w, etc.)
    /// @param arg_operand_idx the index of the operand in the operand array
    /// @param override_seg True if the segment is not the default segment.
    inline void
    set_ith_base_seg(unsigned int memop_idx,
                     xedregs_t arg_base,
                     xedregs_t arg_seg,
                     const xed_opnd_action_t arg_rw,
                     const int arg_operand_idx,
                     const bool override_seg = false)

    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        base[memop_idx] = arg_base;
        set_seg_reg(memop_idx, arg_seg,!override_seg);
        mem_rw_action[memop_idx] = arg_rw;
        set_operand_to_memop_links(memop_idx, arg_operand_idx);
    }

    
    /// Set the memop info. Note the segment register is an optional last
    /// argument and defaults to XEDREG_DS. Note: If a valid displacement
    /// length is provided, this associates the displacement with the
    /// memory operation.
    /// @param memop_idx the index of the memop: 0 or 1
    /// @param arg_base the base register, could be XEDREG_INVALID
    /// @param arg_index the index register, could be XEDREG_INVALID
    /// @param arg_scale the scale value 1,2,4,8
    /// @param arg_disp  an enumeration value indicating the displacement size
    /// @param arg_rw the operand action (r/w, etc.)
    /// @param arg_operand_idx the index of the operand in the operand array
    /// @param arg_seg the segment register
    /// @param override_seg True if the segment is not the default segment.
    inline void
    set_ith_base_index_scale_displacement(unsigned int memop_idx,
                                          xedregs_t arg_base,
                                          xedregs_t arg_index,
                                          UINT arg_scale,
                                          const xeddisplacement_t arg_disp,
                                          const xed_opnd_action_t arg_rw,
                                          const int arg_operand_idx,
                                          xedregs_t arg_seg = XEDREG_INVALID,
                                          const bool override_seg=false)
        
    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        base[memop_idx] = arg_base;
        set_seg_reg(memop_idx, arg_seg, !override_seg);
        index = arg_index;
        scale = arg_scale;
        //XTMSG("Setting scale to " << (int)scale);
        mem_rw_action[memop_idx] = arg_rw;
        set_operand_to_memop_links(memop_idx, arg_operand_idx);

        if( arg_disp != XEDDISPLACEMENT_INVALID ) 
        {
            set_displacement_for_memop(true);
        }
    }
    //@}

    /// @name Setting up the memory access information for the NEXT memory operation
    //@{

    /// Set the base register.
    /// Advances the memop_fill pointer as a side effect.
    /// @param arg_base the base register, could be XEDREG_INVALID
    /// @param arg_rw the operand action (r/w, etc.)
    /// @param arg_operand_idx the index of the operand in the operand array
    inline void
    set_base(xedregs_t arg_base,
             const xed_opnd_action_t arg_rw,
             const int arg_operand_idx)
    {
        set_ith_base(memop_fill, arg_base, arg_rw, arg_operand_idx);
        memop_fill++;
    }

    /// Set the base and segment registers
    /// Advances the memop_fill pointer as a side effect.
    /// @param arg_base the base register, could be XEDREG_INVALID
    /// @param arg_seg the segment register
    /// @param arg_rw the operand action (r/w, etc.)
    /// @param arg_operand_idx the index of the operand in the operand array
    /// @param override_seg True if the segment is not the default segment.
    inline void
    set_base_seg(xedregs_t arg_base,
                 xedregs_t arg_seg,
                 const xed_opnd_action_t arg_rw,
                 const int arg_operand_idx,
                 const bool override_seg = false)

    {
        set_ith_base_seg(memop_fill, arg_base, arg_seg, arg_rw, arg_operand_idx, 
                         override_seg);
        memop_fill++;
    }

    /// Set the scale value.
    /// Note: only one memop can have a scale value.
    /// @param ascale the scale value 1,2,4,8
    inline void
    set_scale(unsigned int ascale)
    {
        assert(ascale == 1 || ascale == 2 || ascale == 4 || ascale == 8);
        scale = ascale;
    }
    
    /// Set all of memory operation information. Note, the segment register is an optional last
    /// argument and defaults to XEDREG_DS Note: If a valid displacement
    /// length is provided, this associates the displacement with the
    /// memory operation.
    /// Advances the memop_fill pointer as a side effect.
    /// @param arg_base the base register, could be XEDREG_INVALID
    /// @param arg_index the index register, could be XEDREG_INVALID
    /// @param arg_scale the scale value 1,2,4,8
    /// @param arg_disp  an enumeration value indicating the displacement size
    /// @param arg_operand_idx the index of the operand in the operand array
    /// @param arg_seg the segment register, if not the default
    /// @param arg_rw the operand action (r/w, etc.)
    /// @param override_seg True if the segment is not the default segment.
    inline void
    set_base_index_scale_displacement(xedregs_t arg_base,
                                      xedregs_t arg_index,
                                      UINT arg_scale,
                                      const xeddisplacement_t arg_disp,
                                      const int arg_operand_idx,
                                      xedregs_t arg_seg = XEDREG_INVALID,
                                      const xed_opnd_action_t arg_rw = XED_OPND_ACTION_INVALID,
                                      const bool override_seg=false)

    {
        set_ith_base_index_scale_displacement(memop_fill,
                                              arg_base,
                                              arg_index,
                                              arg_scale,
                                              arg_disp,
                                              arg_rw,
                                              arg_operand_idx,
                                              arg_seg,
                                              override_seg);
        memop_fill++;
    }
    //@}

    /// @name Memory operand length
    //@{

    /// Set the width of a memory operand. This is required for encoding
    /// most X87 floating point load and store operations. If you decode an
    /// existing operation, this field will be filled in.
    void 
    set_memory_operand_length(unsigned int len)
    {
        memop_length = static_cast<UINT16>(len);
    }
    
    /// Return the width of the memory operation. Note that for prefetches,
    /// XED returns 64B as the length of the memory operation.
    unsigned int 
    get_memory_operand_length() const
    {
        return memop_length;
    }
    //@}

    /// @name  Reading the  operands
    //@{
    /// This is the number of decoded operands.
    inline unsigned int
    get_operand_count() const
    {
        return opnd_res_fill;
    }

    /// These are the decoded operands that the users will want to scan.
    /// It is returned as a constant reference. There is also a nonconstant
    /// accessor.
    /// @param n The n'th operand array value
    inline const xed_decoded_resource_t&
    get_operand_resource(int n) const
    {
        assert(n < opnd_res_fill);
        return opnd_res[n];
    }

    /// Returns the decoded resource as a non-constant reference.
    /// @param n The n'th operand array value
    /// These are the decoded operands that the users will want to scan
    inline xed_decoded_resource_t&
    get_operand_resource_nonconst(int n)
    {
        assert(n < opnd_res_fill);
        return opnd_res[n];
    }
    

    /// Returns the register in the n't operand array index.
    /// Note, you must know that it is a register already.
    /// @param n The n'th operand array value
    inline xedregs_t
    get_operand_reg(unsigned int n) const
    {
        assert(n < opnd_res_fill);
        return opnd_res[n].get_reg();
    }
    //@}

    /// @name  Set up or modifying the main operand array
    //@{

    /// This should only be used to update the operand register. It must
    /// already be a fully defined register resource. This is useful for
    /// register reallocation.
    /// @param n operand array index
    /// @param r register name
    inline void
    update_operand_reg(unsigned int n, xedregs_t r)
    {
        assert(n < opnd_res_fill);
        opnd_res[n].update_reg(r);
    }
    
    /// for adding registers to the operand array. 
    /// Advances a fill pointer as a side effect.
    /// @param res the type of resource
    /// @param r  the name of the register
    /// @param opvis the operand visibility (explicit, implicit, supressed). Not required for encoding.
    /// @param template_index index of the template for this operand. Not required for encoding.
    /// @param rw operand action (read/written, etc.). Not required for encoding.
    inline void
    set_next_operand_reg(const xed_resource_t res,
                         const xedregs_t r,
                         const xed_opvis_enum_t opvis = XED_OPVIS_EXPLICIT, 
                         const unsigned int template_index = XED_INVALID_TEMPLATE_INDEX,
                         const xed_opnd_action_t rw = XED_OPND_ACTION_RW)
    {
        // for real register resources
        assert(opnd_res_fill < XED_MAX_OPERANDS);
        if (res != XED_RESOURCE_INVALID) 
        {
            opnd_res[opnd_res_fill++].set(res,r,opvis,rw, template_index);
        }
    }

    /// for adding pseudo-resources to the operands array.
    /// Advances a fill pointer as a side effect.
    /// @param res the type of resource
    /// @param pres  the name of the pseudo register
    /// @param opvis the operand visibility (explicit, implicit, supressed). Not required for encoding.
    /// @param template_index index of the template for this operand. Not required for encoding.
    /// @param rw operand action (read/written, etc.). Not required for encoding.
    inline void
    set_next_operand_res(const xed_resource_t res,
                         const xed_pseudo_resource_t pres,
                         const xed_opvis_enum_t opvis = XED_OPVIS_EXPLICIT, 
                         const unsigned int template_index = XED_INVALID_TEMPLATE_INDEX,
                         const xed_opnd_action_t rw = XED_OPND_ACTION_RW)
    {
        // for pseudo resources
        assert(opnd_res_fill < XED_MAX_OPERANDS);
        if (res != XED_RESOURCE_INVALID)
        {
            opnd_res[opnd_res_fill++].set(res,pres,opvis,rw,template_index);
        }
    }

    /// for adding memory references to the operands array. The details of the
    /// memory reference are set up separately.
    /// Advances a fill pointer as a side effect.
    /// @param opvis the operand visibility (explicit, implicit, supressed). Not required for encoding.
    /// @param template_index index of the template for this operand. Not required for encoding.
    /// @param rw operand action (read/written, etc.). Not required for encoding.
    inline void
    set_next_operand_mem(const xed_opvis_enum_t opvis = XED_OPVIS_EXPLICIT, 
                         const unsigned int template_index = XED_INVALID_TEMPLATE_INDEX,
                         const xed_opnd_action_t rw = XED_OPND_ACTION_RW)
    {
        xed_resource_t memres; 
        if (memop_fill == 0) 
        {
            memres = XED_RESOURCE_MEM0;
        }
        else
        {
            memres = XED_RESOURCE_MEM1;
        }

        set_next_operand_res(memres,
                             XED_PSEUDO_RES_INVALID,
                             opvis,
                             template_index,
                             rw);
    }
 
    /// For setting the required AGEN (address generation) for LEA instructions.
    /// The details of the
    /// memory address compuation  are set up separately.
    /// Advances a fill pointer as a side effect.
    /// @param opvis the operand visibility (explicit, implicit, supressed). Not required for encoding.
    /// @param template_index index of the template for this operand. Not required for encoding.
    inline void
    set_next_operand_agen(const xed_opvis_enum_t opvis = XED_OPVIS_EXPLICIT,
                          const unsigned int template_index = XED_INVALID_TEMPLATE_INDEX)
    {
        set_next_operand_res(XED_RESOURCE_AGEN,
                             XED_PSEUDO_RES_INVALID,
                             opvis,
                             template_index,
                             XED_OPND_ACTION_R);  // FIXME: might this be invalid?
    }

    /// This is only used for the ENTER instruction's 2nd immediate value.
    /// Advances a fill pointer as a side effect.
    /// @param immval2 The 2nd immediate value for ENTER. One byte.
    /// @param opvis the operand visibility (explicit, implicit, supressed). Not required for encoding.
    /// @param template_index index of the template for this operand. Not required for encoding.
    /// @param rw operand action (read/written, etc.). Not required for encoding.
    inline void
    set_next_operand_imm8(UINT8 immval2,
                          const xed_opvis_enum_t opvis = XED_OPVIS_EXPLICIT, 
                          const unsigned int template_index = XED_INVALID_TEMPLATE_INDEX,
                          const xed_opnd_action_t rw = XED_OPND_ACTION_R)
    {
        assert(opnd_res_fill < XED_MAX_OPERANDS);
        opnd_res[opnd_res_fill++].set(immval2,opvis,rw, template_index);
    }

    //@}

    /// @name re-initialization functions
    //@{
    /// Clear the class.
    void 
    zero() 
    {
        zero_displacement();
        zero_immediate();
        iclass = XEDICLASS_INVALID;
        effective_operand_width = OPND_WIDTH_INVALID;
        effective_address_width = ADDR_WIDTH_INVALID;
        zero_operands();
        zero_mem_ref_info();
        default_seg_reg0 = true;
        default_seg_reg1 = true;
    }
    
    /// clear the displacement
    void
    zero_displacement()
    {
        displacement_for_memop = false;
        disp.set_max_len(XED_MAX_DISPLACEMENT_BYTES);
        disp.zero();
    }

    /// clear the immediate
    void
    zero_immediate()
    {
        immed.set_max_len(XED_MAX_DISPLACEMENT_BYTES);
        immed.zero();
    }
    
    /// clears out memory reference information
    void 
    zero_mem_ref_info();
    
    /// clears out memory actions
    void
    zero_mem_action() 
    {
        for ( int i=0 ; i<XED_MAX_MEMOPS_PER_INST ; i++ )  
        {
            mem_rw_action[i] = XED_OPND_ACTION_INVALID;
        }
    }

    /// clears out the operands array.
    void
    zero_operands()  
    {
        opnd_res_fill = 0; 

        for(int i=0;i<XED_MAX_OPERANDS;i++) 
        {
#if defined(XED_SQUEAKY_CLEAN)
            opnd_res[i].zero();
#else
            opnd_res[i].invalidate();
#endif
            map_template_operand_to_memref[i] = -1;
        }
    }

    //@}


    /// @name memory read / write info
    //@{

    /// Get a memop's read/write action. i= 0 or 1
    /// @param memop_idx memory operation index: 0 or 1.
    /// @return The operand action (r,w,rw, etc.)
    xed_opnd_action_t 
    get_mem_rw(int memop_idx) const 
    {
        assert(memop_idx < XED_MAX_MEMOPS_PER_INST);
        return mem_rw_action[memop_idx];
    }

    /// counts the number of memory operands. Note, if we load and
    /// store one operand, that is only one. The string ops can have
    /// two memory operands.
    /// @return the number of memory operands, 0, 1 or 2.
    inline unsigned int
    number_of_memory_operands() const 
    {
        return memop_fill;
    }

    /// Returns true if the operand accesses memory. Note that because
    /// of LEA's use of the memory information for AGENs, we cannot
    /// use the number_of_memory_operands() as the proxy for this.
    /// @return True if the instruction accesses memory with one of its memory operands.
    bool
    accesses_memory() const 
    {
        for(int i=0; i < memop_fill ; i++)
        {
            if (mem_rw_action[i] != XED_OPND_ACTION_INVALID) 
            {
                return true;
            }
        }
        return false;
    }

    /// Returns true if the i'th memory operand (starting from 0) reads
    /// memory. Note: it could also write memory.
    /// @param i memory operation index: 0 or 1.
    inline bool 
    mem_read(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_rw_action[i] == XED_OPND_ACTION_R || 
            mem_rw_action[i] == XED_OPND_ACTION_RW ||
            mem_rw_action[i] == XED_OPND_ACTION_RCW;
    }

    /// Returns true if the i'th memory operand (starting from 0) writes
    /// memory. Note: it could also read memory. Write may be conditional.
    /// @param i memory operation index: 0 or 1.
    inline bool 
    mem_written(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_may_write(i);
    }

    /// Returns true if the i'th memory operand (starting from 0)
    /// reads and writes memory. Read or write may be conditional.
    /// @param i memory operation index: 0 or 1.
    inline bool 
    mem_read_and_written(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_rw_action[i] == XED_OPND_ACTION_RW ||
            mem_rw_action[i] == XED_OPND_ACTION_RCW || 
            mem_rw_action[i] == XED_OPND_ACTION_CRW ;
    }

    /// Returns true if the i'th memory operand (starting from 0) reads
    /// memory. 
    /// @param i memory operation index: 0 or 1.
    inline bool 
    mem_read_only(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_rw_action[i] == XED_OPND_ACTION_R;
    }

    /// Returns true if the i'th memory operand (starting from 0) writes
    /// memory. Write may be conditional.
    /// @param i memory operation index: 0 or 1.
    inline bool 
    mem_written_only(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_rw_action[i] == XED_OPND_ACTION_W ||
            mem_rw_action[i] == XED_OPND_ACTION_CW;

    }

    /// May write memory (may-write, read-and-may-write)
    /// @param i memory operation index: 0 or 1.
    inline bool
    mem_may_write(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_must_write(i) || 
            mem_rw_action[i] == XED_OPND_ACTION_CW ||
            mem_rw_action[i] == XED_OPND_ACTION_RCW;
    }

    /// Must write memory (must-write, read-and-must-write, conditinal-read-and-must-write)
    /// @param i memory operation index: 0 or 1.
    inline bool
    mem_must_write(unsigned int i) const 
    {
        assert( i < XED_MAX_MEMOPS_PER_INST);
        return mem_rw_action[i] == XED_OPND_ACTION_W ||
            mem_rw_action[i] == XED_OPND_ACTION_RW ||
            mem_rw_action[i] == XED_OPND_ACTION_CRW ;
    }
    //@}

    /// @name Operand scans
    //@{
    
    /// Returns true if the specified generic-width register (rAX, etc.) is written
    bool 
    operand_genreg_scan_written(const xed_genreg_t gr) const;

    /// Returns true if any registers are conditionally written
    bool
    conditionally_writes_registers() const;

    //@}
#if XED_PRINT==1
    /// @name Printing and debugging
    //@{

    // One-line disassembly output. The instruction address, if nonzero,
    // is added to relative offsets to compute addresses for IP relative
    // data.
    void
    print_short(ostream& o, UINT64 instruction_address=0) const;
  private:
    void
    print_memop_short(ostream& o, unsigned int memop, UINT64 instruction_address=0) const ;
  public:

    // Detailed info.
    void
    print(ostream& o) const;

    void
    print_operand_regs(ostream& o) const;
    void
    print_memop_info(ostream& o) const;
    void
    print_memop_info(ostream& o, unsigned int i) const;

    //@}
#endif
} ; //  __attribute__((packed));
} //namespace


#endif

////////////////////////////////////////////////////////////////////////////

//Local Variables:
//pref: "../../xed-common-fields.cpp"
//End:
