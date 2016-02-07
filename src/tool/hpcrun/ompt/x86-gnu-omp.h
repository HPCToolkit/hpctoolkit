// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2014, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//******************************************************************************
// local includes
//******************************************************************************

#include <unwind/x86-family/x86-decoder.h>



//******************************************************************************
// macros
//******************************************************************************

#define X86_CALL_NBYTES 5 // the number of bytes in an x86 call instruction



//******************************************************************************
// operations
//******************************************************************************

static inline uint64_t 
length_of_call_instruction()
{
  return X86_CALL_NBYTES; 
} 


// given an instruction pointer, find the next call instruction. return the 
// difference in bytes between the end of that call instruction and the 
// initial instruction pointer
static inline int 
offset_to_pc_after_next_call(void *ip)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  uint8_t *ins = ip; 

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);

  // continue until success or failure
  for (;;) {

    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, ins, 15);

    if (xed_error != XED_ERROR_NONE) {
        // on a failure, perform no adjustment.
        return 0;
    }

    // advance ins to point at the next instruction
    ins += xed_decoded_inst_get_length(xptr);

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

    switch(xiclass) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
       return ins - (uint8_t *) ip;
    default:
       break;
    }
  }

}
