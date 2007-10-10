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
/// @file xed-raw.H 
/// @author  Mark Charney   <mark.charney@intel.com> 


////////////////////////////////////////////////////////////////////////////

#ifndef _XED_FORMAT_H_
# define _XED_FORMAT_H_

#include "xed-types.H"
//#include "xed-util.H"
#include "xed-prefix-enum.H"
#include "xed-common-defs.H"
#include "xed-legacy-prefix.H"
#include "xed-immdis.H"

namespace XED {
  using namespace std;

const UINT8 DEFAULT_REX_BYTE = 0x40;

/// Raw REX byte bit-field accessor
union XED_DLL_EXPORT rex_prefix_t
{
  rex_prefix_t( UINT8 arg_default = DEFAULT_REX_BYTE ) // CONS
    : b( arg_default )
  {
  }

  struct {
    UINT8 rexb : 1;
    UINT8 rexx : 1;
    UINT8 rexr : 1;
    UINT8 rexw : 1;
    UINT8 reserved : 4; // MUST BE 0x4
  } s;
  UINT8 b;

  inline UINT8 get_rexb() const {
    return s.rexb;
  }
  inline UINT8 get_rexx() const {
    return s.rexx;
  }
  inline UINT8 get_rexr() const {
    return s.rexr;
  }
  inline UINT8 get_rexw() const {
    return s.rexw;
  }

  inline void set_rex(unsigned int x)  {
      b = static_cast<UINT8>(x);
  }

  inline void set_rexb(unsigned int x)  {
    s.rexb = x;
  }
  inline void set_rexx(unsigned int x)  {
    s.rexx = x;
  }
  inline void set_rexr(unsigned int x)  {
    s.rexr = x;
  }
  inline void set_rexw(unsigned int x)  {
    s.rexw = x;
  }
  
  /// Returns true if any of the W/R/B/X bits are set.
  inline bool has_rex_info() const {
    return (b!=DEFAULT_REX_BYTE);
  }
#if XED_PRINT==1
  void print(ostream& o) const {
    o << "REX:"
      << (s.rexw ? "W" : "-")
      << (s.rexr ? "R" : "-")
      << (s.rexx ? "X" : "-")
      << (s.rexb ? "B" : "-");
  }
#endif
}; 
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const rex_prefix_t& rex);
#endif

/// Raw MODRM byte bit-field accessor.
union XED_DLL_EXPORT modrm_byte_t 
{
    struct {
        UINT8 rm : 3;
        UINT8 reg : 3;
        UINT8  mod : 2;
    } s; 
    UINT8 b;

    inline unsigned int get_byte() const {
        return (unsigned int) b;
    }

    inline unsigned int get_mod() const {
        return (unsigned int) s.mod;
    }
    inline unsigned int get_reg() const {
        return (unsigned int) s.reg;
    }
    inline unsigned int get_rm() const {
        return (unsigned int) s.rm;
    }

    inline void set_mod(UINT8 mod) {
        s.mod = mod;
    }
    inline void set_reg(UINT8 reg) {
        s.reg = reg;
    }
    inline void set_rm(UINT8 rm) {
        s.rm = rm;
    }
  
#if XED_PRINT==1
    void print(ostream& o) const {
        o << "mod: " 
          << (int) s.mod
          << " reg: " 
          << (int) s.reg 
          << " rm: " 
          << (int) s.rm;
    }
#endif
    void
    set(UINT8 x)
    {
        b = x;
    }

    modrm_byte_t(UINT8 x=0) //CONS
    {
        b = x;
    }
    
};
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const modrm_byte_t& modrm);
#endif

/// Raw SIB byte bit-field accessor.
union XED_DLL_EXPORT sib_byte_t
{
  struct {
    UINT8 base : 3;
    UINT8 index : 3;
    UINT8 scale: 2;
  } s; 
  UINT8 b;
  
  sib_byte_t(UINT8 x=0) //CONS
  {
    b = x;
  }

  inline int get_scale() const { return s.scale; }
  inline int get_index() const { return s.index; }
  inline int get_base() const { return s.base; }

  inline void set_scale(unsigned int x)  { s.scale = x; }
  inline void set_index(unsigned int x)  { s.index = x; }
  inline void set_base(unsigned int x)   { s.base = x; }

#if XED_PRINT==1
  void print(ostream& o) const {
    o << "base: " 
      << (int) s.base
      << " index: " 
      << (int) s.index
      << " scale: " 
      << (int) s.scale;
  }
#endif
};
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const sib_byte_t& sib);
#endif
/// This class can hold all the details of an xed-64 instruction
/// in a decoded format. It can also "linearize" or pack those
/// pieces into a <= 15 byte array. It knows nothing about machine
/// modes. It is used by the encoder to create an instruction. It 
/// is used by the decoder to store the information from the linear
/// instruction text that was decoded.
class XED_DLL_EXPORT xed_raw_inst_t : public xed_legacy_prefix_t
{

 
  public:
    xed_raw_inst_t() //CONS
        : immediate(XED_MAX_IMMEDIATE_BYTES),
          length(0),
          nopcodes(0),
          has_rex(false),
          has_modrm(false),
          has_sib(false),
          has_immediate2(false),
          immediate2(0),
          displacement(XED_MAX_DISPLACEMENT_BYTES)


    {
    }

    void
    zero() 
    {
        xed_legacy_prefix_t::zero();
        displacement.zero();
        immediate.zero();
        immediate2 =0;
        length=0;
        has_rex=false;
        nopcodes=0;
        has_modrm=false;
        has_sib=false;
        has_immediate2 = false;
    }
  

    /// Fill in the array with 1-15 bytes of instruction info from the
    /// various fields in the class. The output length is in olen. To be
    /// safe, the input length should be at least 15 bytes or the
    /// linearization could fail. This stores the locations of the
    /// immediate and displacement so we can update them later.
    bool 
    linearize(UINT8* array,
              const unsigned int ilen,
              unsigned int& olen);
#if XED_PRINT==1
    void 
    print(ostream& o) const;

    void
    dump(ostream& o) const;
#endif


    /// @name Legacy prefixes - see parent class
    //@{
    void
    add_prefix(xed_prefix_enum_t p)
    {
        bump_len(1);
        xed_legacy_prefix_t::add_prefix(p);
    }
    //@}


    /// @name MODRM bytes
    //@{
    inline modrm_byte_t& 
    get_modrm_nonconst() {
        return modrm;
    }
  
    inline const modrm_byte_t& 
    get_modrm() const  {
        assert(has_modrm);
        return modrm;
    }
    void
    add_modrm(modrm_byte_t arg_modrm)
    {
        set_has_modrm();
        modrm = arg_modrm;
    }
    
    bool
    get_has_modrm() const 
    {
        return has_modrm;
    }

    inline void
    set_has_modrm()
    {
        if (has_modrm == false)
        {
            bump_len(1);
            has_modrm = true;
        }
    }
    //@}

    /// @name SIB bytes
    //@{
    bool
    get_has_sib() const 
    {
        return has_sib;
    }

    inline void
    set_has_sib() {
        has_sib = true;
    }

    void
    add_sib(sib_byte_t arg_sib)
    {
        assert(has_sib == false);
        has_sib = true;
        sib = arg_sib;
        bump_len(1);
    }

    inline const sib_byte_t& 
    get_sib() const
    {
        assert(has_sib);
        return sib;
    }
    inline sib_byte_t& 
    get_sib_nonconst()   {
        return sib;
    }
    //@}





    /// @name Accessing Immediates
    //@{ 
    inline UINT8
    get_immediate_byte(unsigned int indx) const {
        return static_cast<UINT8>(immediate.get_byte(indx));
    }
  
    inline const xed_immdis_t& 
    get_immediate() const {
        return immediate;
    }
    inline void
    set_immediate_unsigned()
    {
        immediate.set_unsigned();
    }

    inline void
    set_immediate_signed()
    {
        immediate.set_signed();
    }


    void
    set_has_immediate() {
        immediate.set_present();
    }

    bool
    get_has_immediate() const 
    {
        return immediate.is_present();
    }


    void
    add_immediate_byte(UINT8 b)
    {
        // add them byte by byte
        immediate.add_byte( b );
        bump_len(1);
    }

    /// Internal function; ignore. For remembering where the displacement starts
    void
    set_immediate_offset(unsigned int o)
    {
        immediate.set_start_offset(o);
    }
    //@}

    /// @name Accessing Displacements
    //@{
    inline const xed_immdis_t& 
    get_displacement() const {
        return displacement;
    }
    inline void
    set_has_displacement() {
        displacement.set_present();
    }

    bool
    get_has_displacement() const 
    {
        return displacement.is_present();
    }

    UINT 
    get_displacement_max_length() const 
    {
        return displacement.get_max_length();
    }

    void
    add_displacement_byte(UINT8 b)
    {
        displacement.add_byte(b);
        bump_len(1);
    }

    /// Internal function; ignore. For remembering where the displacement starts
    void
    set_displacement_offset(unsigned int o)
    {
        displacement.set_start_offset(o);
    }
    //@}

    inline unsigned int 
    get_length() const {
        return length;
    }

    /// @name Opcode information
    //@{
    inline unsigned int
    get_nopcodes() const {
        return nopcodes;
    }
    inline UINT8 get_opcode(int i=0) const {
        assert(i < XED_MAX_OPCODE_BYTES);
        return opcodes[i];
    }
    void 
    add_opcode(UINT8 op)
    {
        bump_len(1);
        assert(nopcodes < XED_MAX_OPCODE_BYTES);
        opcodes[nopcodes++] = op;
    }

    //@}

    /// @name REX prefix info
    //@{
    inline bool 
    rex() const {
        return has_rex;
    }


    inline const rex_prefix_t& 
    get_rex() const {
        assert(has_rex);
        return rex_prefix;
    }

    inline rex_prefix_t& 
    get_rex_nonconst()  {
        return rex_prefix;
    }
    inline void set_has_rex() {
        has_rex = true;
    }

    void
    add_rex(rex_prefix_t r)
    {
        assert(has_rex == false);
        update_rex(r);
    }
    void
    update_rex(rex_prefix_t r)
    {
        if (has_rex == false)
        {
            bump_len(1);
            has_rex = true;
        }
        rex_prefix = r;
    }
    
    inline void set_rex_exact(UINT8 new_rex) 
    {
        rex_prefix_t r;
        r.b = new_rex;
        update_rex(r);
    }
    //@}

    /// @name the 2nd immediate 
    //@{
    // add a byte to the 2nd immediate
    void
    add_immediate2_byte(UINT8 b)
    {
        assert(has_immediate2 == false);
        has_immediate2 = true;
        immediate2 = b;
        bump_len(1);
    }

    inline UINT8
    get_immediate2_byte(unsigned int indx) const {
        assert(has_immediate2);
        assert(indx == 0);
        return immediate2;
    }

    bool
    get_has_immediate2() const 
    {
        return has_immediate2;
    }

    //@}

  private:
    inline void 
    check_len(UINT8 count) const
    {
        assert(count <= XED_MAX_INSTRUCTION_BYTES);
    }

    inline void 
    bump_len(unsigned int i)  {
        length = length + static_cast<UINT8>(i);
        check_len(length);
    }
    // metadata describing what's in the data fields


    xed_immdis_t immediate;

    UINT8 length;
    UINT8 nopcodes:2;
    bool has_rex :1 ;
    bool has_modrm :1;
    bool has_sib :1;
    bool has_immediate2 :1;

    // raw data
    // xed_legacy_prefix_t is in base class
    rex_prefix_t rex_prefix;
    UINT8 immediate2;

    xed_immdis_t displacement;

    modrm_byte_t modrm;
    sib_byte_t sib;
    UINT8 opcodes[XED_MAX_OPCODE_BYTES];
} ; // __attribute__((packed));

#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_raw_inst_t& x);
#endif

////////////////////////////////////////////////////////////////////////////
} //namespace

#endif

//Local Variables:
//pref: "../../xed-raw.cpp"
//End:
