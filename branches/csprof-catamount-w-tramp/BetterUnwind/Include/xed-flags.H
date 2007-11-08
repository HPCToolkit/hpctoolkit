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
/// @file xed-flags.H
/// @author Mark Charney <mark.charney@intel.com>

#ifndef _XED_FLAGS_H_
# define  _XED_FLAGS_H_

#include "xed-types.H"
#include "rflags.H"

namespace XED {
using namespace std;

////////////////////////////////////////////////////////////////////////////

/// a union of flags bits
union XED_DLL_EXPORT xed_rflag_set_t {

    /// Constructor
    xed_rflag_set_t() 
        : flat(0)
    {

    }

    struct {
        unsigned int of:1;
        unsigned int sf:1;
        unsigned int zf:1;
        unsigned int af:1;
        unsigned int pf:1;
        unsigned int cf:1;
        unsigned int df:1;
        unsigned int vif:1;
        unsigned int iopl:1;
        unsigned int _if:1;  ///< underscore to avoid token clash
        unsigned int ac:1;
        unsigned int vm:1;
        unsigned int rf:1;
        unsigned int nt:1;
        unsigned int tf:1;
        unsigned int id:1;
        unsigned int vip:1; 
    } s;
    UINT32 flat;

#if XED_PRINT==1
    void print(std::ostream& o) const;
#endif
    /// returns true if this object has a subset of the flags of the
    /// "other" object.
    bool is_subset_of(const xed_rflag_set_t& other) const;

  private:
    /// turn on a bit in the union 
    void
    add_bit(rflag_enum_t arg_flag);

    friend class xed_rflag_t; 
};
#if XED_PRINT==1
XED_DLL_EXPORT std::ostream& operator<<(std::ostream& o, const xed_rflag_set_t& x);
#endif
////////////////////////////////////////////////////////////////////////////


/// Associated with each rflag field there can be one action.
class XED_DLL_EXPORT xed_rflag_action_t 
{
    rflag_enum_t flag :8 ;
    // there are at most two actions per flag. The 2nd may be invalid.
    rflag_action_enum_t action :8 ;
  public:

    /// Constructor
    xed_rflag_action_t() 
        : flag(RFLAG_INVALID),
          action(RFLAG_ACTION_INVALID)
    {
    }

    /// @name For building the flags table -- do not use.
    //@{
    /// used for building the tables.
    void
    set(rflag_enum_t arg_flag,
        rflag_action_enum_t arg_action) 
    {
        flag = arg_flag;
        action= arg_action;

    }
    //@}

    /// @name Flag accessors
    //@{
    
    /// get the name of the flag
    inline rflag_enum_t
    get_flag_name() const {
        return flag;
    }
    
    /// return the action
    inline rflag_action_enum_t
    get_action(unsigned int i=0) const  {
        (void)i; // pacify compiler warnings
        return action;

    }

    /// returns true if the specified action is invalid. Only the 2nd flag might be invalid.
    inline bool 
    action_invalid(const rflag_action_enum_t a) const {
        return (a == RFLAG_ACTION_INVALID);
    }
#if XED_PRINT==1          
    /// print the flag & actions
    void 
    print(std::ostream& o) const {
        o << flag << "-";
        if (action != RFLAG_ACTION_INVALID) {
            o << action;
        }
    }
#endif
    /// returns true if either action is a read
    inline bool 
    read_flag() const {
        return read_action(action);
    }

    /// returns true if either action is a write
    inline bool 
    writes_flag() const {
        return write_action(action);
    }
  

    /// test to see if the specific action is a read 
    bool 
    read_action(const rflag_action_enum_t a) const {
        switch(a) {
          case RFLAG_ACTION_tst:
            return true;
          default:
            return false;
        }
    }

    /// test to see if a specific action is a write
    bool 
    write_action(const rflag_action_enum_t a) const {
        switch(a) {
          case RFLAG_ACTION_mod:
          case RFLAG_ACTION_0:
          case RFLAG_ACTION_1:    
          case RFLAG_ACTION_ah:
          case RFLAG_ACTION_pop:
          case RFLAG_ACTION_u:
            return true;
          default:
            return false;
        }
    }
    //@}
};
#if XED_PRINT==1
XED_DLL_EXPORT std::ostream& operator<<(std::ostream& o, const xed_rflag_action_t& x);
#endif
////////////////////////////////////////////////////////////////////////////

#define XED_MAX_FLAG_ACTIONS (RFLAG_LAST + 3)
/// A collection of xed_rflag_action_t's and unions of read and written flags
class XED_DLL_EXPORT xed_rflag_t 
{
    UINT8 nflags;

    bool may_write :1;
    bool must_write :1;

    /// indexed from 0, not by position in archtectural flags array.
    xed_rflag_action_t fa[XED_MAX_FLAG_ACTIONS];

    ///union of read flags
    xed_rflag_set_t read;

    /// union of written flags;
    xed_rflag_set_t written;

  public:

    /// Constructor
    xed_rflag_t()
        : nflags(0),
          may_write(false),
          must_write(false)
    {
    }

    /// @name Accessing the flags
    //@{
    /// returns the number of flag-actions
    inline unsigned int 
    get_nflags() const {
        return nflags;
    }

    /// return union of bits for read flags
    inline const xed_rflag_set_t& 
    get_read_flag_set() const {
        return read;
    }
  
    /// return union of bits for written flags
    inline const xed_rflag_set_t& 
    get_written_flag_set() const {
        return written;
    }

    inline bool 
    get_may_write() const 
    {
        return may_write;
    }

    inline bool 
    get_must_write() const 
    {
        return must_write;
    }


    /// return the specific flag-action
    inline const xed_rflag_action_t&
    get_flag_action(unsigned int i) const {
        assert(i < nflags);
        return fa[i];
    }

    /// boolean test to see if flags are read, scans the flags
    bool
    reads_rflags() const 
    {
        for(int i=0;i<nflags ;i++) 
        {
            if (fa[i].read_flag()) 
            {
                return true;
            }
        }
        return false;
    }

    /// boolean test to see if flags are written, scans the flags
    bool
    writes_rflags() const
    {
        for(int i=0;i<nflags ;i++) 
        {
            if (fa[i].writes_flag()) 
            {
                return true;
            }
        }
        return false;
    }
#if XED_PRINT==1
    /// print the flags
    void 
    print(std::ostream& o) const 
    {
        if (get_may_write())
        {
            o << "MAY-WRITE ";
        }
        if (get_must_write())
        {
            o << "MUST-WRITE ";
        }
        for(unsigned int i=0;i<get_nflags() ;i++) 
        {
            const xed_rflag_action_t& f = get_flag_action(i);
            o << f;
            if (i < (get_nflags()-1)) 
            {
                o << " ";
            }
        }
        o << "\n\tFlags read: " << read;
        o << "\n\tFlags written: " << written;
    }
    //@}
#endif

    /// @name Initializing the flags -- do not use
    //@{

#if 0
    /// set then number of flags
    inline void
    set_nflags(unsigned int i) {
        nflags = i;
    }
#endif

    /// initialize the i'th flag-action
    inline void
    set_flag_action(rflag_enum_t arg_flag,
                    rflag_action_enum_t arg_action)
    {
        assert(nflags < XED_MAX_FLAG_ACTIONS);
        fa[nflags].set(arg_flag, arg_action);
    
        // build bit vectors for read & written flags
        add_to_bit_sets(arg_flag, arg_action);
        nflags++;
    }
    //@}

    inline void
    set_may_write()
    {
        assert(must_write == false);
        may_write = true;
    }
    inline void
    set_must_write()
    {
        assert(may_write == false);
        must_write = true;
    }

  private:

    void
    add_to_bit_sets(rflag_enum_t flag, 
                    rflag_action_enum_t action) {
        if (action == RFLAG_ACTION_tst) {
            read.add_bit(flag);
        }
        else if (action != RFLAG_ACTION_INVALID) {
            written.add_bit(flag);
        }
    }    



};

#if XED_PRINT==1
XED_DLL_EXPORT std::ostream& operator<<(std::ostream& o, const xed_rflag_t& x);
#endif
////////////////////////////////////////////////////////////////////////////

// during decode: store the pointer to the lowest level xed_rflag_t in the
// xedd based on an examination of the shift-operand. Need to know what
// operand discriminates the choices. Add that to the text table.

// common the flags in the parser.


typedef enum
{
    XED_RFLAG_CASE_REGISTER, // implied may-dependence for writes
    XED_RFLAG_CASE_IMMED_ZERO,
    XED_RFLAG_CASE_IMMED_ONE,
    XED_RFLAG_CASE_IMMED_OTHER,
    XED_RFLAG_CASE_HAS_REP, // implies may-dependence for writes
    XED_RFLAG_CASE_NO_REP,
    XED_RFLAG_CASE_LAST
} xed_rflag_cases_enum_t;


class xed_complex_rflag_t
{
  public:
    // pointers to an array of dense flags. Only the cases that matter are
    // non-null.  We need to search for case of: register-counts,
    // immediate-0, immediate-1, and other-immediate

    UINT8 operand_index;
    bool check_rep :1;
    bool check_reg :1;
    bool check_imm :1;
    xed_rflag_t* cases[XED_RFLAG_CASE_LAST];

    xed_complex_rflag_t()
        : operand_index(0),
          check_rep(false),
          check_reg(false),
          check_imm(false)
    {
        for(unsigned int i=0 ;i< XED_RFLAG_CASE_LAST; i++)
        {
            cases[i]=0;
        }
    }
};

class XED_DLL_EXPORT xed_rflag_top_t
{
    // for most things:
    xed_rflag_t* simple_rflags_info;
    // for things with special cases:
    xed_complex_rflag_t* complex_rflags_info;

  public:

    xed_rflag_top_t() //CONS
        : simple_rflags_info(0),
          complex_rflags_info(0)
    {
    }

    inline bool
    simple_flags() const
    {
        return (simple_rflags_info !=0);
    }

    inline bool
    complex_flags() const
    {
        return (complex_rflags_info !=0);
    }

    inline xed_complex_rflag_t*
    get_complex_flag_info() const
    {
        return complex_rflags_info;
    }

    inline xed_rflag_t*
    get_simple_flag_info() const
    {
        return simple_rflags_info;
    }
    
    

    inline void
    set_simple(xed_rflag_t* simple_flags)
    {
        simple_rflags_info = simple_flags;
    }

    inline void
    set_complex(xed_complex_rflag_t* complex_flags)
    {
        complex_rflags_info = complex_flags;
    }
};

////////////////////////////////////////////////////////////////////////////

} // namespace
#endif
