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
// Copyright ((c)) 2002-2020, Rice University
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

//*****************************************************************************
// global includes 
//*****************************************************************************

#include <hpctoolkit-config.h>



//*****************************************************************************
// macros 
//*****************************************************************************

// When a kernel sample is attached below a CCT node, we don't want to
// turn a CCT leaf into an interior node because, at present, interior
// nodes don't't have any associated persistent ID preserved; such IDs
// are needed when a node appears in a trace record.  For that reason,
// a kernel sample below a leaf at PC creates a sibling to the leaf
// with PC - 1. If PC is the first PC in a routine, the node with
// address PC - 1 might be attributed to the preceding
// routine.
//
// To avoid such a misattribution with a placeholder, rather
// than representing the placeholder with the first address of a
// routine, use the routine address + 2 so that if a kernel sample is
// hung below the placeholder, the interior node will still map to the
// right placeholder.
#define ADJUST_PLACEHOLDER_PC(x) (((char *)x) + 1)



//*****************************************************************************
// interface operations 
//*****************************************************************************

void *
canonicalize_placeholder(void *routine)
{
#if defined(HOST_BIG_ENDIAN) && defined(HOST_CPU_PPC)
    // with the PPC 64-bit ABI on big-endian systems, functions are
    // represented by D symbols and require one level of indirection
    void *routine_addr = *(void**) routine;
#else
    void *routine_addr = (void *) routine;
#endif
    return ADJUST_PLACEHOLDER_PC(routine_addr);
}

