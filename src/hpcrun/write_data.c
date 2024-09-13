// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "fname_max.h"
#include "unwind/common/backtrace.h"
#include "files.h"
#include "epoch.h"
#include "rank.h"
#include "thread_data.h"
#include "cct/cct_bundle.h"
#include "hpcrun_return_codes.h"
#include "write_data.h"
#include "loadmap.h"
#include "sample_prob.h"
#include "cct/cct_bundle.h"

#include "messages/messages.h"

#include "lush/lush-backtrace.h"

#include "../common/lean/hpcio.h"
#include "../common/lean/hpcfmt.h"
#include "../common/lean/hpcrun-fmt.h"
#include "../common/lean/crypto-hash.h"

#include "../common/lean/OSUtil.h"

#ifdef ENABLE_GTPIN
#include "gpu/api/intel/gtpin/gtpin-instrumentation.h"
#include "gpu/api/intel/level0/level0-api.h"
#endif

//*****************************************************************************
// structs and types
//*****************************************************************************

static epoch_flags_t epoch_flags = {
    .bits = 0};

//*****************************************************************************
// macro
//*****************************************************************************
#define MULTIPLE_1024(v) (v + 1023) & ~1023

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
lazy_open_data_file(core_profile_trace_data_t *cptd)
{
  FILE *fs = cptd->hpcrun_file;
  if (fs)
  {
    return fs;
  }

  int rank = hpcrun_get_rank();
  if (rank < 0)
  {
    rank = 0;
  }

  int fd = hpcrun_open_profile_file(rank, cptd->id);
  fs = fdopen(fd, "w");
  if (fs == NULL)
  {
    EEMSG("HPCToolkit: %s: unable to open profile file", __func__);
    return NULL;
  }
  cptd->hpcrun_file = fs;

  if (!hpcrun_sample_prob_active())
    return fs;

  const unsigned int bufSZ = 32; // sufficient to hold a 64-bit integer in base 10

  const char *jobIdStr = OSUtil_jobid();
  if (!jobIdStr)
  {
    jobIdStr = "";
  }

  char mpiRankStr[bufSZ];
  mpiRankStr[0] = '\0';
  snprintf(mpiRankStr, bufSZ, "%d", rank);

  char tidStr[bufSZ];
  snprintf(tidStr, bufSZ, "%d", cptd->id);

  char hostidStr[bufSZ];
  snprintf(hostidStr, bufSZ, "%x", OSUtil_hostid());

  char pidStr[bufSZ];
  snprintf(pidStr, bufSZ, "%u", OSUtil_pid());

  char traceMinTimeStr[bufSZ];
  snprintf(traceMinTimeStr, bufSZ, "%" PRIu64, cptd->trace_min_time_us);

  char traceMaxTimeStr[bufSZ];
  snprintf(traceMaxTimeStr, bufSZ, "%" PRIu64, cptd->trace_max_time_us);

  char traceDisorderStr[bufSZ];
  snprintf(traceDisorderStr, bufSZ, "%u", cptd->trace_expected_disorder);

  //
  // ==== file hdr =====
  //

  TMSG(DATA_WRITE, "writing file header");
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
                        HPCRUN_FMT_NV_traceDisorder,
                        cptd->trace_is_ordered ? "0" : traceDisorderStr,
                        NULL);

  return fs;
}

// YUMENG: add footer
static int
write_epochs(FILE *fs, core_profile_trace_data_t *cptd, epoch_t *epoch, hpcrun_fmt_footer_t *footer)
{
  // YUMENG: no epoch info needed
  // uint32_t num_epochs = 0;

  if (!hpcrun_sample_prob_active())
    return HPCRUN_OK;

  //
  // === # epochs ===
  //

  epoch_t *current_epoch = epoch;
  int ret;

  //
  // for each epoch ...
  //

  for (epoch_t *s = current_epoch; s; s = s->next)
  {

    //
    // == load map ==
    //
    if (footer)
      footer->loadmap_start = ftell(fs);

    TMSG(DATA_WRITE, "Preparing to write loadmap");

    hpcrun_loadmap_t *current_loadmap = s->loadmap;

    if (current_loadmap == 0) {
      EMSG("HPCToolkit: Unable to write profile data: loadmap is null");
      return HPCRUN_ERR;
    }

    hpcfmt_int4_fwrite(current_loadmap->size, fs);

    // N.B.: Write in reverse order to obtain nicely ascending LM ids.
    for (load_module_t *lm_src = current_loadmap->lm_end;
         (lm_src); lm_src = lm_src->prev)
    {
      loadmap_entry_t lm_entry;
      lm_entry.id = lm_src->id;
      lm_entry.name = lm_src->name;
      lm_entry.flags = hpcrun_loadModule_flags_get(lm_src);

      ret = hpcrun_fmt_loadmapEntry_fwrite(&lm_entry, fs);
      if (ret != HPCFMT_OK)
      {
        TMSG(DATA_WRITE, "Error writing loadmap entry");
        goto write_error;
      }
    }

    // set footer
    if (footer) {
      footer->loadmap_end = ftell(fs);
      fseek(fs, MULTIPLE_1024(footer->loadmap_end), SEEK_SET);
    }

    TMSG(DATA_WRITE, "Done writing loadmap");

    //
    // == cct ==
    //

    cct_bundle_t *cct = &(s->csdata);

    // YUMENG: set up sparse_metrics and walk through cct
    // footer
    if (footer)
      footer->cct_start = ftell(fs);

    // initialize the sparse_metrics
    hpcrun_fmt_sparse_metrics_t sparse_metrics;
    sparse_metrics.id_tuple = cptd->id_tuple;

    // assign value to sparse metrics while writing cct info
    ret = hpcrun_cct_bundle_fwrite(fs, epoch_flags, cct, cptd->cct2metrics_map, &sparse_metrics);

    // footer
    if (footer)
    {
      footer->cct_end = ftell(fs);
      fseek(fs, MULTIPLE_1024(footer->cct_end), SEEK_SET);
    }

    if (ret != HPCRUN_OK) {
      TMSG(DATA_WRITE, "Error writing tree %#lx or collecting sparse metrics", cct);
      TMSG(DATA_WRITE, "Number of tree nodes lost: %ld", cct->num_nodes);
      goto write_error;
    }
    else
    {
      TMSG(DATA_WRITE, "saved cct data to hpcrun file and recorded profile data as sparse metrics");
    }

#if 1
    //
    // == metrics ==
    //
    // YUMENG: footer
    if (footer)
      footer->met_tbl_start = ftell(fs);

    kind_info_t *curr = NULL;
    metric_desc_p_tbl_t *metric_tbl = hpcrun_get_metric_tbl(&curr);

    hpcfmt_int4_fwrite(hpcrun_get_num_kind_metrics(), fs);
    while (curr != NULL)
    {
      TMSG(DATA_WRITE, "metric tbl len = %d", metric_tbl->len);
      ret = hpcrun_fmt_metricTbl_fwrite(metric_tbl, fs);
      metric_tbl = hpcrun_get_metric_tbl(&curr);

      if (ret != HPCFMT_OK)
      {
        TMSG(DATA_WRITE, "Error writing metric-tbl");
        goto write_error;
      }
    }

    // YUMENG: footer
    if (footer)
    {
      footer->met_tbl_end = ftell(fs);
      fseek(fs, MULTIPLE_1024(footer->met_tbl_end), SEEK_SET);
    }

    TMSG(DATA_WRITE, "Done writing metric tbl");
#endif

    //
    // == id-tuple dictionary ==
    //
    if (footer)
      footer->idtpl_dxnry_start = ftell(fs);

    hpcrun_fmt_idtuple_dxnry_fwrite(fs);

    if (footer)
    {
      footer->idtpl_dxnry_end = ftell(fs);
      fseek(fs, MULTIPLE_1024(footer->idtpl_dxnry_end), SEEK_SET);
    }

    //
    // ==  sparse_metrics == YUMENG
    //

    // footer
    if (footer)
      footer->sm_start = ftell(fs);

    ret = hpcrun_fmt_sparse_metrics_fwrite(&sparse_metrics, fs);
    if (ret != HPCFMT_OK)
    {
      TMSG(DATA_WRITE, "Error writing sparse metrics data");
      goto write_error;
    }

    if (footer)
    {
      footer->sm_end = ftell(fs);
      fseek(fs, MULTIPLE_1024(footer->sm_end), SEEK_SET);
      // record for footer section
      footer->footer_start = ftell(fs);
    }

    TMSG(DATA_WRITE, "Done writing sparse metrics data");

    //
    // == footer == YUMENG
    //
    if (s->next == NULL)
    {
      footer->HPCRUNsm = HPCRUNsm;
      ret = hpcrun_fmt_footer_fwrite(footer, fs);
      if (ret != HPCFMT_OK)
      {
        TMSG(DATA_WRITE, "Error writing footer");
        goto write_error;
      }
    }

    TMSG(DATA_WRITE, "Done writing footer");

    current_loadmap++;

  } // epoch loop

  return HPCRUN_OK;

 write_error:
  EMSG("could not save profile data to hpcrun file");
  perror("write_profile_data");
  return HPCRUN_ERR;
}

void hpcrun_flush_epochs(core_profile_trace_data_t *cptd)
{
  FILE *fs = lazy_open_data_file(cptd);
  if (fs == NULL)
    return;

  write_epochs(fs, cptd, cptd->epoch, NULL);
  hpcrun_epoch_reset();
}

int hpcrun_write_profile_data(core_profile_trace_data_t *cptd)
{
#ifdef ENABLE_GTPIN
  if (level0_gtpin_enabled()) {
    for (epoch_t *epoch = cptd->epoch; epoch; epoch = epoch->next) {
      gtpin_process_block_instructions(epoch->csdata.top);
    }
  }
#endif

  if (cptd->scale_fn)
    cptd->scale_fn((void *)cptd);

  // YUMENG: set up footer
  // size_t footer[SF_FOOTER_LENGTH];
  // footer[SF_FOOTER_hdr] = 0;
  hpcrun_fmt_footer_t footer;
  footer.hdr_start = 0;

  TMSG(DATA_WRITE, "Writing hpcrun profile data");
  FILE *fs = lazy_open_data_file(cptd);

  // YUMENG: set footer
  footer.hdr_end = ftell(fs);
  fseek(fs, MULTIPLE_1024(footer.hdr_end), SEEK_SET);

  if (fs == NULL)
    return HPCRUN_ERR;

  write_epochs(fs, cptd, cptd->epoch, &footer);

  TMSG(DATA_WRITE, "closing file");
  hpcio_fclose(fs);
  TMSG(DATA_WRITE, "Done!");

  return HPCRUN_OK;
}

//
// DEBUG: fetch and print current loadmap
//
void hpcrun_dbg_print_current_loadmap(void)
{
  hpcrun_loadmap_print(hpcrun_get_thread_epoch()->loadmap);
}
