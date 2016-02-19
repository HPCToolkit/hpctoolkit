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

#ifndef unwind_cfg_h
#define unwind_cfg_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#define HPC_UNW_LITE 0
#define HPC_UNW_MSG  1

//***************************************************************************

// HPC_UNW_LITE: When this is set, we assume unwinding must work in a
// very restricted environment such as a SEGV handler.  Performance is
// not critical.  This is a useful starting point for abstracting the
// unwinder into a library.
//
// - We must avoid dynamic allocation which rules out the use of
//   hpcrun_malloc.  This implies we cannot use 'build_intervals()' to
//   build all the intervals for a routine.  Nor can we use the interval
//   splay tree to store the intervals across invocations of hpcrun_unw_step().
//   Therefore we must use intervals in a special way and pass them
//   around as objects rather than pointers.  This last requirement
//   means that the platform-independent unwind cursor needs to store
//   the whole platform-specific interval information.
//
// - We cannot use libmonitor to test for stack bottom.
//
// - We cannot use the fnbounds server
//
// - We cannot rely on a SEGV handler to correct unwinding mistakes
// 
// - We may wish to avoid the 'pmsg' message printing library


#if (HPC_UNW_LITE)
#  define HPC_IF_UNW_LITE(x)   x
#  define HPC_IFNO_UNW_LITE(x) /* remove */
#else
#  define HPC_IF_UNW_LITE(x)   /* remove */
#  define HPC_IFNO_UNW_LITE(x) x
#endif


//***************************************************************************

#endif // unwind_cfg_h
