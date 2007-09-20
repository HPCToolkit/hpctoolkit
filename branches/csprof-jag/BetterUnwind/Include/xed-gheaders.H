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
/// @file xed-gheaders.H 
/// @author Mark Charney   <mark.charney@intel.com> 


////////////////////////////////////////////////////////////////////////////

#ifndef _XED_GHEADERS_H_
# define _XED_GHEADERS_H_

#include "xed-types.H"
#include "xed-reg-class.H"
#include "rexr.H"
#include "rexb.H"
#include "rexx.H"
#include "rexw.H"
#include "m64width.H"
#include "modrm-mod.H"
#include "x87-opcode-enum.H"
#include "operand-code.H"
#include "memop.H"
#include "operand-width.H"
#include "operand-width-index.H"
#include "address-width.H"
#include "xed-operand.H"
#include "opvis.H"
#include "xed-opnd-action.H"
#include "xed-prefix-enum.H"
#include "prefix-index.H"
#include "xed-iclass.H"
#include "displacement.H"
#include "xed-group-opcode-maps.H"
#include "xed-x87-cond-code-enum.H"
#include "regs.H"

#include "xed-modes.H"
#include "xed-machine-modes.H"

#include "xed-flags.H" //rflags

#include "xed-tcode.H"

#include "xed-resource.H"
#include "xed-pseudo-res.H"
#include "xed-encoder-subtable.H"
#include "xed-category.H"
#include "xed-extension.H"

#include "xed-opnd-class.H"

#include "xed-opcode-match.H"

#include "xed-common-defs.H" 

namespace XED {
using namespace std;

#define MAX_GPRS 16
#define MODRM_MOD_MAX 4
#define MODRM_REG_MAX 8
#define MODRM_RM_MAX 8
#define SIB_SCALE_MAX 4
#define SIB_INDEX_MAX 8
#define SIB_BASE_MAX 8
#define SIB_BASE_MAX 8
#define MAX_ONE_BYTE_VALUES 0x100


/// These are the internal operands template tables.
class xed_operand_template_t
{
  public:
    xed_operand_enum_t opnd;
    xed_opvis_enum_t opvis;
    xed_opnd_action_t rw;

    xed_operand_template_t() // CONS
        : opnd(XED_OPERAND_NA),
          opvis(XED_OPVIS_INVALID),
          rw(XED_OPND_ACTION_INVALID)
    {
    }

    inline xed_operand_enum_t
    get_opnd() const
    {
        return opnd;
    }

    inline bool reads() const { ///< reads or read+writes
        return rw == XED_OPND_ACTION_R || rw == XED_OPND_ACTION_RW;
    }
    inline bool writes() const { ///< writes or reads+writes
        return rw == XED_OPND_ACTION_W || rw == XED_OPND_ACTION_RW;
    }
    inline bool reads_and_writes() const { ///< reads-and-writes
        return rw == XED_OPND_ACTION_RW;
    }


    inline void 
    set(  xed_operand_enum_t arg_opnd,
          xed_opvis_enum_t arg_opvis,
          xed_opnd_action_t arg_rw    ) { 
        opnd = arg_opnd;
        opvis = arg_opvis;
        // AGEN's do not allow for r/w action
//     if (opnd == XED_OPERAND_AGEN) {
//       rw = XED_OPND_ACTION_INVALID;
//     }
//     else 
        {
            rw = arg_rw;
        }
    }

    inline void
    zero() {
        opnd = XED_OPERAND_NA;
        opvis = XED_OPVIS_INVALID;
        rw = XED_OPND_ACTION_INVALID;
    }

    inline bool 
    invalid() const { 
        return (opnd == XED_OPERAND_NA);
    }
#if XED_PRINT==1
    void 
    print(ostream& o) const {
        o << opnd << "/" << opvis << "/" << rw;
    }
#endif

    xed_opvis_enum_t 
    get_opvis() const
    {
        return opvis;
    }

    bool is_implicit() const { // implicit-expressed
        return opvis == XED_OPVIS_IMPLICIT;
    }

    bool is_suppressed() const { // implicit-suppressed
        return opvis == XED_OPVIS_SUPPRESSED;
    }

    bool is_explicit() const { // explicit (normal case)
        return opvis == XED_OPVIS_EXPLICIT;
    }

    bool expressed() const { // expressed  -- explicit ops or implicit-expressed ops
        return is_explicit() || is_implicit();

    }

};

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_operand_template_t& x);
#endif

/// The first half of the "split codes" used to describe operand type. (Eb, Gv, etc.)
class xed_operand_code_first_t
{
  public:
    bool modrm;
    xed_reg_class_t reg_class;
    xed_memop_enum_t memop;
  
    xed_operand_code_first_t() // CONS
        : modrm(false),
          reg_class(XED_REG_CLASS_INVALID),
          memop(XED_MEMOP_INVALID)
    {
    }
};

/// The valid widths for each operand code
class xed_operand_code_width_t
{
  public:
    opnd_width_t width1;
    opnd_width_t width2;
    opnd_width_t width3;

    xed_operand_code_width_t() //CONS
        : width1(OPND_WIDTH_INVALID),
          width2(OPND_WIDTH_INVALID),
          width3(OPND_WIDTH_INVALID)
    {
    }
};

/// This is the base opcode class. All the other opcode classes are
/// derived from xed_opcode_base_t. It provides the common variables
/// present in all the derived opcodes. The derived opcodes often
/// add other fields like refining prefixes. These opcodes come from
/// big tables in XED.
class xed_opcode_base_t 
{
  public:
    xed_tcode_t tcode :6 ;   ///< type code for decoding
    m64_width_t m64_width :2 ; ///< Default width in 64b mode
    xed_iclass_t cls :16 ; ///< The instruction class (ADD, SUB, etc.)
    xed_operand_template_t op[XED_MAX_OPERANDS]; ///< operand templates. Before decoding.

    xedmode_enum_t mode  :6 ; ///< mode restrictions for decoding
    bool modrm :1 ;  ///< Uses modrm byte
    bool  _valid_opcode  :1 ; ///< is this a non-instruction opcode (prefix, etc.) or part of a valid instruction

    xed_category_t category :8 ; ///< Like an ALU class 
    xed_extension_t extension :8 ; ///< ISA extension (MMX,SSE,SSE2, SSE3, VTx...)
    opnd_width_t width :8 ; ///< Operation width used to override default widths
                        /// could be used for 1st opcode byte, 2nd opcode byte or 3DNow
                        /// immed opcode.
    UINT8 opcode1; 
    UINT8 _max_operand_templates;
  public:

    /// Constructor
    xed_opcode_base_t(UINT8 arg_opcode1=0)  // CONS
        : tcode(XEDTCODE_N),
          m64_width(M64_WIDTH_NRMW),
          cls(XEDICLASS_INVALID),
          mode(XEDMODE_ALL_MODES),
          modrm(false),
          _valid_opcode(false),
          category(XED_CATEGORY_INVALID),
          extension(XED_EXTENSION_INVALID),
          width(OPND_WIDTH_DEFAULT),
          opcode1(arg_opcode1)
    {
    }
    
    virtual ~xed_opcode_base_t() {
    }

    virtual xed_prefix_enum_t
    get_refining_prefix() const
    {
        return XED_PREFIX_INVALID;
    }
        
    /// @name Internal routines for XED -- ignore
    //@{

    /// Identifies which modes this opcode is valid for. Restricts the
    /// decoding to specific modes.
    inline xedmode_enum_t 
    get_mode() const { 
        return mode; 
    }


    inline bool two_byte_modrm_opcode() const { 
        return (tcode == XEDTCODE_T);
    }

    inline bool prefetch() const { 
        return (category == XED_CATEGORY_PREFETCH);
    }

    inline bool lockable_if_accesses_memory() const { 
        return (tcode == XEDTCODE_M) || (tcode == XEDTCODE_MW);
    }

    inline bool rexw_required() const { 
        return (tcode == XEDTCODE_W) || (tcode == XEDTCODE_MW);
    }
    inline bool rep_able_opcode() const {  // able to have a rep-prefix
        return (tcode == XEDTCODE_Q);
    }

    inline bool x87_escape() const { // These require a modrm
        return (tcode == XEDTCODE_F);
    }

    inline bool external_decoder_escape() const {
        return (tcode == XEDTCODE_C);
    }

    inline bool escape() const { // Two byte opcodes
        return (cls == XEDICLASS_ESCAPE);
    }
    inline bool escape_3B() const { // three byte opcodes
        return (tcode == XEDTCODE_3);
    }
    inline bool three_byte_opcode() const { // three byte opcodes
        return (tcode == XEDTCODE_Z);
    }
    inline bool valid_opcode() const {
        return _valid_opcode;
    }

    inline bool invalid() const {
        return (cls == XEDICLASS_INVALID);
    }
    inline bool group_opcode() const {
        return (tcode == XEDTCODE_G);
    }
    inline bool amd_3dnow_opcode() const {
        return (tcode == XEDTCODE_U);
    }

    /// Set the instruction ALU category
    inline void 
    set_category(xed_category_t arg_category)  {
        category = arg_category;
    }
    /// Set the instruction ISA extension
    inline void 
    set_extension(xed_extension_t arg_extension)  {
        extension = arg_extension;
    }
   
    // Set the first opcode, category, and extension
    inline void
    set_opc_cls_cat_ext(xed_iclass_t arg_cls,
                        UINT8 arg_opcode,
                        xed_category_t arg_cat,
                        xed_extension_t arg_ext,
                        xed_tcode_t arg_tcode)
    {
        opcode1 = arg_opcode;
        cls = arg_cls;
        category = arg_cat;
        extension = arg_ext;
        set_tcode(arg_tcode);
    }

    void
    set_valid_opcode() {
        switch(tcode)
        {
          case XEDTCODE_X:
          case XEDTCODE_S:
          case XEDTCODE_O:
          case XEDTCODE_A:
          case XEDTCODE_L:
          case XEDTCODE_R:
            break;
          default:
            _valid_opcode = true;
            break;
        }
    }
    
    inline void
    set_tcode(xed_tcode_t t)
    {
        tcode = t;
        set_valid_opcode();
    }
    
    /// set the instruction class and the string name.
    void 
    set( xed_iclass_t arg_cls ) 
    {
        cls = arg_cls;
    }

#if 0
    /// Get the number of template operands  -- slow -- this counts them
    inline unsigned int
    get_noperands() const {  /// VERY SLOW - returns the number of
        /// operands in the op[] array.
        int nopnd = 0; 
        for( ; !op[nopnd].invalid() && nopnd < XED_MAX_OPERANDS ; nopnd++ ) {
      
        }
        return nopnd;
    }
#endif

    /// Get the number of template operands
    inline unsigned int get_noperands() const    {
        return _max_operand_templates;
    }

    /// get a specific operand template
    inline const xed_operand_template_t& 
    get_operand_template(int indx) const
    {
        assert(indx < XED_MAX_OPERANDS);
        return op[indx];
    }


    void
    set_operand(int indx,
                const xed_operand_enum_t opnd,
                const xed_opvis_enum_t opvis = XED_OPVIS_EXPLICIT, 
                const xed_opnd_action_t rw = XED_OPND_ACTION_RW)
    {
        assert(indx < XED_MAX_OPERANDS);
        //FIXME: check if the opnd's OC1_ is an OC1_I

        if (opnd != XED_OPERAND_NA) {
            op[indx].set(opnd,opvis,rw);
        }
    }

    //@}

  
    /// @name Accessors for instruction information
    //@{
#if 0
    inline bool default_width() const {
        return (width == OPND_WIDTH_DEFAULT);
    }
#endif

    /// True if the instruction defaults to 64b width. No REX prefix
    /// required (unless register names need it).
    inline bool default_64b_width() const {
        return m64_width == M64_WIDTH_DF64;
    }

    /// The instruction name
    inline xed_iclass_t
    get_iclass() const { 
        return cls;
    }

    /// The instruction category or type, roughly based on function units used to execute the instruction
    inline xed_category_t
    get_category() const {
        return category;
    }
    /// The ISA extension that added this instruction. 
    inline xed_extension_t
    get_extension() const {
        return extension;
    }


    /// Get the opcode associated with this instruction
    inline UINT8 
    get_opcode() const {
        return opcode1;
    }

    //@}

#if XED_PRINT==1
    /// @name Printing & Debugging
    //@{
    /// print the opcode information
    void 
    print(ostream& o) const 
    {
        ostream::fmtflags oflags = o.flags();
        o << cls << " ";
        sub_print_prefix(o);
        o << hex << std::setfill('0') 
          << "0x"
          << std::setw(2);
        o << (int)opcode1
          << dec << std::setfill(' ')
          <<" ";
        sub_print(o);
        o << " " 
          << mode << " "
          << tcode << " "
          << m64_width << " "
          << opnd_print();
        o.flags(oflags);
    }

    /// Print the actual operands
    inline string 
    opnd_print() const {
        ostringstream s;
        for(int i=0;i<XED_MAX_OPERANDS;i++) {
            if (op[i].invalid()) {
                break;
            }
            s << op[i] << " ";
        }
        return s.str();
    }

    /// Virtual stub to allow the derived classed to print additional fields
    virtual void 
    sub_print(ostream& o) const {

        (void) o; // pacify compiler warnings

    }
    virtual void 
    sub_print_prefix(ostream& o) const {
        (void)o; // pacify compiler warnings
    }
    //@}
#endif
};

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_opcode_base_t& x);
#endif
/// class for opcodes that are defined by their modrm.reg field.
class group_modrm_opcode_t : public xed_opcode_base_t  // group opcode
{
  public:
    xed_iclass_t group :16 ;
    xed_prefix_enum_t prefix :XED_BIT_FIELD_PSEUDO_WIDTH8;
    UINT8 modrm_reg;

    group_modrm_opcode_t() // CONS
        : group(XEDICLASS_INVALID),
          prefix(XED_PREFIX_INVALID),
          modrm_reg(0)
    {
    }
    
    void
    set(xed_iclass_t arg_group,
        xed_prefix_enum_t arg_prefix,
        UINT8 arg_modrm_reg)
    {
        group = arg_group;
        prefix = arg_prefix;
        modrm_reg = arg_modrm_reg;
    }

    inline xed_prefix_enum_t
    get_prefix() const 
    {
        return prefix;      
    }
  
    inline UINT8
    get_modrm_reg() const {
        return modrm_reg;
    }
#if XED_PRINT==1
    virtual void 
    sub_print(ostream& o) const {
        o << group << " "
          << get_prefix() << " "
          << (int)modrm_reg;
    }
#endif
};

////////////////////////////////////////////////////////////////////////////

/// operand type-code info.  Has "first" first of type #opnd_first_code_t
/// and a "second" field of type #opnd_second_code_t. For example, for "Ev"
/// the first is OC1_E and the second is OC2_v. Many operand use these
/// split codes, however, many do not.  Ones that do not are condidered
/// "special" and require further special-case processing.  Similarly, when
/// operand is restricted to a specific register or generic-width register
/// that information is contained in the ::xedregs_t "reg" field or the
/// ::xed_genreg_t "genreg" field depending on the has_reg and has genreg
/// booleans.
class xed_operand_code_split_t
{
  public:
    opnd_first_code_t first;
    opnd_second_code_t second;
    xedregs_t reg;
    xed_genreg_t genreg;
    bool has_reg :1;
    bool has_genreg :1;
    bool special :1;
    xed_operand_code_split_t() //CONS
        : first(OC1_INVALID),
          second(OC2_INVALID),
          reg(XEDREG_INVALID),
          genreg(XED_GENREG_INVALID),
          has_reg(false),
          has_genreg(false),
          special(false)
    {
    }

    inline void
    zero() {
        first = OC1_INVALID;
        second = OC2_INVALID;
        reg=XEDREG_INVALID;
        genreg=XED_GENREG_INVALID;
        has_reg = false;
        has_genreg = false;
        special = false;
    }

    inline void 
    set_codes(  opnd_first_code_t arg_first,
                opnd_second_code_t arg_second) 
    {
        first = arg_first;
        second = arg_second;
    }

    inline void 
    set_reg(const xedregs_t r) 
    {
        reg = r;
        has_reg = true;
    }
    inline void 
    set_genreg(const xed_genreg_t r) 
    {
        genreg = r;
        has_genreg = true;
    }
#if XED_PRINT==1
    void 
    print(ostream& o) const 
    {
        o << first << "/"
          << second << " ";

        if (has_genreg) 
        {
            o << genreg << " ";
        }
      
        if (has_reg) 
        {
            o << reg << " ";
        }
        o  << (special?"special ":" ");
    }
#endif
};

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_operand_code_split_t& x);
#endif
////////////////////////////////////////////////////////////////////////////

class xed_operand_details_t
{
  public:
    xed_operand_details_t()
        : mmutable(false),
          modrm(false),
          hierarchical(false),
          split_on_mod11(false),
          opnd_class(XED_OPND_CLASS_INVALID),
          reg_class(XED_REG_CLASS_INVALID),
          sub_opnd0(XED_OPERAND_NA),
          sub_opnd1(XED_OPERAND_NA),
          width_in_16b(0),
          width_in_32b(0),
          width_in_64b(0),
          reg(XEDREG_INVALID),
          gen_reg(XED_GENREG_INVALID)
    {

    }

    void
    set(bool arg_mutable, 
        bool arg_modrm,
        xed_opnd_class_t arg_opnd_class,
        xed_reg_class_t arg_reg_class,
        UINT16 arg_width_in_16b,
        UINT16 arg_width_in_32b,
        UINT16 arg_width_in_64b )
    {
        mmutable = arg_mutable;
        modrm = arg_modrm;
        hierarchical = false;
        split_on_mod11 = false;

        opnd_class = arg_opnd_class;
        reg_class = arg_reg_class;
        width_in_64b = arg_width_in_64b;
        width_in_32b = arg_width_in_32b;
        width_in_16b = arg_width_in_16b;

        sub_opnd0 = XED_OPERAND_NA;
        sub_opnd1 = XED_OPERAND_NA;
    }

    void
    set_split(bool arg_split_on_mod11,
              xed_operand_enum_t arg_sub_opnd0,
              xed_operand_enum_t arg_sub_opnd1    )
    {
        hierarchical = true;
        split_on_mod11 = arg_split_on_mod11;
        sub_opnd0 = arg_sub_opnd0;
        sub_opnd1 = arg_sub_opnd1;
    }

    inline bool 
    get_mutable() const
    {
        return mmutable;
    }

#if XED_PRINT==1
    void
    print(ostream& o) const 
    {
        o << (mmutable ? "mutable" : "fixed") << " "
          << (modrm ? "used-modrm" : "no-modrm") <<  " "
          << (hierarchical ? "hierarchical" : "") <<  " "
          << opnd_class << " "
          << reg_class << " " 
          << (int) width_in_16b << "/"
          << (int) width_in_32b << "/"
          << (int) width_in_64b;
        if (reg != XEDREG_INVALID)
        {
            o <<  " " << reg;
        }
        else if (gen_reg != XED_GENREG_INVALID)
        {
            o << " " << gen_reg;
        }
        if (hierarchical)
        {
            o << " "
              << (split_on_mod11?"SPLIT-ON-MOD11":"")
              << " " << sub_opnd0
              << " " << sub_opnd1;
        }
    }
#endif

    bool mmutable :1 ;
    bool modrm  :1 ;
    bool hierarchical  :1 ;
    bool split_on_mod11 :1;
    xed_opnd_class_t opnd_class :8;
    xed_reg_class_t reg_class :8;
    xed_operand_enum_t sub_opnd0 : XED_BIT_FIELD_PSEUDO_WIDTH8;
    xed_operand_enum_t sub_opnd1 : XED_BIT_FIELD_PSEUDO_WIDTH8;
    UINT16 width_in_16b;
    UINT16 width_in_32b;
    UINT16 width_in_64b;
    xedregs_t reg;
    xed_genreg_t gen_reg;
};

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_operand_details_t& x);
#endif
////////////////////////////////////////////////////////////////////////////

///one-byte opcodes in the XED tables
class xed_one_byte_opcode_t : public xed_opcode_base_t
{
  public:

    xed_one_byte_opcode_t() // CONS
    {
    }


};

/// Two-byte opcodes in the XED tables.
class xed_two_byte_opcode_t : public xed_opcode_base_t
{
  public:
    UINT8 opcode2;
    xed_prefix_enum_t refining_prefix : XED_BIT_FIELD_PSEUDO_WIDTH8;
    xed_two_byte_opcode_t() //CONS
        : xed_opcode_base_t(0x0F), // initialize opcode1 to 0x0F here.
          opcode2(0),
          refining_prefix(XED_PREFIX_INVALID)
    {
    }
    inline void
    set_opcode2(UINT8 op)
    {
        opcode2 = op;
    }
    inline void
    set_prefix(xed_prefix_enum_t p)
    {
        refining_prefix = p;
    }

    inline xed_prefix_enum_t 
    get_prefix() const {
        return refining_prefix;
    }
    
    virtual xed_prefix_enum_t
    get_refining_prefix() const 
    {
        return get_prefix();
    }

    inline  UINT8 
    get_opcode2() const {
        return opcode2;
    }
#if XED_PRINT==1
    virtual void
    sub_print_prefix(ostream& o) const 
    {
        o << get_prefix();
    }

    virtual void 
    sub_print(ostream& o) const 
    {
        ostream::fmtflags oflags = o.flags();
        o  << hex 
           << std::setfill('0')
           << "0x"
           << std::setw(2) 
           << (int) opcode2;
        o.flags(oflags);
    }
#endif
};

/// Two byte-modrm opcodes in the XED tables. These are things that require
/// more refinement than the 2B-group opcodes.

class xed_two_byte_modrm_opcode_t : public xed_opcode_base_t
{
    
  public:
    //// opcode1 in base class
    UINT8 opcode2;
    xed_modrm_mod_t modrm_mod : 8;
    UINT8 modrm_reg_start;
    UINT8 modrm_reg_end;
    UINT8 modrm_rm_start;
    UINT8 modrm_rm_end;
    xed_two_byte_modrm_opcode_t() //CONS
        : opcode2(0),
          modrm_reg_start(0),
          modrm_reg_end(0),
          modrm_rm_start(0),
          modrm_rm_end(0)
    {
    }
    bool 
    match(UINT8 arg_opc1, 
          UINT8 arg_opc2,
          UINT8 arg_modrm,
          xedmode_enum_t arg_mode) const;

    inline  UINT8 
    get_opcode2() const {
        return opcode2;
    }

#if XED_PRINT==1
    virtual void 
    sub_print(ostream& o) const {
        o << "0x" << hex 
          << std::setw(2) 
          << std::setfill('0')
          << (int) opcode2 << dec
          << std::setfill(' ')
          << " " 
          << modrm_mod
          << " ["
          << (int) modrm_reg_start
          << ","
          << (int)modrm_reg_end
          << "] ["
          << (int)modrm_rm_start
          << ","
          << (int)modrm_rm_end
          << "]";
    }
#endif
};

////////////////////////////////////////////////////////////////////////////

class xed_three_byte_opcode_t : public xed_opcode_base_t
{
    //// opcode1 in base class
    UINT8 opcode2;
    UINT8 opcode3;
    xed_prefix_enum_t refining_prefix : XED_BIT_FIELD_PSEUDO_WIDTH8;
  
  public:
    xed_three_byte_opcode_t() //CONS
        : opcode2(0),
          opcode3(0),
          refining_prefix(XED_PREFIX_INVALID)
    {
    }

    inline void
    set(UINT8 op2, UINT8 op3, xed_prefix_enum_t p)
    {
        opcode2 = op2;
        opcode3 = op3;
        refining_prefix = p;
    }

    bool 
    match(xed_prefix_enum_t arg_prefix,
          UINT8 arg_opc1, 
          UINT8 arg_opc2,
          UINT8 arg_opc3,
          xedmode_enum_t arg_mode) const;

    inline xed_prefix_enum_t 
    get_prefix() const {
        return refining_prefix;
    }
    
    virtual xed_prefix_enum_t
    get_refining_prefix() const 
    {
        return get_prefix();
    }

    inline  UINT8 
    get_opcode2() const {
        return opcode2;
    }
    inline  UINT8 
    get_opcode3() const {
        return opcode3;
    }

#if XED_PRINT==1
    virtual void 
    sub_print(ostream& o) const {
        ostream::fmtflags oflags = o.flags();

        o  << hex 
           << std::setfill('0')
           << "0x"
           << std::setw(2) 
           << (int) opcode2
           << " 0x"
           << std::setw(2) 
           << (int) opcode3; 
            
        o.flags(oflags);

    }
    virtual void
    sub_print_prefix(ostream& o) const 
    {
        o << get_prefix();
    }
#endif

};
////////////////////////////////////////////////////////////////////////////

class xed_dfa_opcode_t : public xed_opcode_base_t
{
    //Not really a DFA, but a custom regexp

    xed_string_dfa_t dfa;
  public:

    const xed_string_dfa_t&
    get_dfa() const
    {
        return dfa;
    }

    inline bool
    valid() const
    {
        return dfa.valid();
    }

    xed_dfa_opcode_t() //CONS
    {
    }
    
    void
    dfa_init(const char* s)
    {
        dfa.init(s);
        // set the modrm indicator in the base class once we parse up the
        // dfa string.  The parser is currently too stupid to figure this
        // out on its own.
        if (dfa.has_modrm())
        {
            modrm = true; 
        }
    }

    inline bool 
    match(const UINT8* byte_array,
          const unsigned int length,
          const xedmode_enum_t arg_mode) const
    {
        if (mode ==  XEDMODE_ALL_MODES || arg_mode == mode)
        {
            return dfa.match(byte_array, length, arg_mode);
        }
        return false;
    }

    virtual xed_prefix_enum_t
    get_refining_prefix() const 
    {
        return dfa.get_refining_prefix();
    }

#if XED_PRINT==1
    virtual void 
    sub_print(ostream& o) const 
    {
        o  << dfa;
    }

    virtual void
    sub_print_prefix(ostream& o) const 
    {
        (void)o; // pacify compiler warnings
    }
#endif

};
////////////////////////////////////////////////////////////////////////////

/// describing SIB encodings in the XED tables.
class sib_base_table_t
{
  public:
    xed_genreg_t reg;
    xeddisplacement_t disp;
};
class sib_mem_table_t
{
  public:
    xed_genreg_t reg;
    UINT8 scale;
};

/// Describes nesting of general purpose registers.  @ingroup REGINTFC
class xed_gpr_t 
{
  public:
    unsigned int encoding;
    xedregs_t b8low; //< The low 8 bit register
    xedregs_t b8high; //< The high 8 bit register, if exists
    xedregs_t b16;  //< The  16 bit register
    xedregs_t b32;  //< The 32 bit register
    xedregs_t b64;  //< The 64 bit register
};

/// Describes the 16b addressing in the XED tables
class xed_modrm_operand_16b_t { 
  public:
    xedregs_t base_reg;
    xedregs_t index_reg;
    xeddisplacement_t disp;
    bool regtable;

    xed_modrm_operand_16b_t() //CONS
    {
        regtable = false;
        base_reg = XEDREG_INVALID;
        index_reg = XEDREG_INVALID;
        disp = XEDDISPLACEMENT_INVALID;
    }
};

/// 32b / 64b addresing info for XED tables.
class xed_modrm_operand_32_64b_t { 
  public:
    xed_genreg_t base_genreg;
    xeddisplacement_t disp;
    bool regtable :1;
    bool sib :1;

    xed_modrm_operand_32_64b_t() //CONS
        : base_genreg(XED_GENREG_INVALID),
          disp(XEDDISPLACEMENT_INVALID),
          regtable(false),
          sib(false)
    {
    }
};


class xed_modrm_rm_register_encodings_t
{
  public:
    UINT8 reg_encoding;
    xed_rexb_t rexb;
    bool is_valid :1;

    xed_modrm_rm_register_encodings_t() // CONS
        : reg_encoding(0), rexb(XED_REXB_NONE), 
          is_valid(false)
    {
    }
    inline bool
    valid() const {
        return is_valid;
    }

    inline void
    set(UINT8 arg_reg_encoding, 
        xed_rexb_t arg_rexb)
    {
        //assert(is_valid == false); // we rely on table order and
        //overwriting to get correct stuff in this table.

        reg_encoding = arg_reg_encoding;
        rexb = arg_rexb;
        is_valid = true;
    }

    inline UINT8 
    get_encoding() const 
    {
        return reg_encoding;
    }

    inline bool 
    has_rexb() const {
        return rexb != XED_REXB_NONE;
    }

    inline unsigned int
    rexb_as_int() const 
    {
        switch(rexb) {
          case XED_REXB_0:
            return 0;
          case XED_REXB_1:
            return 1;
          default:
            assert(0);
            return 0;
        }
    }  
};

////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
/// X87 FPU opcodes for the XED tables.
class x87_opcode_info_t : public xed_opcode_base_t 
{
  public:

    xed_modrm_mod_t modrm_mod : 8;
    UINT8 modrm_reg;
    UINT8 modrm_rm;

    xed_x87_cond_code_enum_t fc0 : 4;
    xed_x87_cond_code_enum_t fc1 : 4;
    xed_x87_cond_code_enum_t fc2 : 4;
    xed_x87_cond_code_enum_t fc3 : 4;

    x87_opcode_info_t() //CONS
    {
    }

    inline xed_modrm_mod_t
    get_modrm_mod() const
    {
        return modrm_mod;
    }
  
    inline UINT8 
    get_modrm_reg() const {
        return modrm_reg;
    }
  
    inline UINT8 
    get_modrm_rm() const {
        return modrm_rm;
    }

#if XED_PRINT==1
    virtual void 
    sub_print(ostream& o) const 
    {
        o <<  get_modrm_mod() << ":" << (int)modrm_reg << ":" << (int)modrm_rm;
    }
#endif
    inline void 
    set_modrm(  xed_modrm_mod_t arg_modrm_mod,
                UINT8 arg_modrm_reg,
                UINT8 arg_modrm_rm ) {
        modrm_mod = arg_modrm_mod;
        modrm_reg = arg_modrm_reg;
        modrm_rm= arg_modrm_rm;
    }

    void
    set_cond_code(xed_x87_cond_code_enum_t arg_fc0,
                  xed_x87_cond_code_enum_t arg_fc1,
                  xed_x87_cond_code_enum_t arg_fc2,
                  xed_x87_cond_code_enum_t arg_fc3)
    {
        fc0 = arg_fc0;
        fc1 = arg_fc1;
        fc2 = arg_fc2;
        fc3 = arg_fc3;
    }

    /// Return the  specified x87 condition code 0, 1, 2 or 3 operation.
    xed_x87_cond_code_enum_t
    get_cond_code(unsigned int i) const
    {
        switch(i)
        {
          case 0: return fc0;
          case 1: return fc1;
          case 2: return fc2;
          case 3: return fc3;
          default: return XED_X87_CC_INVALID;
        }
    }
        

};

////////////////////////////////////////////////////////////////////////////

class xed_regclass_info_t {
  public:
    xed_reg_class_t rclass;
    xed_regclass_info_t() //CONS
        : rclass(XED_REG_CLASS_INVALID)
    {}
  
    inline xed_reg_class_t
    get_class() const {
        return rclass;
    }
};

class xed_rexr_regclass_t 
{
    // table for figuring out how to encode a register reference, qualified by
    // a rex.r bit or a rex prefix.
  public:
    xed_reg_class_t rclass;
    UINT8 modrm_reg;
    bool ignores_rexr :1;  ///< non SR and MMX regs
    bool requires_rex :1;
    xed_rexr_t rexr;

    xed_rexr_regclass_t()  // CONS
        : rclass(XED_REG_CLASS_INVALID),
          modrm_reg(0),
          ignores_rexr(false),
          requires_rex(false),
          rexr(XED_REXR_NONE)
    {
    }

    inline bool
    invalid() const {
        return rclass == XED_REG_CLASS_INVALID;
    }
  
    inline bool
    valid() const {
        return rclass != XED_REG_CLASS_INVALID;
    }

    inline bool 
    has_rexr() const {
        return rexr != XED_REXR_NONE ;
    }

    inline unsigned int
    rexr_as_int() const {
        switch(rexr) {
          case XED_REXR_0:
            return 0;
          case XED_REXR_1:
            return 1;
          default:
            assert(0);
            return 0;
        }
    }
};

class xed_reg_to_genreg_t {
  public:
    xed_genreg_t genreg;
    xed_genreg_t alt_genreg;

    xed_reg_to_genreg_t()  //CONS
        : genreg(XED_GENREG_INVALID),
          alt_genreg(XED_GENREG_INVALID)
    {
    }
  
    inline bool 
    invalid() const 
    {
        return (genreg == XED_GENREG_INVALID);
    }
};

class xed_modrm_memory_by_genreg_t {
  public:
    bool valid[3];
    bool rexb_one; // false imlies rexb=0 or no rexb
    xeddisplacement_t disp_by_mod[3]; //index by mod
    UINT8 modrm_rm;

    xed_modrm_memory_by_genreg_t() //CONS
        :       rexb_one(false),
                modrm_rm(0)
    {
        valid[0] = valid[1] = valid[2] = false; // one valid bit per mod value
        disp_by_mod[0] = 
            disp_by_mod[1] = 
            disp_by_mod[2] = XEDDISPLACEMENT_INVALID;
    }
};

////////////////////////////////////////////////////////////////////////////
} //namespace

#endif
//Local Variables:
//pref: "../../xed-gheaders.cpp"
//End:
