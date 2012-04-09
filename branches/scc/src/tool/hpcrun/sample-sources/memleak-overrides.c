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



/******************************************************************************
 * standard include files
 *****************************************************************************/

#include <assert.h>
#include <ucontext.h>

/* definition for posix memalign */
#define _XOPEN_SOURCE 600
#include <stdlib.h>

/* definition for sysconf */
#include <unistd.h>

/* define rcce ops */
#include "/shared/xl10/rcce-latest/include/RCCE.h"


/******************************************************************************
 * local include files
 *****************************************************************************/

#include <sample-sources/memleak.h>
#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>
#include <lib/prof-lean/spinlock.h>



/******************************************************************************
 * type definitions
 *****************************************************************************/


typedef int RCCE_get_fcn(t_vcharp, t_vcharp, int, int);
typedef uint64_t hrtime_t;


/******************************************************************************
 * macros
 *****************************************************************************/
#define SKIP_MEM_FRAME 0
#define SKIP_MEMALIGN_HELPER_FRAME 1

#define DEBUG_WITH_CLEAR 0
#define ENABLE_MEMLEAK_CHECKING 1

#define real_RCCE_get   __real_RCCE_get

extern RCCE_get_fcn real_RCCE_get;


static int rcce_enabled = 0; // default is off
static int rcce_uninit = 1;  // default is uninitialized



/******************************************************************************
 * private data
 *****************************************************************************/




/******************************************************************************
 * private operations
 *****************************************************************************/

/*** important: use rdtsc, one should use -O2 instead of -O3 to compile hpcrun ***/
/* I cannot use this function because the function call is costly */
#if 0
hrtime_t gethrcycle_x86()
{
  unsigned int tmp[2];

  __asm__( "rdtsc"
     : "=a" (tmp[1]), "=d" (tmp[0])
     : "c" (0x10) );

  return ( ((hrtime_t)tmp[0] << 32 | tmp[1]) );
}
#endif

static void 
rcce_initialize(void)
{
  rcce_uninit = 0;
  rcce_enabled = 1; // unconditionally enable leak detection for now
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

int
MONITOR_EXT_WRAP_NAME(RCCE_get)(t_vcharp target, t_vcharp source, int count, int ID)
{
  	hrtime_t tsc1, tsc2;
	unsigned int tmp[2];
	cct_node_t *node;
	ucontext_t uc;
	getcontext(&uc);
	hpcrun_async_block();
#define RETCNT_SKIP_FRAME_BUG_FIXED 0 // broken at present -- johnmc
#if RETCNT_SKIP_FRAME_BUG_FIXED
	node = hpcrun_sample_callpath(&uc, hpcrun_rcce_receive_id(), count,
				frames_to_skip, 1);
#else
	node = hpcrun_sample_callpath(&uc, hpcrun_rcce_receive_id(), count, 0, 1);
#endif
	__asm__( "rdtsc"
			: "=a" (tmp[1]), "=d" (tmp[0])
			: "c" (0x10) );

	tsc1= ((hrtime_t)tmp[0] << 32 | tmp[1]);
	hpcrun_async_unblock();

	int ret = real_RCCE_get(target, source, count, ID);

	hpcrun_async_block();
	__asm__( "rdtsc"
			: "=a" (tmp[1]), "=d" (tmp[0])
			: "c" (0x10) );

	tsc2= ((hrtime_t)tmp[0] << 32 | tmp[1]);
	hpcrun_receive_tsc_inc(node, tsc2-tsc1);
	hpcrun_receive_freq_inc(node, 1);
	hpcrun_async_unblock();
	return ret;
}
