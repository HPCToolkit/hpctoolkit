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
// system includes
//*****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "fname_max.h"
#include "backtrace.h"
#include "files.h"
#include "epoch.h"
#include "rank.h"
#include "thread_data.h"
#include "cct_bundle.h"
#include "hpcrun_return_codes.h"
#include "write_data.h"
#include "loadmap.h"
#include "sample_prob.h"
#include "cct/cct_bundle.h"

#include <messages/messages.h>

#include <lush/lush-backtrace.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support-lean/OSUtil.h>


//*****************************************************************************
// structs and types
//*****************************************************************************

static epoch_flags_t epoch_flags = {
  .bits = 0
};

static const uint64_t default_measurement_granularity = 1;



//*****************************************************************************
// local utilities
//*****************************************************************************


//***************************************************************************
//
//        The top level
//
//  hpcrun_fmt_hdr_fwrite()
//  foreach epoch
//     hpcrun_epoch_fwrite()
//
//        Writing an epoch
//
//  hpcrun_fmt_epoch_hdr_fwrite(flags, char-rtn-dst, gran, NVPs) /* char-rtn-dst = 1 4theMomnt */
//  hpcrun_fmt_metricTbl_fwrite()
//  hpcrun_fmt_loadmap_fwrite()
//  hpcrun_le4_fwrite(# cct_nodes)
//  foreach cct-node
//    hpcrun_fmt_cct_node_fwrite(cct_node_t *p)
//
//***************************************************************************

//***************************************************************************
//
// Above functionality is factored into 2 pieces:
//
//    1) (Lazily) open the output file, and write the file header
//    2) Write the epochs
//
// This factoring enables the writing of the current set of epochs at anytime during
// the sampling run.
// Currently, there are 2 such situations:
//   1) The end of the sampling run. This is the normal place to write profile data
//   2) When sample data memory is low. In this case, the profile data is written
//      out, but the sample memory is reclaimed so that more profile data may be
//      collected.
//
//***************************************************************************

static FILE *
lazy_open_data_file(core_profile_trace_data_t * cptd)
{
  FILE* fs = cptd->hpcrun_file;
  if (fs) {
    return fs;
  }

  int rank = hpcrun_get_rank();
  if (rank < 0) {
    rank = 0;
  }
  int fd = hpcrun_open_profile_file(rank, cptd->id);
  fs = fdopen(fd, "w");
  if (fs == NULL) {
    EEMSG("HPCToolkit: %s: unable to open profile file", __func__);
    return NULL;
  }
  cptd->hpcrun_file = fs;

  if (! hpcrun_sample_prob_active())
    return fs;

  const uint bufSZ = 32; // sufficient to hold a 64-bit integer in base 10

  const char* jobIdStr = OSUtil_jobid();
  if (!jobIdStr) {
    jobIdStr = "";
  }

  char mpiRankStr[bufSZ];
  mpiRankStr[0] = '\0';
  snprintf(mpiRankStr, bufSZ, "%d", rank);

  char tidStr[bufSZ];
  snprintf(tidStr, bufSZ, "%d", cptd->id);

  char hostidStr[bufSZ];
  snprintf(hostidStr, bufSZ, "%lx", OSUtil_hostid());

  char pidStr[bufSZ];
  snprintf(pidStr, bufSZ, "%u", OSUtil_pid());

  char traceMinTimeStr[bufSZ];
  snprintf(traceMinTimeStr, bufSZ, "%"PRIu64, cptd->trace_min_time_us);

  char traceMaxTimeStr[bufSZ];
  snprintf(traceMaxTimeStr, bufSZ, "%"PRIu64, cptd->trace_max_time_us);

  //
  // ==== file hdr =====
  //

  TMSG(DATA_WRITE,"writing file header");
  hpcrun_fmt_hdr_fwrite(fs,
                        HPCRUN_FMT_NV_prog, hpcrun_files_executable_name(),
                        HPCRUN_FMT_NV_progPath, hpcrun_files_executable_pathname(),
			                  HPCRUN_FMT_NV_envPath, getenv("PATH"),
                        HPCRUN_FMT_NV_jobId, jobIdStr,
                        HPCRUN_FMT_NV_mpiRank, mpiRankStr,
                        HPCRUN_FMT_NV_tid, tidStr,
                        HPCRUN_FMT_NV_hostid, hostidStr,
                        HPCRUN_FMT_NV_pid, pidStr,
                        HPCRUN_FMT_NV_traceMinTime, traceMinTimeStr,
                        HPCRUN_FMT_NV_traceMaxTime, traceMaxTimeStr,
                        HPCRUN_FMT_NV_traceOrdered, cptd->traceOrdered?"1":"0",
                        NULL);
  return fs;
}


static int
write_epochs(FILE* fs, core_profile_trace_data_t * cptd, epoch_t* epoch)
{
  uint32_t num_epochs = 0;

  if (! hpcrun_sample_prob_active())
    return HPCRUN_OK;

  //
  // === # epochs === 
  //

  epoch_t* current_epoch = epoch;
  for(epoch_t* s = current_epoch; s; s = s->next) {
    num_epochs++;
  }

  TMSG(EPOCH, "Actual # epochs = %d", num_epochs);

  TMSG(DATA_WRITE, "writing # epochs = %d", num_epochs);

  //
  // for each epoch ...
  //

  for(epoch_t* s = current_epoch; s; s = s->next) {

#if 0
    if (ENABLED(SKIP_WRITE_EMPTY_EPOCH)){
      if (hpcrun_empty_cct_bundle(&(s->csdata))){
	EMSG("Empty cct encountered: it is not written out");
	continue;
      }
    }
#endif
    //
    //  == epoch header ==
    //

    TMSG(DATA_WRITE," epoch header");
    //
    // set epoch flags before writing
    //

    epoch_flags.fields.isLogicalUnwind = hpcrun_isLogicalUnwind();
    TMSG(LUSH,"epoch lush flag set to %s", epoch_flags.fields.isLogicalUnwind ? "true" : "false");
    
    TMSG(DATA_WRITE,"epoch flags = %"PRIx64"", epoch_flags.bits);
    hpcrun_fmt_epochHdr_fwrite(fs, epoch_flags,
			       default_measurement_granularity,
			       "TODO:epoch-name","TODO:epoch-value",
			       NULL);

    //
    // == metrics ==
    //

    kind_info_t *curr = NULL;
    metric_desc_p_tbl_t *metric_tbl = hpcrun_get_metric_tbl(&curr);

    hpcfmt_int4_fwrite(hpcrun_get_num_kind_metrics(), fs);
    while (curr != NULL) {
      TMSG(DATA_WRITE, "metric tbl len = %d", metric_tbl->len);
      hpcrun_fmt_metricTbl_fwrite(metric_tbl, cptd->perf_event_info, fs);
      metric_tbl = hpcrun_get_metric_tbl(&curr);
    }

    TMSG(DATA_WRITE, "Done writing metric data");

    //
    // == load map ==
    //

    TMSG(DATA_WRITE, "Preparing to write loadmap");

    hpcrun_loadmap_t* current_loadmap = s->loadmap;
    
    hpcfmt_int4_fwrite(current_loadmap->size, fs);

    // N.B.: Write in reverse order to obtain nicely ascending LM ids.
    for (load_module_t* lm_src = current_loadmap->lm_end;
	 (lm_src); lm_src = lm_src->prev) {
      loadmap_entry_t lm_entry;
      lm_entry.id = lm_src->id;
      lm_entry.name = lm_src->name;
      lm_entry.flags = 0;
      
      hpcrun_fmt_loadmapEntry_fwrite(&lm_entry, fs);
    }

    TMSG(DATA_WRITE, "Done writing loadmap");

    //
    // == cct ==
    //

    cct_bundle_t* cct      = &(s->csdata);
    int ret = hpcrun_cct_bundle_fwrite(fs, epoch_flags, cct, cptd->cct2metrics_map);
    if(ret != HPCRUN_OK) {
      TMSG(DATA_WRITE, "Error writing tree %#lx", cct);
      TMSG(DATA_WRITE, "Number of tree nodes lost: %ld", cct->num_nodes);
      EMSG("could not save profile data to hpcrun file");
      perror("write_profile_data");
      ret = HPCRUN_ERR; // FIXME: return this value now
    }
    else {
      TMSG(DATA_WRITE, "saved profile data to hpcrun file ");
    }
    current_loadmap++;

  } // epoch loop

  return HPCRUN_OK;
}


void
hpcrun_flush_epochs(core_profile_trace_data_t * cptd)
{
  FILE *fs = lazy_open_data_file(cptd);
  if (fs == NULL)
    return;

  write_epochs(fs, cptd, cptd->epoch);
  hpcrun_epoch_reset();
}

int
hpcrun_write_profile_data(core_profile_trace_data_t * cptd)
{
  if(cptd->scale_fn) cptd->scale_fn((void*)cptd);

  TMSG(DATA_WRITE,"Writing hpcrun profile data");
  FILE* fs = lazy_open_data_file(cptd);
  if (fs == NULL)
    return HPCRUN_ERR;

  write_epochs(fs, cptd, cptd->epoch);

  TMSG(DATA_WRITE,"closing file");
  hpcio_fclose(fs);
  TMSG(DATA_WRITE,"Done!");

  return HPCRUN_OK;
}

//
// DEBUG: fetch and print current loadmap
//
void
hpcrun_dbg_print_current_loadmap(void)
{
  hpcrun_loadmap_print(hpcrun_get_thread_epoch()->loadmap);
}
