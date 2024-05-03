// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//************************* System Include Files ****************************

#define _GNU_SOURCE

#if __has_include(<xed/xed-interface.h>)
#include <xed/xed-interface.h>
#else
#include <xed-interface.h>
#endif
#include <stdint.h>

//*************************** configuration ****************************


//*************************** User Include Files ****************************

#include "x86-unwind-analysis.h"
#include "x86-build-intervals.h"
#include "x86-decoder.h"
#include "../../fnbounds/fnbounds_interface.h"

#include "../common/unwindr_info.h"
#include "../common/uw_recipe_map.h"
#include "../../messages/messages.h"

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
  fprintf(stderr, "%s", errbuf);
  fflush(stderr);
}
