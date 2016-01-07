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

#ifndef MEM_ERROR_GEN_H
#define MEM_ERROR_GEN_H

#include "mem_error_dbg.h"

#if GEN_INF_MEM_REQ
  // special purpose extreme error condition checking code
  extern long hpcrun_num_samples_total(void);
  if (hpcrun_num_samples_total() >= 4) {
    TMSG(SPECIAL,"Hit infinite interval build to test mem kill");
    int msg_prt = 0;
    for(int i = 1;;i++){
      unwind_interval *u = (unwind_interval *) hpcrun_malloc(sizeof(unwind_interval));
      if (! u && ! msg_prt){
        TMSG(SPECIAL,"after %d intervals, memory failure",i);
        msg_prt = 1;
      }
    }
  }
#endif // GEN_INF_MEM_REQ

#endif //MEM_ERROR_GEN_H
