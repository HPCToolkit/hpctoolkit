// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef EPOCH_H
#define EPOCH_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "cct/cct.h"
#include "cct/cct_bundle.h"
#include "loadmap.h"

#include "lush/lush.h"

#include "messages/messages.h"

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
