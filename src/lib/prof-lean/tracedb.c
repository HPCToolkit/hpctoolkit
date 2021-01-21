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
// Copyright ((c)) 2002-2020, Rice University
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

//***************************************************************************
//
// Purpose:
//   Low-level types and functions for reading/writing thread.db
//
//   See thread.db figure. //TODO change this
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "hpcio.h"
#include "hpcio-buffer.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"
#include "tracedb.h"


//***************************************************************************

//***************************************************************************
// tracedb hdr
//***************************************************************************
int 
tracedb_hdr_fwrite(tracedb_hdr_t* hdr,FILE* fs)
{
  fwrite(HPCTRACEDB_FMT_Magic, 1, HPCTRACEDB_FMT_MagicLen,   fs);
  uint8_t versionMajor = HPCTRACEDB_FMT_VersionMajor;
  uint8_t versionMinor = HPCTRACEDB_FMT_VersionMinor;
  fwrite(&versionMajor, 1, 1, fs);
  fwrite(&versionMinor, 1, 1, fs);

  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(hdr->num_trace, fs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(HPCTRACEDB_FMT_NumSec, fs));

  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->trace_hdr_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(HPCTRACEDB_FMT_HeaderLen, fs));

  return HPCFMT_OK;
}

int
tracedb_hdr_fread(tracedb_hdr_t* hdr, FILE* infs)
{
  char tag[HPCTRACEDB_FMT_MagicLen + 1];

  int nr = fread(tag, 1, HPCTRACEDB_FMT_MagicLen, infs);
  tag[HPCTRACEDB_FMT_MagicLen] = '\0';

  if (nr != HPCTRACEDB_FMT_MagicLen) {
    return HPCFMT_ERR;
  }
  if (strcmp(tag, HPCTRACEDB_FMT_Magic) != 0) {
    return HPCFMT_ERR;
  }

  nr = fread(&hdr->versionMajor, 1, 1, infs);
  if (nr != 1) {
    return HPCFMT_ERR;
  }
  nr = fread(&hdr->versionMinor, 1, 1, infs);
  if (nr != 1) {
    return HPCFMT_ERR;
  }

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(hdr->num_trace), infs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(hdr->num_sec), infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(hdr->trace_hdr_sec_size), infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(hdr->trace_hdr_sec_ptr), infs));

  return HPCFMT_OK;
}

int
tracedb_hdr_fprint(tracedb_hdr_t* hdr, FILE* fs)
{
  fprintf(fs, "%s\n", HPCTRACEDB_FMT_Magic);

  fprintf(fs, "[hdr:\n");
  fprintf(fs, "  (version: %d.%d)\n", hdr->versionMajor, hdr->versionMinor);
  fprintf(fs, "  (num_trace: %d)\n", hdr->num_trace);
  fprintf(fs, "  (num_sec: %d)\n", hdr->num_sec);
  fprintf(fs, "  (trace_hdr_sec_size: %ld)\n", hdr->trace_hdr_sec_size);
  fprintf(fs, "  (trace_hdr_sec_ptr: %ld)\n", hdr->trace_hdr_sec_ptr);
  fprintf(fs, "]\n");

  return HPCFMT_OK;
}

//***************************************************************************
// trace_hdr_t
//***************************************************************************
int 
trace_hdr_fwrite(trace_hdr_t x, FILE* fs)
{
  
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(x.prof_info_idx, fs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(x.trace_idx, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x.start, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(x.end, fs));
 
  return HPCFMT_OK;
}

int 
trace_hdr_fread(trace_hdr_t* x, FILE* fs)
{

  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&(x->prof_info_idx), fs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&(x->trace_idx), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->start), fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&(x->end), fs));

  return HPCFMT_OK;
}

int 
trace_hdr_fprint(trace_hdr_t x, int i, FILE* fs)
{

  fprintf(fs,"  %d[(prof_info_idx: %d) (trace_idx: %d) (start: %ld) (end: %ld)]\n", 
    i, x.prof_info_idx, x.trace_idx, x.start, x.end);

  return HPCFMT_OK;
}



int 
trace_hdrs_fwrite(uint32_t num_t,trace_hdr_t* x, FILE* fs)
{
  for (uint32_t i = 0; i < num_t; ++i) {
    trace_hdr_fwrite(x[i], fs);
  }
  return HPCFMT_OK;
}

int 
trace_hdrs_fread(trace_hdr_t** x, uint32_t num_t,FILE* fs)
{
  trace_hdr_t * trace_hdrs = (trace_hdr_t *) malloc(num_t * sizeof(trace_hdr_t));

  for (uint32_t i = 0; i < num_t; ++i) {
    trace_hdr_fread(&(trace_hdrs[i]), fs);
  }

  *x = trace_hdrs;
  return HPCFMT_OK;
}

int 
trace_hdrs_fprint(uint32_t num_t,trace_hdr_t* x, FILE* fs)
{
  fprintf(fs,"[Trace hdrs for %d traces\n", num_t);

  for (uint32_t i = 0; i < num_t; ++i) {
    trace_hdr_fprint(x[i], i, fs);
  }
  fprintf(fs,"]\n");
  return HPCFMT_OK;
}

void 
trace_hdrs_free(trace_hdr_t** x)
{
  free(*x);
  *x = NULL;
}


//***************************************************************************
// hpctrace_fmt_datum_t in tracedb (for one profile's trace)
//***************************************************************************
int
tracedb_data_fread(hpctrace_fmt_datum_t** x, uint64_t num_datum, hpctrace_hdr_flags_t flags,FILE* fs)
{
  *x = (hpctrace_fmt_datum_t *) malloc(num_datum * sizeof(hpctrace_fmt_datum_t));
  
  for (uint64_t i = 0; i < num_datum; ++i) {
    hpctrace_fmt_datum_fread(*x+i, flags, fs);
  }

  //*x = trace_data;
  return HPCFMT_OK;

}

int
tracedb_data_fprint(hpctrace_fmt_datum_t* x, uint64_t num_datum, uint32_t prof_info_idx, hpctrace_hdr_flags_t flags,FILE* fs)
{
  fprintf(fs,"[Trace %d:\n", prof_info_idx);

  for (uint64_t i = 0; i < num_datum; ++i) {
    printf("  ");
    hpctrace_fmt_datum_fprint(&(x[i]), flags, fs);
  }
  fprintf(fs,"]\n");
  return HPCFMT_OK;
}

void
tracedb_data_free(hpctrace_fmt_datum_t** x)
{
  free(*x);
  *x = NULL;
}