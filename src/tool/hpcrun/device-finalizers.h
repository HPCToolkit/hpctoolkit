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

#ifndef _HPCTOOLKIT_DEVICE_FINALIZERS_H_
#define _HPCTOOLKIT_DEVICE_FINALIZERS_H_

typedef enum {
  device_finalizer_type_flush,
  device_finalizer_type_shutdown
} device_finalizer_type_t;

typedef void (*device_finalizer_fn_t)(void* args, int how);

typedef struct device_finalizer_fn_entry_t {
  struct device_finalizer_fn_entry_t* next;
  device_finalizer_fn_t fn;
  void* args;
} device_finalizer_fn_entry_t;

// note finalizers in the order they are registered
extern void
device_finalizer_register(device_finalizer_type_t type, device_finalizer_fn_entry_t* entry);

// apply finalizers in the order they are registered
extern void device_finalizer_apply(device_finalizer_type_t type, int how);

#endif
