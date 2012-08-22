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
// Copyright ((c)) 2002-2012, Rice University
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
#include <limits.h>



//*********************************************************************
// local includes 
//*********************************************************************

#include "env.h"
#include "files.h"
#include "monitor.h"
#include "rank.h"
#include "string.h"
#include "trace.h"
#include "thread_data.h"
#include "sample_prob.h"

#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcio-buffer.h>


//*********************************************************************
// type declarations
//*********************************************************************
typedef struct gpu_trace_file_ptr {
		int gpu_number;
		int stream_id;
  	void* trace_buffer;
  	hpcio_outbuf_t trace_outbuf;
		int file_id; 
} gpu_trace_file_t;




//*********************************************************************
// forward declarations 
//*********************************************************************

static void hpcrun_trace_file_validate(int valid, char *op);



//*********************************************************************
// local variables 
//*********************************************************************

static int tracing = 0;

static gpu_trace_file_t *gpu_trace_file_array;
#define GPU_TRACE_FILE_OFFSET 0
// TODO: just use one #define from gpu_blame.h
#define MAX_STREAMS (500)
#define NUM_GPU_DEVICES 2
//*********************************************************************
// interface operations
//*********************************************************************

int
hpcrun_trace_isactive()
{
  return tracing;
}


void
hpcrun_trace_init()
{
  if (getenv(HPCRUN_TRACE)) {
					tracing = 1;
					gpu_trace_file_array = (gpu_trace_file_t *)hpcrun_malloc(sizeof(gpu_trace_file_t)*MAX_STREAMS*NUM_GPU_DEVICES);
					memset(gpu_trace_file_array, 0, sizeof(gpu_trace_file_t)*MAX_STREAMS*NUM_GPU_DEVICES);
  }
}


void
hpcrun_trace_open()
{
  // With fractional sampling, if this process is inactive, then don't
  // open an output file, not even /dev/null.
  if (tracing && hpcrun_sample_prob_active()) {
    thread_data_t *td = hpcrun_get_thread_data();
	
		    int fd, ret;

    // I think unlocked is ok here (we don't overlap any system
    // locks).  At any rate, locks only protect against threads, they
    // don't help with signal handlers (that's much harder).
    fd = hpcrun_open_trace_file(td->id);
    hpcrun_trace_file_validate(fd >= 0, "open");
    td->trace_buffer = hpcrun_malloc(HPCRUN_TraceBufferSz);
    ret = hpcio_outbuf_attach(&td->trace_outbuf, fd, td->trace_buffer,
			      HPCRUN_TraceBufferSz, HPCIO_OUTBUF_UNLOCKED);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "open");

    ret = hpctrace_fmt_hdr_outbuf(&td->trace_outbuf);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "write header to");
  }
}

int
hpcrun_stream_open_trace_file(int thread)
{
  int ret;

  spinlock_lock(&files_lock);
  hpcrun_files_init();
	/*FIXME : you are passsing 0 everytime? */
	int rank = hpcrun_get_rank();
	if(rank < 0) {
		rank = 0;
	}
  ret = hpcrun_open_file(rank, thread, HPCRUN_TraceFnmSfx, FILES_EARLY);
  spinlock_unlock(&files_lock);

  return ret;
}



void 
gpu_trace_open(stream_data_t *st, int gpu_device_num, int stream_num)
{
  // With fractional sampling, if this process is inactive, then don't
  // open an output file, not even /dev/null.
  if (tracing && hpcrun_sample_prob_active()) {
    //thread_data_t *td = hpcrun_get_thread_data();
		/*
 		* instead of getting the thread data, we have to get the gpu_trace_file
		* contents associated with the above gpu_device_num and stream_num
		*/
		
    int fd, ret;
		uint32_t id = ((gpu_device_num) + stream_num); 
    // I think unlocked is ok here (we don't overlap any system
    // locks).  At any rate, locks only protect against threads, they
    // don't help with signal handlers (that's much harder).
    fd = hpcrun_stream_open_trace_file(id + GPU_TRACE_FILE_OFFSET);
    hpcrun_trace_file_validate(fd >= 0, "open");
    gpu_trace_file_array[id].trace_buffer = hpcrun_malloc(HPCRUN_TraceBufferSz);
//printf("\nmallocced trace_buffer\n");
    ret = hpcio_outbuf_attach(&(gpu_trace_file_array[id].trace_outbuf), fd, gpu_trace_file_array[id].trace_buffer,
			      HPCRUN_TraceBufferSz, HPCIO_OUTBUF_UNLOCKED);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "open");

    ret = hpctrace_fmt_hdr_outbuf(&(gpu_trace_file_array[id].trace_outbuf));
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "write header to");
		if(st->trace_min_time_us == 0) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			st->trace_min_time_us = ((uint64_t)tv.tv_usec
                                                 + (((uint64_t)tv.tv_sec) * 1000000));
		}
  }
}

void
gpu_trace_append_with_time(stream_data_t *st, int gpu_device_num, int stream_num, unsigned int call_path_id, uint64_t microtime)
{
	if (tracing && hpcrun_sample_prob_active()) {
		uint32_t id = (gpu_device_num*10)+stream_num;



    hpctrace_fmt_datum_t trace_datum;
    trace_datum.time = microtime;
    trace_datum.cpId = (uint32_t)call_path_id;
    int ret = hpctrace_fmt_datum_outbuf(&trace_datum, &(gpu_trace_file_array[id].trace_outbuf));
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "append");

		if(st->trace_max_time_us < microtime) {
			st->trace_max_time_us = microtime;
		}
	}
}

uint64_t
gpu_trace_append(stream_data_t *st, int gpu_device_num, int stream_num, unsigned int call_path_id)
{
	//call_path_id = 140;
	if (tracing && hpcrun_sample_prob_active()) {
		struct timeval tv;
		int ret = gettimeofday(&tv, NULL);
		assert(ret == 0 && "in trace_append: gettimeofday failed!");
    uint64_t microtime = ((uint64_t)tv.tv_usec
				+ (((uint64_t)tv.tv_sec) * 1000000));

		if(st->trace_max_time_us < microtime) {
      st->trace_max_time_us = microtime;
    }
		//printf("\nappending gpu trace");
		uint32_t id = (gpu_device_num*10)+stream_num;

    hpctrace_fmt_datum_t trace_datum;
    trace_datum.time = microtime;
    trace_datum.cpId = (uint32_t)call_path_id;
    ret = hpctrace_fmt_datum_outbuf(&trace_datum, &(gpu_trace_file_array[id].trace_outbuf));
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "append");


		return microtime;
	}
	return 0;
}



void
hpcrun_trace_append(unsigned int call_path_id)
{
  if (tracing && hpcrun_sample_prob_active()) {
    struct timeval tv;
    int ret = gettimeofday(&tv, NULL);
    assert(ret == 0 && "in trace_append: gettimeofday failed!");
    uint64_t microtime = ((uint64_t)tv.tv_usec
			  + (((uint64_t)tv.tv_sec) * 1000000));

    thread_data_t *td = hpcrun_get_thread_data();

    if (td->trace_min_time_us == 0) {
      td->trace_min_time_us = microtime;
    }
    td->trace_max_time_us = microtime;

    hpctrace_fmt_datum_t trace_datum;
    trace_datum.time = microtime;
    trace_datum.cpId = (uint32_t)call_path_id;
    ret = hpctrace_fmt_datum_outbuf(&trace_datum, &td->trace_outbuf);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "append");
  }

}


void
gpu_trace_close(stream_data_t *st, int gpu_number, int stream_id)
{
  if (tracing && hpcrun_sample_prob_active()) {
    //thread_data_t *td = hpcrun_get_thread_data();

		uint32_t id = (gpu_number * 10) + stream_id;
    int ret = hpcio_outbuf_close(&(gpu_trace_file_array[id].trace_outbuf));
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "close");

/*
 * we need to redo this renaming as john discussed.
 * for now lets manually rename

*/
    //int rank = hpcrun_get_rank();
    //if (rank >= 0) {
      //hpcrun_rename_trace_file(0, id+GPU_TRACE_FILE_OFFSET);
    //}
  }
}



void
hpcrun_trace_close()
{
  if (tracing && hpcrun_sample_prob_active()) {
    thread_data_t *td = hpcrun_get_thread_data();

    int ret = hpcio_outbuf_close(&td->trace_outbuf);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "close");

    int rank = hpcrun_get_rank();
    if (rank >= 0) {
      hpcrun_rename_trace_file(rank, td->id);
    }
  }
}


//*********************************************************************
// private operations
//*********************************************************************


static void
hpcrun_trace_file_validate(int valid, char *op)
{
  if (!valid) {
    EMSG("unable to %s trace file\n", op);
    monitor_real_abort();
  }
}
