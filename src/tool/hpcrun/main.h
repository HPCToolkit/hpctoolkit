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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef main_h
#define main_h

#include <setjmp.h>
#include <stdbool.h>

extern bool hpcrun_is_initialized();

extern bool hpcrun_is_safe_to_sync(const char* fn);
extern void hpcrun_set_safe_to_sync(void);

//
// fetch the full path of the execname
//
extern char* hpcrun_get_execname(void);

typedef void siglongjmp_fcn(sigjmp_buf, int);

siglongjmp_fcn *hpcrun_get_real_siglongjmp(void);

typedef struct hpcrun_aux_cleanup_t  hpcrun_aux_cleanup_t;

hpcrun_aux_cleanup_t * hpcrun_process_aux_cleanup_add( void (*func) (void *), void * arg);
void hpcrun_process_aux_cleanup_remove(hpcrun_aux_cleanup_t * node);

// ** HACK to accomodate PAPI-C w cuda component & gpu blame shifting

extern void special_cuda_ctxt_actions(bool enable);

#endif  // ! main_h
