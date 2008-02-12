#include <stdio.h>

#include "csprof_misc_fn_stat.h"
#include "csprof_options.h"
#include "env.h"
#include "epoch.h"
#include "fname_max.h"
#include "hpcfile_general.h"
#include "hpcfile_csproflib.h"
#include "metrics.h"
#include "name.h"
#include "pmsg.h"
#include "state.h"

extern csprof_options_t opts;

int
csprof_write_profile_data(csprof_state_t* state){

  extern int csprof_using_threads;

  int ret = CSPROF_OK, ret1, ret2;
  FILE* fs;

  /* Generate a filename */
  char fnm[CSPROF_FNM_SZ];


  if (csprof_using_threads){
    sprintf(fnm, "%s/%s.%lx-%x-%ld%s", opts.out_path,
            csprof_get_executable_name(), gethostid(), state->pstate.pid,
            state->pstate.thrid, CSPROF_OUT_FNM_SFX);
  }
  else {
    sprintf(fnm, "%s/%s.%lx-%x%s", opts.out_path, csprof_get_executable_name(),
            gethostid(), state->pstate.pid, CSPROF_OUT_FNM_SFX);
  }
  PMSG(DATA_WRITE, "CSPROF write_profile_data: Writing %s", fnm);

    /* Open file for writing; fail if the file already exists. */
    fs = hpcfile_open_for_write(fnm, /* overwrite */ 0);
    ret1 = hpcfile_csprof_write(fs, csprof_get_metric_data());

    PMSG(DATA_WRITE, "Done writing metric data");

    if(ret1 != HPCFILE_OK) {
        goto error;
    }

    PMSG(DATA_WRITE, "Preparing to write epochs");
    csprof_write_all_epochs(fs);

    PMSG(DATA_WRITE, "Done writing epochs");
    /* write profile states out to disk */
    {
        csprof_state_t *runner = state;
        unsigned int nstates = 0;
        unsigned long tsamps = 0;

        /* count states */
        while(runner != NULL) {
            if(runner->epoch != NULL) {
                nstates++;
                tsamps += runner->trampoline_samples;
            }
            runner = runner->next;
        }

        hpc_fwrite_le4(&nstates, fs);
        hpc_fwrite_le8(&tsamps, fs);

        /* write states */
        runner = state;

        while(runner != NULL) {
            if(runner->epoch != NULL) {
		PMSG(DATA_WRITE, "Writing %ld nodes", runner->csdata.num_nodes);
                ret2 = csprof_csdata__write_bin(&runner->csdata,
                                                runner->epoch->id, fs);
          
                if(ret2 != CSPROF_OK) {
                    PMSG(DATA_WRITE, "Error writing tree %#lx", &runner->csdata);
                    PMSG(DATA_WRITE, "Number of tree nodes lost: %ld", runner->csdata.num_nodes);
                    EMSG("could not save profile data to file '%s'",fnm);
                    perror("write_profile_data");
                    ret = CSPROF_ERR;
                }
            }
            else {
                EMSG("Not writing tree %#lx; null epoch", &runner->csdata);
                EMSG("Number of tree nodes lost: %ld", runner->csdata.num_nodes);
            }

            runner = runner->next;
        }
    }
          
    if(ret1 == HPCFILE_OK && ret2 == CSPROF_OK) {
        PMSG(DATA_WRITE, "saved profile data to file '%s'", fnm);
    }
    /* if we've gotten this far, there haven't been any fatal errors */
    goto end;

 error:
 end:
    hpcfile_close(fs);

    return ret;
}
