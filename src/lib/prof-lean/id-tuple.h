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
//   Low-level types and functions for reading/writing id_tuples (each represent a unique profile)
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ID_TUPLE_H
#define ID_TUPLE_H

//************************* System Include Files ****************************

#include <stdbool.h>
#include <limits.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "../prof-lean/hpcio.h"
#include "../prof-lean/hpcio-buffer.h"
#include "../prof-lean/hpcfmt.h"
#include "../prof-lean/hpcrun-fmt.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
#define IDTUPLE_SUMMARY   (uint16_t)0
#define IDTUPLE_NODE      (uint16_t)1
#define IDTUPLE_RANK      (uint16_t)2
#define IDTUPLE_THREAD    (uint16_t)3
#define IDTUPLE_GPUDEVICE (uint16_t)4
#define IDTUPLE_GPUSTREAM (uint16_t)5

#define TMS_id_tuple_len_SIZE  2
#define TMS_id_SIZE            10

typedef struct tms_id_t{
  uint16_t kind;
  uint64_t index;
}tms_id_t;

typedef struct tms_id_tuple_t{
  uint16_t length;
  uint32_t rank; //rank that read/write this profile
  tms_id_t* ids;

  uint32_t prof_info_idx;
  uint32_t all_at_root_idx;
}tms_id_tuple_t;

char* kindStr(const uint16_t kind);

//***************************************************************************
// Single id_tuple
//***************************************************************************
int 
id_tuple_fwrite(tms_id_tuple_t* x, FILE* fs);

int 
id_tuple_fread(tms_id_tuple_t* x, FILE* fs);

int 
id_tuple_fprint(tms_id_tuple_t* x, FILE* fs);

void 
id_tuple_free(tms_id_tuple_t* x);


//***************************************************************************
// for thread.db (thread major sparse)
//***************************************************************************
int 
id_tuples_tms_fwrite(uint32_t num_tuples,tms_id_tuple_t* x, FILE* fs);

int 
id_tuples_tms_fread(tms_id_tuple_t** x, uint32_t num_tuples,FILE* fs);

int 
id_tuples_tms_fprint(uint32_t num_tuples,tms_id_tuple_t* x, FILE* fs);

void 
id_tuples_tms_free(tms_id_tuple_t** x, uint32_t num_tuples);

//***************************************************************************
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif //ID_TUPLE_H