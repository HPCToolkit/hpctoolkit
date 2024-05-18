// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//************************* System Include Files ****************************

#define _GNU_SOURCE

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

//*************************** User Include Files ****************************

#include "../../common/lean/placeholders.h"

#include "ip-normalized.h"
#include "../messages/messages.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// macros
//***************************************************************************

#define NULL_OR_NAME(v) ((v) ? (v)->name : "(NULL)")



//***************************************************************************
// constants
//***************************************************************************

const ip_normalized_t ip_normalized_NULL =
    (ip_normalized_t){HPCRUN_PLACEHOLDER_LM, 0};



//***************************************************************************
// interface functions
//***************************************************************************
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

  return (ip_normalized_t){HPCRUN_PLACEHOLDER_LM, hpcrun_placeholder_unnormalized_ip};
}

void *
hpcrun_denormalize_ip(ip_normalized_t *normalized_ip)
{
  if (normalized_ip->lm_id != HPCRUN_PLACEHOLDER_LM) {
    load_module_t* lm = hpcrun_loadmap_findById(normalized_ip->lm_id);
    if (lm != 0) {
      uint64_t offset = lm->dso_info->start_to_ref_dist;
      void *denormalized_ip = (void *) (normalized_ip->lm_ip + offset);
      return denormalized_ip;
    }
  }
  // Unable to denormalize, return a bad value
  return NULL;
}
