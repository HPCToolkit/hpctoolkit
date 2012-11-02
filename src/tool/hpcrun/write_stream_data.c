// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/write_stream_data.c $
// $Id: write_data.c 3650 2012-01-18 21:02:40Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
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

//*****************************************************************************
// local includes
//*****************************************************************************

#include "stream.h"

static epoch_flags_t stream_epoch_flags = {
  .bits = 0
};

static const uint64_t stream_default_measurement_granularity = 1;

static const uint32_t stream_default_ra_to_callsite_distance =
#if defined(HOST_PLATFORM_MIPS64LE_LINUX)
  8 // move past branch delay slot
#else
  1 // probably sufficient all architectures without a branch-delay slot
#endif
  ;


//*****************************************************************************
// structs and types
//*****************************************************************************
static FILE * open_stream_data_file(core_profile_trace_data_t *st)
{
  thread_data_t* td = hpcrun_get_thread_data();

  FILE* f = st->hpcrun_file;
  if (f) {
    return f;
  }

  /*
 * FIXME: open_profile_file has to be changed but for now lets assume
 * that this is bigger than the number of processes
 */
	int rank = hpcrun_get_rank();//td->rank;//st->device_id; 
	if (rank < 0) {
		rank = 0;
	}
  int fd = hpcrun_open_profile_file(rank, st->id);

  f = fdopen(fd, "w");
  if (f == NULL) {
    EEMSG("HPCToolkit: %s: unable to open profile file", __func__);
    return NULL;
  }
  st->hpcrun_file = f;

  if (! hpcrun_sample_prob_active())
    return f;

  const uint bufSZ = 32; // sufficient to hold a 64-bit integer in base 10

  const char* jobIdStr = OSUtil_jobid();
  if (!jobIdStr) {
    jobIdStr = "";
  }

  char mpiRankStr[bufSZ];
  mpiRankStr[0] = '\0';
  snprintf(mpiRankStr, bufSZ, "%d", rank);

  char tidStr[bufSZ];
  snprintf(tidStr, bufSZ, "%d", st->id);

  char hostidStr[bufSZ];
  snprintf(hostidStr, bufSZ, "%lx", OSUtil_hostid());

  char pidStr[bufSZ];
  snprintf(pidStr, bufSZ, "%u", OSUtil_pid());

  char traceMinTimeStr[bufSZ];
  snprintf(traceMinTimeStr, bufSZ, "%"PRIu64, st->trace_min_time_us);
	//printf("\nThe start time  is %x for stream %d",st->trace_min_time_us, st->id);

  char traceMaxTimeStr[bufSZ];
  snprintf(traceMaxTimeStr, bufSZ, "%"PRIu64, st->trace_max_time_us);
	//printf("\nThe end time  is %x for stream %d",st->trace_max_time_us, st->id);

  //
  // ==== file hdr =====
  //

  TMSG(DATA_WRITE,"writing file header");
  hpcrun_fmt_hdr_fwrite(f,
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
                        NULL);
  return f;
}


int
hpcrun_write_stream_profile_data(core_profile_trace_data_t *st)
{
	//printf("\nABOUT TO WRITE THE STREAM PROFILE DATA");
  TMSG(DATA_WRITE,"Writing stream hpcrun profile data");
  FILE* f = open_stream_data_file(st);
  if (f == NULL)
    return HPCRUN_ERR;

  write_epochs(f, st->epoch);

  TMSG(DATA_WRITE,"closing file");
  hpcio_fclose(f);
  TMSG(DATA_WRITE,"Done!");

  return HPCRUN_OK;
}
