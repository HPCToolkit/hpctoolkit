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

//*********************************************************************
// global includes 
//*********************************************************************

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>




//*********************************************************************
// local includes 
//*********************************************************************

#include "env.h"
#include "files.h"
#include "monitor.h"
#include "trace.h"
#include "thread_data.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcio.h>


//*********************************************************************
// type declarations
//*********************************************************************

typedef struct trecord_s {
  double time;
  unsigned int call_path_id;
} trecord_t;

#define TRECORD_SIZE (sizeof(double) + sizeof(unsigned int))

//*********************************************************************
// forward declarations 
//*********************************************************************

static void trace_file_validate(int valid, char *op);



//*********************************************************************
// local variables 
//*********************************************************************

static int tracing = 0;



//*********************************************************************
// interface operations
//*********************************************************************

int 
trace_isactive()
{
  return tracing;
}


void 
trace_init()
{
  if (getenv(CSPROF_OPT_TRACE)) {
    tracing = 1;
  }
}


void 
trace_open()
{
  if (tracing) {
    char trace_file[PATH_MAX];
    files_trace_name(trace_file, 0, PATH_MAX);
    thread_data_t *td = hpcrun_get_thread_data();
    td->trace_file = hpcio_fopen_w(trace_file, 0);
    trace_file_validate(td->trace_file != 0, "open");
  }
}


void
trace_append(unsigned int call_path_id)
{
  struct {
    union {
      double dbl;
      uint64_t ui64;
    } data;
  } microtime;

  if (tracing) {
    struct timeval tv;
    int notime = gettimeofday(&tv, NULL);
    assert(notime == 0 && "in trace_append: gettimeofday failed!"); 
    microtime.data.dbl = tv.tv_usec + tv.tv_sec * 1000000;

    thread_data_t *td = hpcrun_get_thread_data();

    int written = hpcio_fwrite_be8(&microtime.data.ui64, td->trace_file);
    written += hpcio_fwrite_be4((uint32_t*)&call_path_id, td->trace_file);
    trace_file_validate(written == (sizeof(double) + sizeof(int)), "append");
  }
}


void
trace_close()
{
  if (tracing) {
    int ret;
    thread_data_t *td = hpcrun_get_thread_data();
    ret = fflush(td->trace_file);
    trace_file_validate(ret == 0, "flush");

    ret = hpcio_fclose(td->trace_file);
    trace_file_validate(ret == 0, "close");
    int rank = monitor_mpi_comm_rank();
    if (rank >= 0) {
      char old[PATH_MAX];
      char new[PATH_MAX];
      files_trace_name(old, 0, PATH_MAX);
      files_trace_name(new, rank, PATH_MAX);
      rename(old, new);
    }
  }
}



//*********************************************************************
// private operations
//*********************************************************************


static void
trace_file_validate(int valid, char *op)
{
  if (!valid) {
    EMSG("unable to %s trace file\n", op);
    monitor_real_abort();
  }
}
