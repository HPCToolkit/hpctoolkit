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
/// @file xed-state.H
/// @author  Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_STATE_H_
# define _XED_STATE_H_

namespace XED {
  using namespace std;

////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////////////////////


/// The parent class for the decoder and encoder requests that encapsulates machine modes.
/// It specifies the machine operating mode as a 
/// #xed_machine_mode_t 
/// for decoding
/// and encoding. For all modes other than the 64b long mode (XED_MACHINE_MODE_LONG_64), a
/// default addressing width, and a stack addressing width must be
/// supplied of type
/// #addr_width_t .
/// @ingroup INIT
class XED_DLL_EXPORT xed_state_t
{
public:

  /// Constructor
  xed_state_t(xed_machine_mode_t arg_mmode = XED_MACHINE_MODE_INVALID,
	      addr_width_t arg_addr_width = ADDR_WIDTH_INVALID,
	      addr_width_t arg_stack_addr_width = ADDR_WIDTH_INVALID) 
    :  mmode(arg_mmode),
       addr_width(arg_addr_width),
       stack_addr_width(arg_stack_addr_width)
  {
  }

  /// Another way to set the state, if you have a pre-existing object.
  void
  set_state(const xed_state_t& x) 
  {
    mmode = x.mmode;
    addr_width = x.addr_width;
    stack_addr_width = x.stack_addr_width;
  }

  void
  set_state(xed_machine_mode_t arg_mmode = XED_MACHINE_MODE_INVALID,
            addr_width_t arg_addr_width = ADDR_WIDTH_INVALID,
            addr_width_t arg_stack_addr_width = ADDR_WIDTH_INVALID) 
  {
    mmode = arg_mmode;
    addr_width = arg_addr_width;
    stack_addr_width = arg_stack_addr_width;
  }

  /// clear the xed_state_t
  void 
  zero()
  {
    mmode = XED_MACHINE_MODE_INVALID;
    addr_width = ADDR_WIDTH_INVALID;
    stack_addr_width = ADDR_WIDTH_INVALID;
  }

  /// return the machine mode
  inline xed_machine_mode_t 
  get_machine_mode() const {
    return mmode; 
  }
  
  /// true iff the machine is in LONG_64 mode
  inline bool 
  long64_mode() const { 
    return get_machine_mode() == XED_MACHINE_MODE_LONG_64;
  }

  inline bool
  mode_width_16() const {
    return (get_machine_mode() == XED_MACHINE_MODE_LEGACY_16) ||
      (get_machine_mode() == XED_MACHINE_MODE_LONG_COMPAT_16);
  }

  inline bool
  mode_width_32() const {
    return (get_machine_mode() == XED_MACHINE_MODE_LEGACY_32) ||
      (get_machine_mode() == XED_MACHINE_MODE_LONG_COMPAT_32);
  }
  
  inline void
  set_machine_mode(const xed_machine_mode_t& arg_mode)  {
    mmode = arg_mode;
  }

  inline void
  set_address_width(const addr_width_t arg_addr_width) {
    addr_width = arg_addr_width;
  }

  /// return the address width
  inline addr_width_t
  get_address_width() const {
    return addr_width;
  }

  /// return the stack address width
  inline void
  set_stack_address_width(const addr_width_t arg_addr_width) {
    stack_addr_width = arg_addr_width;
  }

  inline addr_width_t
  get_stack_address_width() const {
    return stack_addr_width;
  }

#if XED_PRINT==1
  void
  print(ostream& o) const 
  {
    o << "MachineMode: " 
      << mmode
      << " AddrWidth: "
      << addr_width 
      << " StackAddrWidth: "
      << stack_addr_width;
  }
#endif  
  /// real architected machine modes
  xed_machine_mode_t mmode :8; 
  
  /// the current default addressing width.
  addr_width_t addr_width :8;
  
  /// for 16b/32b modes
  addr_width_t stack_addr_width :8; 
};

////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////


} //namespace


#endif

////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-state.cpp"
//End:
