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
/// @file xed-encode.H 
/// @author Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_ENCODE_H_
# define _XED_ENCODE_H_

#include "xed-common-defs.H"
#include "xed-gheaders.H"
#include "xed-state.H"
#include "xed-decoded-resource.H"
#include "xed-legacy-prefix.H"
#include "xed-immdis.H"
#include "xed-common-fields.H"

////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////
namespace XED {
using namespace std;

class xed_encoder_root_table_node_t; // fwd decl;


/// The class #XED::xed_encoder_request_t is the input to the encoder (See #XED::xed_encode() ). Clients
/// of the encoder must fill in the appropriate fields to encode their
/// instruction. The parent
/// class #XED::xed_state_t 
/// must be initialized
/// properly via the constructor for the encoder to function
/// properly. Also legacy prefixes must be added to the base
/// class #XED::xed_legacy_prefix_t
/// so that they are available for encoding.
///
/// The
/// class #XED::xed_encoder_request_t 
/// is just a wrapper for the
/// parent
/// class #XED::xed_common_fields_t. 
/// Look in the 
/// #XED::xed_common_fields_t 
/// for
/// all the operands, resources, etc. That class is also used by the
/// decoder
/// class #XED::xed_decoded_inst_t.
///
/// @ingroup ENC
class XED_DLL_EXPORT xed_encoder_request_t :  public xed_common_fields_t
{ 
  private:
    void clear_legacy_prefixes();

    // Internal variables for thread safety.
    bool default64b : 1;
    unsigned int expected_displacement_length : 5;
  public:

    /// @name Constructors
    //@{
    /// Constructor
    xed_encoder_request_t(const xed_state_t& arg_dstate) ;

    /// Constructor -- make sure to initialize the dstate using the parent classes set_state() call.
    xed_encoder_request_t();
      
    /// Constructor
    xed_encoder_request_t(const xed_common_fields_t& cf);
    //@}


#if XED_PRINT==1
    /// Printing 
    //@{
    void 
    print(ostream& o) const 
    {
        xed_common_fields_t::print(o);
    }
    //@}
#endif  
    friend class xed_encoder_root_table_t;

  private:
    /// Internal encoder function. Ignore. As the encoder figures out what size
    /// displacement it needs, it stashes the value here.
    inline void 
    set_expected_displacement_length(unsigned int i)
    {
        expected_displacement_length = i;
    }

    inline unsigned int
    get_expected_displacement_length() const
    {
        return expected_displacement_length;
    }

    /// Internal encoder function. Ignore. As the encoder processes the
    /// encode request, it remembers if the instruction defaults to 64b
    /// width so that it can determine later on if a REX prefix is
    /// required.
    inline void 
    set_default64b(bool v) 
    { 
        default64b = v; 
    }
    inline bool
    get_default64b() const
    {
        return default64b;
    }
};
#if XED_PRINT==1
XED_DLL_EXPORT ostream& operator<<(ostream& o, const xed_encoder_request_t& x);
#endif
////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////

/// Internal function used for installing encoder table entries. // Ignore
void 
xed_encoder_table_install(xed_opcode_base_t* opcode_base_ptr,
                          xed_iclass_t iclass,
                          xed_encoder_subtable_enum_t subtable);

#if XED_PRINT==1
/// Internal function used for debugging. Ignore
XED_DLL_EXPORT void 
xed_encoder_dump(ostream& o);
#endif


///   This is the main interface to the encoder. The array should be
///   at most 15 bytes long. The ilen parameter should indiciate
///   this length. If the array is too short, the encoder may fail to
///   encode the request.  Failure is indicated by a return value of
///   false. Otherwise, true is returned and the length of the encoded
///   instruction is returned in olen.
/// @param r encoder request description (#XED::xed_encoder_request_t), includes mode info
/// @param array the encoded instruction bytes are stored here
/// @param ilen the input length of array.
/// @param olen the actual  length of array used for encoding
/// @return success/failure
/// @ingroup ENC
XED_DLL_EXPORT bool
xed_encode(xed_encoder_request_t& r,
           UINT8* array, 
           const unsigned int ilen,
           unsigned int& olen);

/// This function will attempt to encode a NOP of exactly ilen
/// bytes. If such a NOP is not encodeable, then false will be returned.
///
/// @param array the encoded instruction bytes are stored here
/// @param  ilen the input length array.
///  @return success/failure
/// @ingroup ENC
XED_DLL_EXPORT bool
xed_encode_nop(UINT8* array, 
               const unsigned int ilen);
////////////////////////////////////////////////////////////////////////////






} //namespace

////////////////////////////////////////////////////////////////////////////
#endif
//Local Variables:
//pref: "../../xed-encode.cpp"
//End:
