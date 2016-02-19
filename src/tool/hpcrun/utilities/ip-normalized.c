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

//************************* System Include Files ****************************

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

//*************************** User Include Files ****************************

#include "ip-normalized.h"
#include <messages/messages.h>

//*************************** Forward Declarations **************************

//***************************************************************************

#define NULL_OR_NAME(v) ((v) ? (v)->name : "(NULL)")

const ip_normalized_t ip_normalized_NULL_lval = ip_normalized_NULL;


ip_normalized_t
hpcrun_normalize_ip(void* unnormalized_ip, load_module_t* lm)
{
  TMSG(NORM_IP, "normalizing %p, w load_module %s", unnormalized_ip, NULL_OR_NAME(lm));
  if (!lm) {
    lm = hpcrun_loadmap_findByAddr(unnormalized_ip, unnormalized_ip);
  }
  
  if (lm && lm->dso_info) {
    ip_normalized_t ip_norm = (ip_normalized_t) {
      .lm_id = lm->id,
      .lm_ip = (uintptr_t)unnormalized_ip - lm->dso_info->start_to_ref_dist };
    return ip_norm;
  }

  TMSG(NORM_IP, "%p not normalizable", unnormalized_ip);
  if (ENABLED(NORM_IP_DBG)){
    EMSG("/proc/maps below");
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "/proc/%u/maps", getpid());
    FILE* loadmap = fopen(tmp, "r");
    char linebuf[1024 + 1];
    for (;;){
      char* l = fgets(linebuf, sizeof(linebuf), loadmap);
      if (feof(loadmap)) {
	break;
      }
      EMSG("  %s", l);
    }
    fclose(loadmap);
  }

  return (ip_normalized_t) {.lm_id = HPCRUN_FMT_LMId_NULL,
			    .lm_ip = (uintptr_t) unnormalized_ip};
}

void *
hpcrun_denormalize_ip(ip_normalized_t *normalized_ip)
{
  if (normalized_ip->lm_id != HPCRUN_FMT_LMId_NULL) {
    load_module_t* lm = hpcrun_loadmap_findById(normalized_ip->lm_id);
    if (lm != 0) {
      uint64_t offset = lm->dso_info->start_to_ref_dist;
      void *denormalized_ip = (void *) (normalized_ip->lm_ip + offset); 
      return denormalized_ip;
    }
  } 
  return (void *) normalized_ip->lm_ip;
}
