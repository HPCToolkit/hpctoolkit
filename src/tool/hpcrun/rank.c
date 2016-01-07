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

//***************************************************************************
//
// File:
// $HeadURL$
//
// Purpose:
// This file generalizes how hpcrun computes the 'rank' of a process,
// eg: MPI, dmapp, gasnet, etc.
//
// For now, this API acts passively, that is, it waits for something
// else in hpcrun to ask what is the rank.  But we may need to
// generalize this to actively find the rank in some cases.
//
// Note: we kinda assume that all methods (if available) return
// consistent answers.
//
//***************************************************************************

#include <stdint.h>
#include <monitor.h>
#include "rank.h"

struct jobinfo {
  int  version;
  int  hw_version;
  int  npes;
  int  pe;
  long pad[20];
};

int dmapp_get_jobinfo(struct jobinfo *info);

extern int32_t gasneti_mynode;


//***************************************************************************
// interface functions
//***************************************************************************

// Returns: generalized rank, or else -1 if unknown or unavailable.
//
int
hpcrun_get_rank(void)
{
  int rank;

  rank = monitor_mpi_comm_rank();
  if (rank >= 0) {
    return rank;
  }

  // Dmapp only runs on Cray systems which pretty much run only static
  // binaries.  The dynamic case probably works but is currently
  // untested (because there are few or no examples), so ifdef it out
  // for now.
  //
#ifdef HPCRUN_STATIC_LINK
  struct jobinfo info;
  if (dmapp_get_jobinfo(&info) == 0) {
    rank = info.pe;
    if (rank >= 0) {
      return rank;
    }
  }
#endif

  // Gasnet stores the rank in a global variable and <gasnet.h> makes
  // heavy use of macros.  So, there is no gasnet_mynode() function to
  // call.  We can catch the variable statically via hpclink, but
  // basically there's no hope for the dynamic case.
  //
#ifdef HPCRUN_STATIC_LINK
  rank = (int) gasneti_mynode;
  if (rank >= 0) {
    return rank;
  }
#endif

  return -1;
}
