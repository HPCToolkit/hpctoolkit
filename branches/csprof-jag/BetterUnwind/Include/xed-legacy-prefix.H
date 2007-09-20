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
/// @file xed-legacy-prefix.H
/// @author Mark Charney   <mark.charney@intel.com>



#ifndef _XED_LEGACY_PREFIX_H_
# define _XED_LEGACY_PREFIX_H_

//#define LEGACY_PREFIX_ARRAY
#if defined(LEGACY_PREFIX_ARRAY)
const int MAX_LEGACY_PREFIXES = 5;
#endif
#include "xed-types.H"
#include "xed-prefix-enum.H"
#include "xed-decoded-prefix-enum.H"
#include "regs.H"

namespace XED {
using namespace std;


/// An collection of legacy prefixes with accessor predicates. The REX prefixes
/// are tracked separately.
class XED_DLL_EXPORT xed_legacy_prefix_t
{
  protected:
#if defined(LEGACY_PREFIX_ARRAY)
    UINT8 legacy_prefixes[MAX_LEGACY_PREFIXES];
    UINT8 nlegacy_prefixes;
#endif
    xed_prefix_enum_t seg_prefix : 8;

    bool has_lock : 1;

    /// F3 prefix REP or REPE (REPZ) 
    bool has_rep : 1; 

    /// F2 prefix REPNE  (REPNZ)
    bool has_repne : 1; 

    bool has_address_size_prefix :1;
    bool has_operand_size_prefix :1 ;
    bool has_segment_prefix :1 ;

    bool has_branch_taken_hint :1 ;
    bool has_branch_not_taken_hint :1 ;

    // prefixed just used for encoding
    bool has_refining_66 :1; 
    bool has_refining_f2 :1; 
    bool has_refining_f3 :1; 
  public:

    /// @name Construction and initialization
    //@{
    /// Constructor
    xed_legacy_prefix_t(); // CON

    /// Initialize the class
    void
    zero();

    /// Copy all prefixes from "other" to this one. This faithfully copies
    /// the disabled prefixes as well.
    void
    copy_legacy_prefixes(const xed_legacy_prefix_t& other);
    //@}


    /// @name Basic accessor methods
    //@{
#if defined(LEGACY_PREFIX_ARRAY)
    /// The number of prefixes
    inline unsigned int 
    nprefixes() const 
    { 
        return nlegacy_prefixes;
    }
#endif

    /// Returns the number of prefixes
    unsigned int prefixes() const;

    /// Return an array of decoded legacy prefixes; Returns number of
    /// prefixes or -1 if elements is too small.
    int get_prefix_array(xed_decoded_prefix_enum_t* array, unsigned int elements) const;

#if defined(LEGACY_PREFIX_ARRAY)
    /// Get the i'th legacy prefix. Deprecated. Do not use.
    xed_prefix_enum_t 
    get_prefix(unsigned int i) const;
#endif

    /// Add a legacy prefix
    void
    add_prefix(xed_prefix_enum_t p);

    bool 
    prefix_present(const xed_prefix_enum_t p) const;

    /// Get the segment prefix if explicitly specified as a XED::xed_prefix_enum_t
    inline xed_prefix_enum_t
    get_segment_prefix() const 
    { 
        return seg_prefix; 
    }
    
    /// Get the segment prefix if explicitly specified as a XED::xedregs_t
    xedregs_t get_segment_prefix_reg() const;

    //@}

    /// @name Prefix disables
    //@{
    /// If the operand size prefix is used for opcode selection for 2B
    /// opcodes, then the operand-size prefix loses its effects on the
    /// operand width. The external interface for the prefixes checks this
    /// boolean and thus by turning it off, we hide its effects. Note the
    /// prefix still exists in the array for linearization and printing
    /// purposes
    void 
    disable_operand_size_prefix(); 

    void 
    disable_rep_prefix();

    void 
    disable_repne_prefix();

    void
    disable_branch_taken_hint();
    
    void
    disable_branch_not_taken_hint();

    void 
    disable_segment_prefix();
    //@}

    /// @name Prefix enables
    //@{
    void
    enable_branch_taken_hint();

    void
    enable_branch_not_taken_hint();
    //@}

    /// @name Predicates on the prefixes
    //@{
    //inline bool has_legacy_prefixes() const { return nlegacy_prefixes > 0 ; }

    inline bool operand_size_prefix() const { return has_operand_size_prefix; }

    inline bool address_size_prefix() const { return has_address_size_prefix; }
    inline bool lock_prefix() const { return has_lock; }

    /// REP or REPE prefix
    inline bool rep_prefix() const { return has_rep; } 

    /// REPNE prefix
    inline bool repne_prefix() const { return has_repne; }

    inline bool segment_prefix() const { return has_segment_prefix; }

    inline bool branch_taken_hint() const { return has_branch_taken_hint; }
    inline bool branch_not_taken_hint() const { return has_branch_not_taken_hint; }

    /// The 66 prefix is not used as an operand size prefix; just to refine the opcode selection.
    inline bool refining_66_prefix() const { return has_refining_66; }
    /// The f2  prefix is not used as repne  prefix; just to refine the opcode selection.
    inline bool refining_f2_prefix() const { return has_refining_f2; }
    /// The f3 prefix is not used as a rep  prefix; just to refine the opcode selection.
    inline bool refining_f3_prefix() const { return has_refining_f3; }


    //@}

    /// @name Predicates on groupings of prefixes
    //@{
    /// rep or repne
    inline bool some_rep_prefix() const { return has_rep||has_repne; }

    /// has a repne or refining f2 prefix
    inline bool some_prefix_f2() const { return has_repne || has_refining_f2; } 

    /// has a rep, repe, refining rep prefix, or refining f3 prefix
    inline bool some_prefix_f3() const { return has_rep || has_refining_f3; }

    /// has a operand-size prefix  or a refining 66 prefix
    inline bool some_prefix_66() const { return has_operand_size_prefix || has_refining_66; }
    //@}

    /// Returns number of bytes encoded in buffer, or -1 if ran out of room
    int 
    linearize_prefixes(UINT8* buffer, unsigned int max_bytes) const;

#if XED_PRINT==1
    /// @name printing and debugging
    //@{
    void
    print_short(std::ostream& o,
                bool real_rep) const;

    void
    print(std::ostream& o, bool space=true) const; 

  private:
    void
    emit_prefix(std::ostream& o, xed_prefix_enum_t prefix, bool space) const;

    //@}
#endif
}; // end of class

} //namespace

#endif
//Local Variables:
//pref: "../../xed-legacy-prefix.cpp"
//End:
