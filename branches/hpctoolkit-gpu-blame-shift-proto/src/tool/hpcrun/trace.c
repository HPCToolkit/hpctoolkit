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

static void trace_file_validate(int valid, char *op);



//*********************************************************************
// local variables 
//*********************************************************************

static int tracing = 1;

static gpu_trace_file_t *gpu_trace_file_array;
#define GPU_TRACE_FILE_OFFSET 2
#define MAX_STREAMS 100
#define NUM_GPU_DEVICES 2
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
  if (getenv(HPCRUN_TRACE)) {
					tracing = 1;
					gpu_trace_file_array = (gpu_trace_file_t *)hpcrun_malloc(sizeof(gpu_trace_file_t)*MAX_STREAMS*NUM_GPU_DEVICES);
					memset(gpu_trace_file_array, 0, sizeof(gpu_trace_file_t)*MAX_STREAMS*NUM_GPU_DEVICES);
  }
}


void 
trace_open()
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
    trace_file_validate(fd >= 0, "open");
    td->trace_buffer = hpcrun_malloc(HPCRUN_TraceBufferSz);
    ret = hpcio_outbuf_attach(&td->trace_outbuf, fd, td->trace_buffer,
			      HPCRUN_TraceBufferSz, HPCIO_OUTBUF_UNLOCKED);
    trace_file_validate(ret == HPCFMT_OK, "open");

    ret = hpctrace_fmt_hdr_outbuf(&td->trace_outbuf);
    trace_file_validate(ret == HPCFMT_OK, "write header to");
  }
}

void 
gpu_trace_open(int gpu_device_num, int stream_num)
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
		uint32_t id = ((gpu_device_num * 10) + stream_num); 
    // I think unlocked is ok here (we don't overlap any system
    // locks).  At any rate, locks only protect against threads, they
    // don't help with signal handlers (that's much harder).
    fd = hpcrun_open_trace_file(id + GPU_TRACE_FILE_OFFSET);
printf("\nopened the trace file\n");
    trace_file_validate(fd >= 0, "open");
    gpu_trace_file_array[id].trace_buffer = hpcrun_malloc(HPCRUN_TraceBufferSz);
printf("\nmallocced trace_buffer\n");
    ret = hpcio_outbuf_attach(&(gpu_trace_file_array[id].trace_outbuf), fd, gpu_trace_file_array[id].trace_buffer,
			      HPCRUN_TraceBufferSz, HPCIO_OUTBUF_UNLOCKED);
    trace_file_validate(ret == HPCFMT_OK, "open");

    ret = hpctrace_fmt_hdr_outbuf(&(gpu_trace_file_array[id].trace_outbuf));
    trace_file_validate(ret == HPCFMT_OK, "write header to");
  }
}

void
gpu_trace_append_with_time(int gpu_device_num, int stream_num, unsigned int call_path_id, uint64_t microtime)
{

	if (tracing && hpcrun_sample_prob_active()) {

		uint32_t id = (gpu_device_num*10)+stream_num;
		int ret = hpctrace_fmt_append_outbuf(&(gpu_trace_file_array[id].trace_outbuf), microtime,
				(uint32_t)call_path_id);  
		//trace_file_validate(ret == HPCFMT_OK, "append");
	}
}

uint64_t
gpu_trace_append(int gpu_device_num, int stream_num, unsigned int call_path_id)
{
	//call_path_id = 140;
	if (tracing && hpcrun_sample_prob_active()) {
		struct timeval tv;
		int ret = gettimeofday(&tv, NULL);
		assert(ret == 0 && "in trace_append: gettimeofday failed!");
		uint64_t microtime = ((uint64_t)tv.tv_usec 
				+ (((uint64_t)tv.tv_sec) * 1000000));

		//printf("\nappending gpu trace");
		uint32_t id = (gpu_device_num*10)+stream_num;
		ret = hpctrace_fmt_append_outbuf(&(gpu_trace_file_array[id].trace_outbuf), microtime,
				(uint32_t)call_path_id);
		trace_file_validate(ret == HPCFMT_OK, "append");
		return microtime;
	}
	return 0;
}



void
trace_append(unsigned int call_path_id)
{
  if (tracing && hpcrun_sample_prob_active()) {
    thread_data_t *td = hpcrun_get_thread_data();
    struct timeval tv;
    int ret = gettimeofday(&tv, NULL);
    assert(ret == 0 && "in trace_append: gettimeofday failed!");
    uint64_t microtime = ((uint64_t)tv.tv_usec 
			  + (((uint64_t)tv.tv_sec) * 1000000));


    if (td->trace_min_time_us == 0) {
      td->trace_min_time_us = microtime;
    }
    td->trace_max_time_us = microtime;

    ret = hpctrace_fmt_append_outbuf(&td->trace_outbuf, microtime,
				     (uint32_t)call_path_id);
    trace_file_validate(ret == HPCFMT_OK, "append");
  }
}


void
gpu_trace_close(int gpu_number, int stream_id)
{
  if (tracing && hpcrun_sample_prob_active()) {
    //thread_data_t *td = hpcrun_get_thread_data();

		uint32_t id = (gpu_number * 10) + stream_id;
    int ret = hpcio_outbuf_close(&(gpu_trace_file_array[id].trace_outbuf));
    trace_file_validate(ret == HPCFMT_OK, "close");

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
trace_close()
{
  if (tracing && hpcrun_sample_prob_active()) {
    thread_data_t *td = hpcrun_get_thread_data();

    int ret = hpcio_outbuf_close(&td->trace_outbuf);
    trace_file_validate(ret == HPCFMT_OK, "close");

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
trace_file_validate(int valid, char *op)
{
  if (!valid) {
    EMSG("unable to %s trace file\n", op);
    monitor_real_abort();
  }
}
