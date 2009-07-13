// -*-Mode: C++;-*- // technically C99
// $Id$

#include <stdio.h>
#include <setjmp.h>

#include "pmsg.h"
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

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>


// ******** default values for epoch hdr **********
//
//     ===== FIXME: correct these later ===
//

static const uint64_t default_epoch_flags = 0;
static const uint32_t default_ra_distance = 1;
static const uint64_t default_granularity = 1;

// ******** local utilities **********

static char *
hpcrun_itos(char *buf, int i)
{
  sprintf(buf, "%d", i);
  return buf;
}

//***************************************************************************
//
//        The top level
//
//  hpcrun_fmt_hdr_fwrite()
//  hpcrun_le4_fwrite(# epochs)
//  foreach epoch
//     hpcrun_epoch_fwrite()
//
//        Writing an epoch
//
//  hpcrun_fmt_epoch_hdr_fwrite(flags, char-rtn-dst, gran, NVPs) /* char-rtn-dst = 1 4theMomnt */
//  hpcrun_fmt_metric_tbl_fwrite()
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
  FILE* fs;

  if ((fs = TD_GET(hpcrun_file))){
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

  TD_GET(hpcrun_file) = fs;

  char _tmp[10];

  //
  // ==== file hdr =====
  //

  TMSG(DATA_WRITE,"writing file header");
  hpcrun_fmt_hdr_fwrite(fs,
                        "program-name", "TBD",
                        "process_id", "TBD",
                        "mpi-rank", hpcrun_itos(_tmp, rank),
                        "target", "--FIXME--target_name",
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

    if (ENABLED(SKIP_ZERO_METRIC_EPOCH)){
      if (no_metric_samples(&(s->csdata))) {
	TMSG(DATA_WRITE, "empty cct in epoch -- skipping");
	continue;
      }
    }
    //
    //  == epoch header ==
    //

    TMSG(DATA_WRITE," epoch header");
    hpcrun_fmt_epoch_hdr_fwrite(fs, default_epoch_flags,
                                default_ra_distance,
                                default_granularity,
                                "LIP-size","2",
                                NULL);

    //
    // == metrics ==
    //

    metric_tbl_t *metric_tbl = hpcrun_get_metric_data();

    TMSG(DATA_WRITE,"metric data num metrics = %d",metric_tbl->len);

    hpcrun_fmt_metric_tbl_fwrite(metric_tbl, fs);

    TMSG(DATA_WRITE, "Done writing metric data");

    //
    // == load map ==
    //

    TMSG(DATA_WRITE, "Preparing to write loadmap");

    csprof_epoch_t* current_epoch = s->epoch;
    hpcrun_fmt_loadmap_fwrite(current_epoch->num_modules, current_epoch->loaded_modules, fs);

    TMSG(DATA_WRITE, "Done writing loadmap");

    //
    // == cct ==
    //

    hpcrun_cct_t*    cct           = &(s->csdata);
    lush_cct_ctxt_t* lush_cct_ctxt = s->csdata_ctxt;

    TMSG(DATA_WRITE, "Writing %ld nodes", cct->num_nodes);

    int ret = csprof_cct__write_bin(fs, cct, lush_cct_ctxt);
          
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
