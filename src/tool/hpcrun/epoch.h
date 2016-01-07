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

#ifndef EPOCH_H
#define EPOCH_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <cct/cct.h>
#include <cct/cct_bundle.h>
#include "loadmap.h"

#include <lush/lush.h>

#include <messages/messages.h>

//*************************** Datatypes **************************

typedef struct epoch_t {
  cct_bundle_t csdata;     // cct (call stack data)
  cct_ctxt_t* csdata_ctxt; // creation context
  hpcrun_loadmap_t* loadmap;
  struct epoch_t* next;    // epochs gathererd into a (singly) linked list

} epoch_t;

typedef epoch_t* epoch_t_f(void);

typedef void epoch_t_setter(epoch_t* s);

extern void hpcrun_reset_epoch(epoch_t* epoch);

epoch_t* hpcrun_check_for_new_loadmap(epoch_t *);
void hpcrun_epoch_init(cct_ctxt_t* ctxt);
void hpcrun_epoch_reset(void);

#endif // EPOCH_H
