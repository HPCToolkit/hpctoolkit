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
// Copyright ((c)) 2002-2019, Rice University
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

//************************* System Include Files ****************************

#include <xed-interface.h>
#include <stdint.h>

//*************************** configuration ****************************

#include <include/hpctoolkit-config.h>

//*************************** User Include Files ****************************

#include "x86-unwind-analysis.h"
#include "x86-build-intervals.h"
#include "x86-decoder.h"
#include "fnbounds_interface.h"

static void
x86_print_intervals(btuwi_status_t intervals)
{
  unwind_interval *u;
  for(u = intervals.first; u; u = UWI_NEXT(u)) {
    dump_ui_dbg(u);
  }
}

static void
x86_dump_intervals(void *addr, int noisy)
{
  unwindr_info_t unwr_info;
  if (!uw_recipe_map_lookup(addr, NATIVE_UNWINDER, &unwr_info))
	  EMSG("x86_dump_intervals: bounds of addr %p taken, but no bounds known", addr);
  void * s = (void*)unwr_info.interval.start;
  void * e = (void*)unwr_info.interval.end;

  btuwi_status_t intervals;
  intervals = x86_build_intervals(s, e - s, noisy);
  x86_print_intervals(intervals);
}

void
hpcrun_dump_intervals(void* addr)
{
  x86_dump_intervals(addr, 0);
}

void
hpcrun_dump_intervals_noisy(void* addr)
{
  x86_dump_intervals(addr, 1);
}

void
x86_dump_ins(void *ins)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  char inst_buf[1024];
  char errbuf[2048];

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);
  
  if (xed_error == XED_ERROR_NONE) {
    xed_decoded_inst_dump(xptr, inst_buf, sizeof(inst_buf));
    sprintf(errbuf, "(%p, %d bytes, %s) %s \n" , ins, 
	    xed_decoded_inst_get_length(xptr), 
	    xed_iclass_enum_t2str(iclass(xptr)), inst_buf);
  }
  else {
#if defined(ENABLE_XOP) && defined (HOST_CPU_x86_64)
    amd_decode_t decode_res;
    adv_amd_decode(&decode_res, ins);
    if (decode_res.success) {
      if (decode_res.weak)
	sprintf(errbuf, "(%p, %d bytes) weak AMD XOP \n", ins, (int) decode_res.len);
      else
	sprintf(errbuf, "(%p, %d bytes) robust AMD XOP \n", ins, (int) decode_res.len);
    }
    else
#endif // ENABLE_XOP and HOST_CPU_x86_64
      sprintf(errbuf, "x86_dump_ins: xed decode error addr=%p, code = %d\n", 
	      ins, (int) xed_error);
  }
  EMSG(errbuf);
  fprintf(stderr, errbuf);
  fflush(stderr);
}
