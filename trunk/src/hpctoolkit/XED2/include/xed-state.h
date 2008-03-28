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
/// @file xed-state.h
/// @author  Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_STATE_H_
# define _XED_STATE_H_
#include "xed-types.h"
#include "xed-portability.h"
#include "xed-address-width-enum.h" // generated
#include "xed-machine-mode-enum.h" // generated


/// Encapsulates machine modes for decoder/encoder requests.
/// It specifies the machine operating mode as a 
/// #xed_machine_mode_enum_t 
/// for decoding and encoding. For all modes other than the 64b long mode
/// (XED_MACHINE_MODE_LONG_64), a default addressing width, and a stack
/// addressing width must be supplied of type #xed_address_width_enum_t .
/// @ingroup INIT
typedef struct XED_DLL_EXPORT xed_state_s {
  /// real architected machine modes
  xed_machine_mode_enum_t mmode; 
  
  /// the current default addressing width.
  xed_address_width_enum_t addr_width;
  
  /// for 16b/32b modes
  xed_address_width_enum_t stack_addr_width; 
} xed_state_t;

/// @name Initialization
//@{
/// Constructor.
/// The mode, and addresses widths are enumerations that specify the number
/// of bits.  In 64b mode (#XED_MACHINE_MODE_LONG_64) the address width and
/// stack address widths are 64b (#XED_ADDRESS_WIDTH_64b). In other machine
/// modes, you must specify valid addressing widths.
///
/// @param p the pointer to the #xed_state_t type
/// @param arg_mmode the machine mode of type #xed_machine_mode_enum_t
/// @param arg_addr_width the address width of type #xed_address_width_enum_t (only required if not the mode is not #XED_MACHINE_MODE_LONG_64)
/// @param arg_stack_addr_width the stack address width of type #xed_address_width_enum_t (only required if not the mode is not #XED_MACHINE_MODE_LONG_64)
/// @ingroup INIT
static XED_INLINE void xed_state_init(xed_state_t* p,
                                      xed_machine_mode_enum_t arg_mmode,
                                      xed_address_width_enum_t arg_addr_width,
                                      xed_address_width_enum_t arg_stack_addr_width) {
    p->mmode=arg_mmode;
    p->addr_width=arg_addr_width;
    p->stack_addr_width=arg_stack_addr_width;
}

/// clear the xed_state_t
/// @ingroup INIT
static XED_INLINE void xed_state_zero(xed_state_t* p) {
    p->mmode= XED_MACHINE_MODE_INVALID;
    p->addr_width=XED_ADDRESS_WIDTH_INVALID;
    p->stack_addr_width=XED_ADDRESS_WIDTH_INVALID;
}

//@}

/// @name Machine mode
//@{
/// return the machine mode
/// @ingroup INIT
static XED_INLINE xed_machine_mode_enum_t   xed_state_get_machine_mode(const xed_state_t* p) {
    return p->mmode; 
}


/// true iff the machine is in LONG_64 mode
/// @ingroup INIT
static XED_INLINE bool xed_state_long64_mode(const xed_state_t* p) { 
    return xed_state_get_machine_mode(p) == XED_MACHINE_MODE_LONG_64;
}

/// @ingroup INIT
static XED_INLINE bool xed_state_mode_width_16(const xed_state_t* p) {
    return (xed_state_get_machine_mode(p) == XED_MACHINE_MODE_LEGACY_16) ||
        (xed_state_get_machine_mode(p) == XED_MACHINE_MODE_LONG_COMPAT_16);
}

/// @ingroup INIT
static XED_INLINE bool xed_state_mode_width_32(const xed_state_t* p) {
    return (xed_state_get_machine_mode(p) == XED_MACHINE_MODE_LEGACY_32) ||
        (xed_state_get_machine_mode(p) == XED_MACHINE_MODE_LONG_COMPAT_32);
}
  

/// Set the machine mode  
/// @ingroup INIT
static XED_INLINE void  xed_state_set_machine_mode( xed_state_t* p,
                        xed_machine_mode_enum_t arg_mode)  {
    p->mmode = arg_mode;
}
//@}

/// @name Address width
//@{
/// Set the address width
/// @ingroup INIT
static XED_INLINE void xed_state_set_address_width(xed_state_t* p,
                                                   xed_address_width_enum_t arg_addr_width) {
    p->addr_width = arg_addr_width;
}

/// return the address width
/// @ingroup INIT
static XED_INLINE xed_address_width_enum_t  xed_state_get_address_width(const xed_state_t* p) {
    return p->addr_width;
}

//@}

/// @name Stack address width
//@{
/// set the STACK address width
/// @ingroup INIT
static XED_INLINE void  xed_state_set_stack_address_width(xed_state_t* p,
                              xed_address_width_enum_t arg_addr_width) {
    p->stack_addr_width = arg_addr_width;
}


/// Return the STACK address width
/// @ingroup INIT
static XED_INLINE xed_address_width_enum_t  xed_state_get_stack_address_width(const xed_state_t* p) {
    return p->stack_addr_width;
}
//@}

/// @ingroup INIT
XED_DLL_EXPORT int xed_state_print(const xed_state_t* p, char* buf, int buflen);

#endif

////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "../../xed-state.c"
//End:
