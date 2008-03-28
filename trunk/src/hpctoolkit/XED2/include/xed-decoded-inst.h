/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
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
/// @file xed-decoded-inst.h
/// @author Mark Charney <mark.charney@intel.com>

#if !defined(_XED_DECODER_STATE_H_)
# define _XED_DECODER_STATE_H_
#include "xed-common-hdrs.h"
#include "xed-common-defs.h"
#include "xed-portability.h"
#include "xed-util.h"
#include "xed-types.h"
#include "xed-operand-values-interface.h" 
#include "xed-inst.h"
#include "xed-flags.h"
#include "xed-encoder-gen-defs.h" //generated


// fwd-decl xed_simple_flag_t;
// fwd-decl xed_inst_t;


struct xed_encoder_vars_s;
struct xed_decoder_vars_s;

/// @ingroup DEC
/// The main container for instructions. After decode, it holds an array of
/// operands with derived information from decode and also valid
/// #xed_inst_t pointer which describes the operand templates and the
/// operand order.  See @ref DEC for API documentation.
typedef struct XED_DLL_EXPORT xed_decoded_inst_s  {
    /// The operand storage fields discovered during decoding. This same array is used by encode.
    xed_operand_values_t _operands[XED_OPERAND_LAST]; // FIXME: can further squeeze down 16b units

    /// Used for encode operand ordering. Not set by decode.
    uint8_t _operand_order[XED_ENCODE_ORDER_MAX_OPERANDS];

    uint8_t _decoded_length;
    // Length of the _operand_order[] array.
    uint8_t _n_operand_order; 

    /// when we decode an instruction, we set the _inst and get the
    /// properites of that instruction here. This also points to the
    /// operands template array.
    const xed_inst_t* _inst;

    // decoder does not change it, encoder does    
    union {
        uint8_t* _enc;
        const uint8_t* _dec;
    } _byte_array; 

    // These are stack allocated by xed_encode() or xed_decode(). These are
    // per-encode or per-decode transitory data.
    union {

        /* user_data is available as a user data storage field after
         * decoding. It does not live across re-encodes or re-decodes. */
        uint64_t user_data; 
        struct xed_decoder_vars_s* dv;
        struct xed_encoder_vars_s* ev;
    } u;


    
} xed_decoded_inst_t;



/// @name xed_decoded_inst_t Operands array access
//@{
/// @ingroup DEC
/// Obtain a constant pointer to the operands
static XED_INLINE const xed_operand_values_t* 
xed_decoded_inst_operands_const(const xed_decoded_inst_t* p) {
    return STATIC_CAST(xed_operand_values_t*,p->_operands);
}
/// @ingroup DEC
/// Obtain a non-constant pointer to the operands
static XED_INLINE xed_operand_values_t* 
xed_decoded_inst_operands(xed_decoded_inst_t* p) {
    return STATIC_CAST(xed_operand_values_t*,p->_operands);
}
//@}

/// @name xed_decoded_inst_t Initialization
//@{
/// @ingroup DEC
/// Zero the decode structure, but set the machine state/mode information
XED_DLL_EXPORT void  xed_decoded_inst_zero_set_mode(xed_decoded_inst_t* p, const xed_state_t* dstate);
/// @ingroup DEC
/// Zero the decode structure, but preserve the existing machine state/mode information
XED_DLL_EXPORT void  xed_decoded_inst_zero_keep_mode(xed_decoded_inst_t* p);
/// @ingroup DEC
/// Zero the decode structure completely.
XED_DLL_EXPORT void  xed_decoded_inst_zero(xed_decoded_inst_t* p);
/// @ingroup DEC
/// Zero the decode structure, but copy the existing machine state/mode information from the supplied operands pointer.
XED_DLL_EXPORT void  xed_decoded_inst_zero_keep_mode_from_operands(xed_decoded_inst_t* p,
                                                                   const xed_operand_values_t* operands);
//@}

/// @name xed_decoded_inst_t Length 
//@{
/// @ingroup DEC
/// Return the length of the decoded  instruction in bytes.
static XED_INLINE uint_t
xed_decoded_inst_get_length(const xed_decoded_inst_t* p) {  
    return p->_decoded_length;
}


//@}

/// @name modes
//@{
/// @ingroup DEC
static XED_INLINE uint_t xed_decoded_inst_get_mode(const xed_decoded_inst_t* p) {
    return p->_operands[XED_OPERAND_MODE];
}
/// @ingroup DEC
static XED_INLINE uint_t xed_decoded_inst_get_address_mode(const xed_decoded_inst_t* p) {
    return p->_operands[XED_OPERAND_AMODE];
}
/// @ingroup DEC
static XED_INLINE uint_t xed_decoded_inst_get_stack_address_mode(const xed_decoded_inst_t* p) {
    return p->_operands[XED_OPERAND_SMODE];
}
//@}


///////////////////////////////////////////////////////
/// API
///////////////////////////////////////////////////////

/// @name xed_decoded_inst_t High-level accessors
//@{
/// @ingroup DEC
/// Return true if the instruction is valid
static XED_INLINE bool xed_decoded_inst_valid(const xed_decoded_inst_t* p ) {
    return STATIC_CAST(bool,(p->_inst != 0));
}
/// @ingroup DEC
/// Return the #xed_inst_t structure for this instruction. This is the route to the basic operands form information.
static XED_INLINE const xed_inst_t* xed_decoded_inst_inst( const xed_decoded_inst_t* p) {
    return p->_inst;
}


/// @ingroup DEC
/// Return the instruction category enumeration
static XED_INLINE xed_category_enum_t xed_decoded_inst_get_category(const xed_decoded_inst_t* p) {
    xed_assert(p->_inst != 0);
    return xed_inst_category(p->_inst);
}
/// @ingroup DEC
/// Return the instruction extension enumeration
static XED_INLINE xed_extension_enum_t xed_decoded_inst_get_extension( const xed_decoded_inst_t* p) {
    xed_assert(p->_inst != 0);
    return xed_inst_extension(p->_inst);
}
/// @ingroup DEC
/// Return the instruction class enumeration.
static XED_INLINE xed_iclass_enum_t xed_decoded_inst_get_iclass( const xed_decoded_inst_t* p){
    xed_assert(p->_inst != 0);
    return xed_inst_iclass(p->_inst);
}

/// @ingroup DEC
/// Returns 1 if the attribute is defined for this instruction.
XED_DLL_EXPORT uint32_t xed_decoded_inst_get_attribute(const xed_decoded_inst_t* p, xed_attribute_enum_t attr);

/// @ingroup DEC
/// Returns the attribute bitvector
XED_DLL_EXPORT uint32_t xed_decoded_inst_get_attributes(const xed_decoded_inst_t* p);
//@}

/// @name IFORM handling
//@{

/// @ingroup DEC
/// Return the instruction iform enum of type #xed_iform_enum_t .
static XED_INLINE xed_iform_enum_t xed_decoded_inst_get_iform_enum(const xed_decoded_inst_t* p) {
    xed_assert(p->_inst != 0);
    return xed_inst_iform_enum(p->_inst);
}


/// @ingroup DEC
/// Return the instruction zero-based iform number based on masking the
/// corresponding #xed_iform_enum_t. This value is suitable for
/// dispatching. The maximum value for a particular iclass is provided by
/// #xed_iform_max_per_iclass() .
static XED_INLINE unsigned int xed_decoded_inst_get_iform_enum_dispatch(const xed_decoded_inst_t* p) {
    xed_assert(p->_inst != 0);
    return xed_inst_iform_enum(p->_inst) & 0xFF;
}

/// @ingroup DEC
/// Return the maximum number of iforms for a particular iclass.  This
/// function returns valid data as soon as global data is initialized. (This
/// function does not require a decoded instruction as input).
XED_DLL_EXPORT uint32_t xed_iform_max_per_iclass(xed_iclass_enum_t iclass);

#define XED_MASK_IFORM(x) ((x) & 0xFF)

/// @ingroup DEC
/// DEPRECATED Return the instruction iform number. The iform is zero-based number of
/// the different instances of each iclass. 
static XED_INLINE uint_t xed_decoded_inst_get_old_iform( const xed_decoded_inst_t* p){
    xed_assert(p->_inst != 0);
    return xed_inst_iform(p->_inst);
}

//@}



/// @name xed_decoded_inst_t Operands: Number and Length
//@{
/// Return the length in bytes of the operand_index'th operand.
/// @ingroup DEC
XED_DLL_EXPORT unsigned int  xed_decoded_inst_operand_length(const xed_decoded_inst_t* p, 
                                                             unsigned int operand_index);
/// Return the number of operands
/// @ingroup DEC
static XED_INLINE unsigned int  xed_decoded_inst_noperands(const xed_decoded_inst_t* p) {
    unsigned int noperands = xed_inst_noperands(xed_decoded_inst_inst(p));
    return noperands;
}
//@}

/// @name xed_decoded_inst_t Printers
//@{
/// @ingroup DEC
/// Print out all the information about the decoded instruction to the buffer buf whose length is maximally buflen.
XED_DLL_EXPORT void xed_decoded_inst_dump(const xed_decoded_inst_t* p, char* buf,  int buflen);
/// @ingroup DEC
/// Print the instructions with the destination on the left. Use PTR qualifiers for memory access widths.
/// buflen must be at least 100 bytes.
XED_DLL_EXPORT bool xed_decoded_inst_dump_intel_format(const xed_decoded_inst_t* p, 
                                                       char* buf, 
                                                       int buflen, 
                                                       uint64_t runtime_address) ;

/// @ingroup DEC
/// Print the instructions with the destination operand on the right, with
/// several exceptions (bound, invlpga, enter, and other instructions with
/// two immediate operands).  Also use instruction name suffixes to
/// indicate operation width. Several instructions names are different as
/// well. buflen must be at least 100 bytes.

XED_DLL_EXPORT bool xed_decoded_inst_dump_att_format(const xed_decoded_inst_t* p, 
                                                     char* buf, 
                                                     int buflen, 
                                                     uint64_t runtime_address) ;


/// @ingroup DEC
XED_DLL_EXPORT bool xed_decoded_inst_dump_xed_format(const xed_decoded_inst_t* p,
                                                     char* buf, 
                                                     int buflen, uint64_t runtime_address) ;
//@}

/// @name xed_decoded_inst_t Operand Field Details
//@{
/// @ingroup DEC
XED_DLL_EXPORT xed_reg_enum_t xed_decoded_inst_get_seg_reg(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT xed_reg_enum_t xed_decoded_inst_get_base_reg(const xed_decoded_inst_t* p, unsigned int mem_idx);
XED_DLL_EXPORT xed_reg_enum_t xed_decoded_inst_get_index_reg(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT uint_t xed_decoded_inst_get_scale(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT int64_t xed_decoded_inst_get_memory_displacement(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
/// Result in BYTES
XED_DLL_EXPORT uint_t  xed_decoded_inst_get_memory_displacement_width(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
/// Result in BITS
XED_DLL_EXPORT uint_t  xed_decoded_inst_get_memory_displacement_width_bits(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT int32_t xed_decoded_inst_get_branch_displacement(const xed_decoded_inst_t* p);
/// @ingroup DEC
/// Result in BYTES
XED_DLL_EXPORT uint_t  xed_decoded_inst_get_branch_displacement_width(const xed_decoded_inst_t* p);
/// @ingroup DEC
/// Result in BITS
XED_DLL_EXPORT uint_t  xed_decoded_inst_get_branch_displacement_width_bits(const xed_decoded_inst_t* p);
/// @ingroup DEC
XED_DLL_EXPORT uint64_t xed_decoded_inst_get_unsigned_immediate(const xed_decoded_inst_t* p); 
/// @ingroup DEC
/// Return true if the first immediate (IMM0)  is signed
XED_DLL_EXPORT uint_t xed_decoded_inst_get_immediate_is_signed(const xed_decoded_inst_t* p);
/// @ingroup DEC
/// Return the immediate width in BYTES.
XED_DLL_EXPORT uint_t xed_decoded_inst_get_immediate_width(const xed_decoded_inst_t* p);
/// @ingroup DEC
/// Return the immediate width in BITS.
XED_DLL_EXPORT uint_t xed_decoded_inst_get_immediate_width_bits(const xed_decoded_inst_t* p);
/// @ingroup DEC
XED_DLL_EXPORT int32_t xed_decoded_inst_get_signed_immediate(const xed_decoded_inst_t* p);
/// @ingroup DEC
/// Return the second immediate. 
static XED_INLINE uint8_t xed_decoded_inst_get_second_immediate(const xed_decoded_inst_t* p) {
    return p->_operands[XED_OPERAND_UIMM1];
}

/// @ingroup DEC
/// Return the specified register operand. The specifier is of type #xed_operand_enum_t .
static XED_INLINE xed_reg_enum_t xed_decoded_inst_get_reg(const xed_decoded_inst_t* p, 
                                                          xed_operand_enum_t reg_operand) {
    return STATIC_CAST(xed_reg_enum_t,p->_operands[reg_operand]);
}



/// @ingroup DEC
XED_DLL_EXPORT const xed_simple_flag_t* xed_decoded_inst_get_rflags_info( const xed_decoded_inst_t* p );
/// @ingroup DEC
XED_DLL_EXPORT bool xed_decoded_inst_uses_rflags(const xed_decoded_inst_t* p);
/// @ingroup DEC
XED_DLL_EXPORT uint_t xed_decoded_inst_number_of_memory_operands(const xed_decoded_inst_t* p);
/// @ingroup DEC
XED_DLL_EXPORT bool xed_decoded_inst_mem_read(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT bool xed_decoded_inst_mem_written(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT bool xed_decoded_inst_mem_written_only(const xed_decoded_inst_t* p, unsigned int mem_idx);
/// @ingroup DEC
XED_DLL_EXPORT bool xed_decoded_inst_conditionally_writes_registers(const xed_decoded_inst_t* p);
/// @ingroup DEC
static XED_INLINE unsigned int  xed_decoded_inst_get_memory_operand_length(const xed_decoded_inst_t* p, 
                                                                           unsigned int memop_idx)  {
    return p->_operands[XED_OPERAND_MEM_WIDTH];
    (void)memop_idx;
}


/// @ingroup DEC
/// Returns true if the instruction is a prefetch
XED_DLL_EXPORT bool xed_decoded_inst_is_prefetch(const xed_decoded_inst_t* p);
//@}

                  
/// @name xed_decoded_inst_t Modification
//@{
// Modifying decoded instructions before re-encoding    
/// @ingroup DEC
XED_DLL_EXPORT void xed_decoded_inst_set_scale(xed_decoded_inst_t* p, uint_t scale);
/// @ingroup DEC
/// Set the memory displacement using a BYTE length
XED_DLL_EXPORT void xed_decoded_inst_set_memory_displacement(xed_decoded_inst_t* p, int64_t disp, uint_t length_bytes);
/// @ingroup DEC
/// Set the branch  displacement using a BYTE length
XED_DLL_EXPORT void xed_decoded_inst_set_branch_displacement(xed_decoded_inst_t* p, int32_t disp, uint_t length_bytes);
/// @ingroup DEC
/// Set the signed immediate a BYTE length
XED_DLL_EXPORT void xed_decoded_inst_set_immediate_signed(xed_decoded_inst_t* p, int32_t x, uint_t length_bytes);
/// @ingroup DEC
/// Set the unsigned immediate a BYTE length
XED_DLL_EXPORT void xed_decoded_inst_set_immediate_unsigned(xed_decoded_inst_t* p, uint64_t x, uint_t length_bytes);


/// @ingroup DEC
/// Set the memory displacement a BITS length
XED_DLL_EXPORT void xed_decoded_inst_set_memory_displacement_bits(xed_decoded_inst_t* p, int64_t disp, uint_t length_bits);
/// @ingroup DEC
/// Set the branch displacement a BITS length
XED_DLL_EXPORT void xed_decoded_inst_set_branch_displacement_bits(xed_decoded_inst_t* p, int32_t disp, uint_t length_bits);
/// @ingroup DEC
/// Set the signed immediate a BITS length
XED_DLL_EXPORT void xed_decoded_inst_set_immediate_signed_bits(xed_decoded_inst_t* p, int32_t x, uint_t length_bits);
/// @ingroup DEC
/// Set the unsigned immediate a BITS length
XED_DLL_EXPORT void xed_decoded_inst_set_immediate_unsigned_bits(xed_decoded_inst_t* p, uint64_t x, uint_t length_bits);

//@}

/// @name xed_decoded_inst_t User Data Field
//@{
/// @ingroup DEC
/// Return a user data field for arbitrary use by the user after decoding.
static XED_INLINE  uint64_t xed_decoded_inst_get_user_data(xed_decoded_inst_t* p) {
    return p->u.user_data;
}
/// @ingroup DEC
/// Modify the user data field.
static XED_INLINE  void xed_decoded_inst_set_user_data(xed_decoded_inst_t* p, uint64_t new_value) {
    p->u.user_data = new_value;
}
//@}
#endif
//Local Variables:
//pref: "../../xed-decoded-inst.c"
//End:
