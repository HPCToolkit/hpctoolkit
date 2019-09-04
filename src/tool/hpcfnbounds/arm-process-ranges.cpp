// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2019, Rice University
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

/******************************************************************************
 * system include files
 *****************************************************************************/


#include <stdio.h>
#include <assert.h>
#include <string>



/******************************************************************************
 * include files
 *****************************************************************************/

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"
#include "sections.h"

#include <include/hpctoolkit-config.h>



/******************************************************************************
 * macros
 *****************************************************************************/

const uint32_t ADRP_MASK   = 0x9f000000;
const uint32_t BR_MASK     = 0xfffffc1f;

const uint32_t BR_OPCODE   = 0xd61f0000;
const uint32_t ADRP_OPCODE = 0x90000000;



/******************************************************************************
 * type declarations
 *****************************************************************************/

typedef enum arm_state_e {
  ARM_STATE_DEFAULT,
  ARM_STATE_BR_SEEN
} arm_state_t;



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

#define RELOCATE(u, offset) (((char *) (u)) + (offset)) 


/******************************************************************************
 * local variables 
 *****************************************************************************/

static arm_state_t state = ARM_STATE_DEFAULT;



/******************************************************************************
 * private operations
 *****************************************************************************/

static inline bool
isInsn_BR(uint32_t insn)
{
  return ((insn & BR_MASK) == BR_OPCODE);
}


static inline bool
isInsn_ADRP(uint32_t insn)
{
  return ((insn & ADRP_MASK) == ADRP_OPCODE);
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

void
process_range_init(void)
{
}


void
process_range(const char *name, long offset, void *vstart, void *vend, DiscoverFnTy fn_discovery)
{
  if (name != SECTION_PLT || fn_discovery == DiscoverFnTy_None) {
    return;
  }
  
  uint32_t *ins = (uint32_t *) vstart;
  uint32_t *end = (uint32_t *) vend;
  
  //----------------------------------------------------------------------------
  // lightweight analysis for PLT section
  //
  // assumptions:
  // - each PLT entry ends with BR -  branch to register
  // - all but the first begin with ADRP
  // 
  // approach:
  // - treat a PLT each ADRP other than those in
  //   the first PLT stub as the beginning of a function
  //----------------------------------------------------------------------------
  for (; ins < end; ins++) {
    if (isInsn_BR(*ins)) {
      state = ARM_STATE_BR_SEEN;
    } else if (isInsn_ADRP(*ins)) {
      if (state == ARM_STATE_BR_SEEN) {
        uint32_t *ins_vaddr = (uint32_t *) RELOCATE(ins, offset);
        add_function_entry(ins_vaddr, NULL, true /* isvisible */, 0);
      }
      state = ARM_STATE_DEFAULT;
    }
  }
}



bool
range_contains_control_flow(void *vstart, void *vend)
{
  return true;
}
