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
//   See thread.db figure.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef TRACEDB_FMT_H
#define TRACEDB_FMT_H

//************************* System Include Files ****************************

#include <stdbool.h>
#include <limits.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "hpcio.h"
#include "hpcio-buffer.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// tracedb hdr
//***************************************************************************
#define HPCTRACEDB_FMT_Magic   "HPCPROF-tracedb_" //16 bytes
#define HPCTRACEDB_FMT_VersionMajor 1             //1  byte
#define HPCTRACEDB_FMT_VersionMinor 0             //1  byte
#define HPCTRACEDB_FMT_NumSec       1             //2  byte

#define HPCTRACEDB_FMT_MagicLen     (sizeof(HPCTRACEDB_FMT_Magic) - 1)
#define HPCTRACEDB_FMT_VersionLen   2 
#define HPCTRACEDB_FMT_NumTraceLen  4
#define HPCTRACEDB_FMT_NumSecLen    2  
#define HPCTRACEDB_FMT_SecSizeLen   8
#define HPCTRACEDB_FMT_SecPtrLen    8
#define HPCTRACEDB_FMT_SecLen       (HPCTRACEDB_FMT_SecSizeLen + HPCTRACEDB_FMT_SecPtrLen)

#define HPCTRACEDB_FMT_Real_HeaderLen  (HPCTRACEDB_FMT_MagicLen + HPCTRACEDB_FMT_VersionLen \
  + HPCTRACEDB_FMT_NumTraceLen + HPCTRACEDB_FMT_NumSecLen \
  + HPCTRACEDB_FMT_SecLen * HPCTRACEDB_FMT_NumSec)
#define HPCTRACEDB_FMT_HeaderLen    128

typedef struct tracedb_hdr_t{
  uint8_t versionMajor;
  uint8_t versionMinor;
  uint32_t num_trace;
  uint16_t num_sec; //number of sections include hdr and traces section

  uint64_t trace_hdr_sec_size;
  uint64_t trace_hdr_sec_ptr;
}tracedb_hdr_t;

int 
tracedb_hdr_fwrite(tracedb_hdr_t* hdr, FILE* fs);

char*
tracedb_hdr_swrite(tracedb_hdr_t* hdr, char* buf);

int
tracedb_hdr_fread(tracedb_hdr_t* hdr, FILE* infs);

int
tracedb_hdr_fprint(tracedb_hdr_t* hdr, FILE* fs);

//***************************************************************************
// trace_hdr_t
//***************************************************************************
#define trace_hdr_SIZE 22

typedef struct trace_hdr_t{
  uint32_t prof_info_idx; 
  uint16_t trace_idx;
  uint64_t start;
  uint64_t end;
}trace_hdr_t;


int 
trace_hdr_fwrite(trace_hdr_t x, FILE* fs);

char*
trace_hdr_swrite(trace_hdr_t x, char* buf);

int
trace_hdr_fread(trace_hdr_t* x, FILE* fs);

int 
trace_hdr_fprint(trace_hdr_t x, int i, FILE* fs);




int 
trace_hdrs_fwrite(uint32_t num_t,trace_hdr_t* x, FILE* fs);

int 
trace_hdrs_fread(trace_hdr_t** x, uint32_t num_t,FILE* fs);

int 
trace_hdrs_fprint(uint32_t num_t,trace_hdr_t* x, FILE* fs);

void 
trace_hdrs_free(trace_hdr_t** x);

//***************************************************************************
// hpctrace_fmt_datum_t in tracedb
//***************************************************************************
#define timepoint_SIZE 12

int
tracedb_data_fread(hpctrace_fmt_datum_t** x, uint64_t num_datum, hpctrace_hdr_flags_t flags,FILE* fs);

int
tracedb_data_fprint(hpctrace_fmt_datum_t* x, uint64_t num_datum, uint32_t prof_info_idx, 
                    hpctrace_hdr_flags_t flags,FILE* fs);

void
tracedb_data_free(hpctrace_fmt_datum_t** x);


//***************************************************************************
// footer
//***************************************************************************
#define TRACDBft 0x5452414344426674

//***************************************************************************
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //TRACE_FMT_H
