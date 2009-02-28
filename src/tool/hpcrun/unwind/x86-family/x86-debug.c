#include "x86-unwind-analysis.h"
#include "x86-build-intervals.h"
#include "x86-decoder.h"
#include "fnbounds_interface.h"

void x86_dump_intervals(char  *addr) 
{
  void *s, *e;
  unwind_interval *u;
  interval_status intervals;

  fnbounds_enclosing_addr(addr, &s, &e);

  intervals = x86_build_intervals(s, e - s, 0);

  for(u = (unwind_interval *)intervals.first; u; u = (unwind_interval *)(u->common).next) {
    dump_ui(u, 1);
  }
}


void x86_dump_ins(void *ins)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  char inst_buf[1024];
  char errbuf[2048];

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

  xed_format_xed(xptr, inst_buf, sizeof(inst_buf), (uint64_t) ins);
  sprintf(errbuf, "(%p, %d bytes, %s) %s \n" , ins, xed_decoded_inst_get_length(xptr), 
	 xed_iclass_enum_t2str(iclass(xptr)), inst_buf);

  EMSG(errbuf);
  fprintf(stderr, errbuf);
  fflush(stderr);
}

