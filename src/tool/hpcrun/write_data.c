// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
#include <setjmp.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "monitor.h"
#include "fname_max.h"
#include "backtrace.h"
#include "files.h"
#include "state.h"
#include "thread_data.h"
#include "cct.h"
#include "hpcrun_return_codes.h"
#include "write_data.h"
#include "epoch.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lush/lush-backtrace.h>

//*****************************************************************************
// structs and types
//*****************************************************************************

static epoch_flags_t epoch_flags = {
  .bits = 0
};
static const uint32_t default_ra_distance = 1;
static const uint64_t default_granularity = 1;

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
lazy_open_data_file(void)
{
  thread_data_t* td = csprof_get_thread_data();

  FILE* fs = td->hpcrun_file;
  if (fs) {
    return fs;
  }

  /* Generate a filename */
  char fnm[HPCRUN_FNM_SZ];

  int rank = monitor_mpi_comm_rank();
  if (rank < 0) rank = 0;
  files_profile_name(fnm, rank, HPCRUN_FNM_SZ);

  TMSG(DATA_WRITE, "Filename = %s", fnm);

  /* Open file for writing; fail if the file already exists. */
  fs = hpcio_fopen_w(fnm, /* overwrite */ 0);

  td->hpcrun_file = fs;

  const uint bufSZ = 32;

  const char* jobIdStr = os_job_id();
  if (!jobIdStr) {
    jobIdStr = "";
  }

  char mpiRankStr[bufSZ];
  mpiRankStr[0] = '\0';
  if (monitor_mpi_comm_rank() >= 0) {
    snprintf(mpiRankStr, bufSZ, "%d", rank);
  }

  char tidStr[bufSZ];
  snprintf(tidStr, bufSZ, "%d", td->id);

  char hostidStr[bufSZ];
  snprintf(hostidStr, bufSZ, "%lx", os_hostid());

  char pidStr[bufSZ];
  snprintf(pidStr, bufSZ, "%u", os_pid());


  //
  // ==== file hdr =====
  //

  TMSG(DATA_WRITE,"writing file header");
  hpcrun_fmt_hdr_fwrite(fs,
                        HPCRUN_FMT_NV_prog, files_executable_name(),
                        HPCRUN_FMT_NV_path, files_executable_pathname(),
                        HPCRUN_FMT_NV_jobId, jobIdStr,
                        HPCRUN_FMT_NV_mpiRank, mpiRankStr,
                        HPCRUN_FMT_NV_tid, tidStr,
                        HPCRUN_FMT_NV_hostid, hostidStr,
                        HPCRUN_FMT_NV_pid, pidStr,
                        "nasty-message", "Please support <lm-id, lm-offset>!",
                        NULL);
  return fs;
}

static int
write_epochs(FILE* fs, csprof_state_t* state)
{
  uint32_t num_epochs = 0;

  //
  // === # epochs === 
  //

  csprof_state_t* current_state = state;
  for(csprof_state_t* s = current_state; s; s = s->next) {
    num_epochs++;
  }

  TMSG(EPOCH, "Actual # epochs = %d", num_epochs);

  TMSG(DATA_WRITE, "writing # epochs = %d", num_epochs);

  //
  // for each epoch ...
  //

  for(csprof_state_t* s = current_state; s; s = s->next) {

    if (! ENABLED(WRITE_EMPTY_EPOCH)){
      if (hpcrun_empty_cct(&(s->csdata))){
	EMSG("Empty cct encountered: it is not written out");
	continue;
      }
    }
    //
    //  == epoch header ==
    //

    TMSG(DATA_WRITE," epoch header");
    //
    // set epoch flags before writing
    //

    epoch_flags.flags.isLogicalUnwind = hpcrun_isLogicalUnwind();
    
    hpcrun_fmt_epoch_hdr_fwrite(fs, epoch_flags,
                                default_ra_distance,
                                default_granularity,
                                "TODO:epoch-name","TODO:epoch-value",
                                NULL);

    //
    // == metrics ==
    //

    metric_tbl_t *metric_tbl = hpcrun_get_metric_data();

    TMSG(DATA_WRITE, "metric data num metrics = %d", metric_tbl->len);

    hpcrun_fmt_metricTbl_fwrite(metric_tbl, fs);

    TMSG(DATA_WRITE, "Done writing metric data");

    //
    // == load map ==
    //

    TMSG(DATA_WRITE, "Preparing to write loadmap");

    hpcrun_epoch_t* current_epoch = s->epoch;
    
    hpcfmt_byte4_fwrite(current_epoch->num_modules, fs);

    loadmap_src_t* lm_src = current_epoch->loaded_modules;
    for (uint32_t i = 0; i < current_epoch->num_modules; i++) {
      loadmap_entry_t lm_entry;
      lm_entry.id = lm_src->id;
      lm_entry.name = lm_src->name;
      lm_entry.vaddr = (uint64_t)lm_src->vaddr;
      lm_entry.mapaddr = (uint64_t)lm_src->mapaddr;
      lm_entry.flags = 0;

      hpcrun_fmt_loadmapEntry_fwrite(&lm_entry, fs);

      lm_src = lm_src->next;
    }

    TMSG(DATA_WRITE, "Done writing loadmap");

    //
    // == cct ==
    //

    hpcrun_cct_t*    cct           = &(s->csdata);
    lush_cct_ctxt_t* lush_cct_ctxt = s->csdata_ctxt;

    TMSG(DATA_WRITE, "Writing %ld nodes", cct->num_nodes);

    int ret = hpcrun_cct_fwrite(fs, epoch_flags, cct, lush_cct_ctxt);
          
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
    current_epoch++;

  } // epoch loop

  return HPCRUN_OK;
}

void
hpcrun_flush_epochs(void)
{
  FILE *fs = lazy_open_data_file();
  write_epochs(fs, TD_GET(state));
  hpcrun_epoch_reset();
}

int
hpcrun_write_profile_data(csprof_state_t *state)
{

  FILE* fs = lazy_open_data_file();
  write_epochs(fs, state);

  TMSG(DATA_WRITE,"closing file");
  hpcio_close(fs);
  TMSG(DATA_WRITE,"Done!");

  return HPCRUN_OK;
}
