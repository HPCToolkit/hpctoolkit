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
// Copyright ((c)) 2002-2016, Rice University
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

/* the xed include files used in the x86 family of functions define a lot of static functions. 
 * to avoid replicated copies, we should include xed only once. since each of the x86 family 
 * of files includes xed, we do the next best thing - we compile all of our xed-based source 
 * files into a single object file.
 */

#include <stdbool.h>


#include "x86-addsub.c"
#include "x86-and.c"
#include "x86-build-intervals.c"
#include "x86-call.c"
#include "x86-canonical.c"
#include "x86-debug.c"
#include "x86-decoder.c"
#include "x86-enter.c"
#include "x86-jump.c"
#include "x86-lea.c"
#include "x86-leave.c"
#include "x86-move.c"
#include "x86-process-inst.c"
#include "x86-push.c"
#include "x86-return.c"

