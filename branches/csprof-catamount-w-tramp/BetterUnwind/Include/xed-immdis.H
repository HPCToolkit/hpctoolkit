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
/// @file xed-immdis.H 
/// @author  Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_IMMDIS_H_
# define _XED_IMMDIS_H_

#include "xed-types.H"
#include "xed-common-defs.H"
//#include "xed-util.H"


////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////
namespace XED {
using namespace std;

#define MAX_IMMDIS_BYTES 8

// A union for speed of zeroing
union xed_immdis_values_t
{
    UINT8 x[MAX_IMMDIS_BYTES];// STORED LITTLE ENDIAN. BYTE 0 is LSB
    UINT64 q;
};

/// Stores immediates and displacements for the encoder & decoder.
class XED_DLL_EXPORT  xed_immdis_t
{
    xed_immdis_values_t value;
    unsigned int currently_used_space :4; // current number of assigned bytes
    unsigned int max_allocated_space :4; // max allocation, 4 or 8

    // this is used to allow quick updates of the immediates and displacements
    unsigned int start_byte : 4; // byte location where we begin linearization for this immdis.

    bool present : 1;
    bool immediate_is_unsigned : 1;

    void check(int p)
    {
        assert(currently_used_space == 0);
        present = true;
        currently_used_space  = p;
        assert(currently_used_space <= max_allocated_space);
    }

  public:
    xed_immdis_t(int max_bytes) // CONS
        : currently_used_space(0),
          max_allocated_space(max_bytes),
          present(false),
          immediate_is_unsigned(false) 
    {
        assert(max_bytes == 4 || max_bytes == 8);
        value.q = 0;
    }

    /// @name Sizes and lengths
    //@{
    /// return the number of bytes added
    inline unsigned int 
    get_bytes() const {
        return currently_used_space;
    }


    //@}

    /// @name Accessors for the value of the immediate or displacement
    //@{
    INT64 
    get_signed64() const;

    UINT64 
    get_unsigned64() const;

    inline bool
    is_zero() const
    {
        //FIXME: could just check value.q if willing to rely on initialization
        for(unsigned int i=0; i < currently_used_space; i++)
        {
            if (value.x[i] != 0)
            {
                return false; // not zero
            }
        }
        return true;// is zero
    }

    inline bool
    is_one() const
    {
        if (value.x[0] == 1)
        {
            for(unsigned int i=1; i < currently_used_space; i++)
            {
                if (value.x[i] != 0)
                {
                    return false; // not one
                }
            }
            return true; // a 1 and the rest, if any, are zeros
        }
        return false; // not one
    }

    /// Access the i'th byte of the immediate
    int 
    get_byte(unsigned int i) const {
        assert(i < currently_used_space);
        return value.x[i];
    }
    //@}

    /// @name Presence / absence of an immediate or displacement
    //@{
    void
    set_present() 
    {
        present = true;
    }
    /// True if the object has had a value or individual bytes added to it.
    inline bool
    is_present() const {
        return present;
    }
    //@}


    /// @name Initialization and setup
    //@{
    void 
    set_max_len(unsigned int mx)
    {
        assert(mx <= MAX_IMMDIS_BYTES);
        max_allocated_space = mx;
    }
    void
    zero()
    {
        present = false;
        immediate_is_unsigned = false;
        currently_used_space = 0;
#if defined(XED_SQUEAKY_CLEAN)
        value.q = 0;
#endif
    }

    UINT 
    get_max_length() const {
        return max_allocated_space;
    }

    //@}

    /// @name Signed vs Unsigned
    //@{ 
    /// Return true if  signed.
    inline bool
    is_unsigned() const {
        return immediate_is_unsigned;
    }
    /// Return true if signed.
    inline bool
    is_signed() const {
        return !immediate_is_unsigned;
    }
    
    /// Set the immediate to be signed; For decoder use only.
    inline void 
    set_signed() {
        immediate_is_unsigned = false;
    }
    /// Set the immediate to be unsigned; For decoder use only.
    inline void 
    set_unsigned() {
        immediate_is_unsigned = true;
    }
    //@}


    /// @name Adding / setting values
    //@{
    void
    add_byte(UINT8 b);


    void
    add_byte_array(int nb, UINT8* ba);

    /// Add 1, 2, 4 or 8 bytes depending on the value x and the mask of
    /// legal_widths. The default value of legal_widths = 0x5 only stops
    /// adding bytes only on 1 or 4 byte quantities - depending on which
    /// bytes of x are zero -- as is used for most memory addressing.  You
    /// can set legal_widths to 0x7 for branches (1, 2 or 4 byte branch
    /// displacements). Or if you have an 8B displacement, you can set
    /// legal_widths to 0x8. NOTE: add_shortest_width will add up to
    /// MAX_IMMDIS_BYTES if the x value requires it. NOTE: 16b memory
    /// addressing can have 16b immediates.
    void
    add_shortest_width_signed(INT64 x, UINT8 legal_widths = 0x5);

    /// See add_shortest_width_signed()
    void
    add_shortest_width_unsigned(UINT64 x, UINT8 legal_widths = 0x5);


    /// add an 8 bit value to the byte array
    void
    add8(INT8 d)
    {
        check(1);
        value.x[0] = d;
    }

    /// add a 16 bit value to the byte array
    void
    add16(INT16 d)
    {
        check(2);
        value.x[0] = XED_BYTE_MASK(d);
        value.x[1] = XED_BYTE_MASK(d>>8);
    }

    /// add a 32 bit value to the byte array
    void
    add32(INT32 d)
    {
        check(4);
        value.x[0] = XED_BYTE_MASK(d);
        value.x[1] = XED_BYTE_MASK(d>>8);
        value.x[2] = XED_BYTE_MASK(d>>16);
        value.x[3] = XED_BYTE_MASK(d>>24);
    }

    /// add a 64 bit value to the byte array.
    void
    add64(INT64 d)
    {
        check(8);
        for(int i = 0; i < 8 ;i++) {
            value.x[i] = XED_BYTE_MASK( d>>(8*i) );
        }
    }

    //@}

    /// Internal function; Ignore. This is set during encoding and is used
    /// to allow quick updates to the immdis.
    void 
    set_start_offset(unsigned int bytes)
    {
        start_byte = bytes;
    }

    /// Internal function; Ignore. This is set during encoding imm/disp updates.
    unsigned int
    get_start_offset() const
    {
        return start_byte;
    }
    
    /// Update the  value of an already encoded instruction at the
    /// given location without re-encoding.
    ///
    /// @param new_value the new  value to insert in to the itext
    /// @param nbytes the length of the new value
    /// @param itext the start of the encoded instruction text in memory. Must be writable.
    ///
    /// @return true iff the nbytes size matches the existing value
    ///         size. if false, the instruction is not updated and a re-encode is
    ///         required.
    bool
    update_encoding(UINT64 new_value, 
                    unsigned int nbytes,
                    UINT8* itext);


#if XED_PRINT==1
    /// @name printing / debugging
    //@{

    /// just print the raw bytes in hex with a leading 0x
    void
    print(std::ostream& o) const;

    /// Print the value as a signed or unsigned number depending on the
    /// value of the immediate_is_unsigned variable.
    void
    print_signed_or_unsigned(std::ostream& o) const;

    /// print the signed value, appropriate width, with a leading 0x
    void
    print_value_signed(std::ostream& o) const;

    /// print the unsigned value, appropriate width, with a leading 0x
    void
    print_value_unsigned(std::ostream& o) const;

  private:
    void
    print_ptr(std::ostream& o) const;
#endif

  public:

    //@}
};

#if XED_PRINT==1
XED_DLL_EXPORT std::ostream& operator<<(std::ostream& o, const xed_immdis_t& x);
#endif
} //namespace

#endif
////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-immdis.cpp"
//End:
