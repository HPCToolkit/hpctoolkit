#include <stdio.h>
#include <setjmp.h>
#include "pmsg.h"
#include "monitor.h"
#include "fname_max.h"
#include "backtrace.h"
#include "files.h"
#include "state.h"
#include "thread_data.h"
#include "csprof_csdata.h"
#include "hpcrun_return_codes.h"
#include "hpcio.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"
#include "write_data.h"

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

int
hpcrun_write_profile_data(csprof_state_t *state)
{
  FILE* fs;

  int ret;

  /* Generate a filename */
  char fnm[HPCRUN_FNM_SZ];


  int rank = monitor_mpi_comm_rank();
  if (rank < 0) rank = 0;
  files_profile_name(fnm, rank, HPCRUN_FNM_SZ);

  sigjmp_buf_t *it = &(TD_GET(mem_error));

  if (sigsetjmp(it->jb,1)){
    EEMSG("***hpcrun failed to write profile data due to memory allocation failure***");
    return HPCRUN_ERR;
  }

  TMSG(DATA_WRITE, "Filename = %s", fnm);

  /* Open file for writing; fail if the file already exists. */
  fs = hpcio_fopen_w(fnm, /* overwrite */ 0);

//***************************************************************************
//
//        The top level loop
//
//  hpcrun_fmt_hdr_fwrite()
//  hpcrun_le4_fwrite(# epochs)
//  foreach epoch
//     hpcrun_epoch_fwrite()
//
// hpcrun_epoch_fwrite()
//    hpcrun_fmt_epoch_hdr_fwrite(flags, char-rtn-dst, gran, NVPs) /* char-rtn-dst = 1 4theMomnt */
//    hpcrun_fmt_metric_tbl_fwrite()
//    hpcrun_fmt_loadmap_fwrite()
//    /* write cct */
//    hpcrun_le4_fwrite(# cct_nodes)
//    foreach cct-node
//       hpcrun_fmt_cct_node_fwrite(cct_node_t *p)
//
//***************************************************************************

  char _tmp[10];
  uint32_t num_epochs = 0;

  //
  // ==== hdr =====
  //
  TMSG(DATA_WRITE,"writing file header");
  hpcrun_fmt_hdr_fwrite(fs,
                        "program-name", "TBD",
                        "process_id", "TBD",
                        "mpi-rank", hpcrun_itos(_tmp, rank),
                        "target", "--FIXME--target_name",
                        NULL);

  //
  // === # epochs === 
  //

  csprof_epoch_t *current_epoch = csprof_get_epoch();
  for(csprof_epoch_t *ep = current_epoch; ep != NULL; ep = ep->next) {
    num_epochs++;
  }


  // ******FIXME*********
  //
  //   Limit num_epochs to 1 FOR THE MOMENT!
  //
  num_epochs = (num_epochs > 1) ? 1 : num_epochs;
  
  TMSG(DATA_WRITE, "writing # epochs = %d", num_epochs);
  hpcfmt_byte4_fwrite(num_epochs, fs);

  //
  // for each epoch ...
  //

  for (int i=0; i < num_epochs; i++) {

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

    TMSG(DATA_WRITE, "Preparing to write loadmaps");

    hpcrun_fmt_loadmap_fwrite(current_epoch->num_modules, current_epoch->loaded_modules, fs);

    TMSG(DATA_WRITE, "Done writing loadmaps");

    //
    // == cct ==
    //

    hpcrun_cct_t    *cct           = &(state->csdata);
    lush_cct_ctxt_t *lush_cct_ctxt = state->csdata_ctxt;

    TMSG(DATA_WRITE, "Writing %ld nodes", cct->num_nodes);

    ret = csprof_cct__write_bin(fs, cct, lush_cct_ctxt);
          
    if(ret != HPCRUN_OK) {
      TMSG(DATA_WRITE, "Error writing tree %#lx", cct);
      TMSG(DATA_WRITE, "Number of tree nodes lost: %ld", cct->num_nodes);
      EMSG("could not save profile data to file '%s'", __FILE__, __LINE__, fnm);
      perror("write_profile_data");
      ret = HPCRUN_ERR; // FIXME: return this value now
    }
    else {
      TMSG(DATA_WRITE, "saved profile data to file '%s'", fnm);
    }
    current_epoch++;

  } // epoch loop

  TMSG(DATA_WRITE,"closing file");
  hpcio_close(fs);
  TMSG(DATA_WRITE,"Done!");

  return HPCRUN_OK;
}
