// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_CALLBACK_H__
#define __OMPT_CALLBACK_H__


//*****************************************************************************
//  macros
//*****************************************************************************

#define ompt_event_may_occur(r) \
  ((r == ompt_set_sometimes) | \
   (r == ompt_set_sometimes_paired) | \
   (r == ompt_set_always))

#endif
