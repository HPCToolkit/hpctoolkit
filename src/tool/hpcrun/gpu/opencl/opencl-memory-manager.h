// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

#ifndef OPENCL_MEMORY_MANAGER_H
#define OPENCL_MEMORY_MANAGER_H



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bistack.h>

#include "opencl-intercept.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum {
  OPENCL_KERNEL_CALLBACK                     = 0,
  OPENCL_MEMORY_CALLBACK                     = 1
} opencl_object_kind_t;


typedef struct opencl_object_channel_t opencl_object_channel_t;


typedef struct opencl_object_details_t {
  union {
    cl_kernel_callback_t ker_cb;
    cl_memory_callback_t mem_cb;
  };
} opencl_object_details_t;


typedef struct opencl_object_t {
  s_element_ptr_t next;
  opencl_object_channel_t *channel;
  opencl_object_kind_t kind;
  bool isInternalClEvent;
  opencl_object_details_t details;
} opencl_object_t;



//******************************************************************************
// interface operations
//******************************************************************************

opencl_object_t*
opencl_malloc
(
  void
);


void
opencl_free
(
  opencl_object_t *
);



#endif  //OPENCL_MEMORY_MANAGER_H
