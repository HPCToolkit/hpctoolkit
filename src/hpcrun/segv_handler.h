// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef SEGV_HANDLER_H
#define SEGV_HANDLER_H

typedef void (*hpcrun_sig_callback_t) (void);

int
hpcrun_segv_register_cb( hpcrun_sig_callback_t cb );

int
hpcrun_setup_segv();

#endif
