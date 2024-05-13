// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_DEFER_WRITE_H__
#define __OMPT_DEFER_WRITE_H__


//******************************************************************************
// local includes
//******************************************************************************

#include "../thread_data.h"



//******************************************************************************
// type declarations
//******************************************************************************

// type definition
struct entry_t {
  thread_data_t *td;
  struct entry_t *prev;
  struct entry_t *next;
  bool flag;
};



//******************************************************************************
// interface functions
//******************************************************************************

// operations
struct entry_t *
new_dw_entry
(
 void
);


void
insert_dw_entry
(
 struct entry_t* entry
);


void
add_defer_td
(
 thread_data_t *td
);


void
write_other_td
(
 void
);

#endif
