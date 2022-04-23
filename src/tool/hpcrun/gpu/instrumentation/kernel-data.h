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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef gpu_instrumentation_kernel_data_h
#define gpu_instrumentation_kernel_data_h

#include <gtpin.h>

typedef enum { KERNEL_DATA_GTPIN } kernel_data_kind_t;

typedef struct kernel_data_gtpin_inst {
  int32_t offset;
  struct kernel_data_gtpin_inst* next;
} kernel_data_gtpin_inst_t;

typedef struct kernel_data_gtpin_block {
  int32_t head_offset;
  int32_t tail_offset;
  GTPinMem mem;
  struct kernel_data_gtpin_inst* inst;
  struct kernel_data_gtpin_block* next;
} kernel_data_gtpin_block_t;

typedef struct kernel_data_gtpin {
  uint64_t kernel_id;
  struct kernel_data_gtpin_block* block;
} kernel_data_gtpin_t;

typedef struct kernel_data {
  uint32_t loadmap_module_id;
  kernel_data_kind_t kind;
  void* data;
} kernel_data_t;

#endif
