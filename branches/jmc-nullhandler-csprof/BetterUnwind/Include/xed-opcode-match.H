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
/// @file xed-opcode-match.H
/// @author Mark Charney   <mark.charney@intel.com>



#ifndef _XED_OPCODE_MATCH_H_
# define _XED_OPCODE_MATCH_H_

#include "xed-types.H"
#include "xed-common-hdrs.H"
#include "xed-prefix-enum.H"
#include "xed-modes.H"
using namespace std;

namespace XED 
{

////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////
class xed_string_dfa_t;
typedef void (xed_string_dfa_t::*AdderFunc)(UINT8 mark, UINT8 mask);

#define XED_MAX_DFA_BYTES  6
class xed_string_dfa_t
{  
    //Not really a DFA, but a custom regexp

  public:
    xed_string_dfa_t()
        : length(0),
          legacy_prefix_start(-1),
          rex_prefix_start(-1),
          opcode_start(-1),
          modrm_start(-1),
          refining_prefix(XED_PREFIX_LAST)
    {
    }

    
    /// Initialize from a string in our ASCII syntax
    bool
    init(const char* s);
#if XED_PRINT==1
    void
    print(ostream& o) const;
#endif
    bool
    match(const UINT8* byte_array, 
          const unsigned int len,
          const xedmode_enum_t mode) const;
    
    unsigned int 
    get_nopcodes() const;

    UINT8
    get_opcode(unsigned int i) const;

    const UINT8*
    get_legacy_prefixes(unsigned int& len) const;
    
    inline xed_prefix_enum_t
    get_refining_prefix() const
    {
        return refining_prefix;
    }

    bool
    get_rex_prefix(UINT8& rex) const;

    bool
    get_modrm_byte(UINT8& modrm) const;

    inline bool
    valid() const
    {
        return (opcode_start != -1);
    }

    inline bool
    has_modrm() const
    {
        return modrm_start != -1;
    }


  private:
    void
    zero();

    void 
    add_legacy_prefix(UINT8 prefix, UINT8 mask=0xff);

    void 
    add_rex_prefix(UINT8 rex, UINT8 mask=0xff);
    
    void
    add_opcode(UINT8 opcode, UINT8 mask=0xff);

    void
    add_modrm(UINT8 modrm, UINT8 mask=0xff);
        
    void
    add_byte(UINT8 b, UINT8 mask);

    inline bool
    has_legacy_prefixes() const 
    {
        return legacy_prefix_start != -1;
    }

    inline bool
    has_rex() const 
    {
        return rex_prefix_start != -1;
    }
    
    inline bool
    rexlike(UINT8 b) const
    {
        if ((b & 0xF0) == 0x40)
            return true;
        return false;
    }

    inline bool
    expecting_rex(unsigned int pos) const 
    {
        return  (has_rex() && static_cast<int>(pos) == rex_prefix_start);
    }
    inline bool
    expecting_opcode(unsigned int pos) const 
    {
        return  (static_cast<int>(pos) == opcode_start);
    }
    
    inline bool
    expecting_modrm(unsigned int pos) const
    {
        return has_modrm() && (static_cast<int>(pos) == modrm_start);
    }

    inline bool
    expecting_legacy_prefixes(unsigned int pos) const 
    {
        return  has_legacy_prefixes() && 
            static_cast<int>(pos) < opcode_start &&
            (!has_rex() || !expecting_rex(pos));
    }


    bool 
    init_generic(const char* s,  
                 const unsigned int slen,
                 AdderFunc adder);

    xed_prefix_enum_t 
    legacy_prefix(UINT8 x) const;
        
    bool
    compute_mask(const char* s, 
                 UINT8& mark,
                 UINT8& mask) const;
    bool
    match_pos(const UINT8 b, 
              const unsigned int pos) const;
        
    inline void 
    check_length()
    {
        assert(length < XED_MAX_DFA_BYTES);
    }

    // for each byte, I have a mask that indicates which bits we should really examine.
    UINT8 opcode_bytes[XED_MAX_DFA_BYTES];
    UINT8 opcode_byte_mask[XED_MAX_DFA_BYTES];
    UINT8 length;
    INT8  legacy_prefix_start;
    INT8  rex_prefix_start;
    INT8  opcode_start;
    INT8  modrm_start;
    xed_prefix_enum_t refining_prefix;
};
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_string_dfa_t& x);
#endif
////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////


} // namespace
#endif




//Local Variables:
//pref: "../../xed-opcode-match.cpp"
//End:
