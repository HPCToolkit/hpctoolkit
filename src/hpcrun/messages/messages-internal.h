// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef pmsg_i
#define pmsg_i

//*****************************************************************************
// File: pmsg.i
//
// Description:
//   definitions shared between the synchronous and asynchronous halves of the
//   messaging system.
//
// History:
//   19 July 2009
//     partition pmsg.c into messages-sync.c and messages-async.c c
//
//*****************************************************************************



//*****************************************************************************
// global includes
//*****************************************************************************

#include "../../common/lean/spinlock.h"



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../common/lean/spinlock.h"
#include "fmt.h"


//*****************************************************************************
// macros
//*****************************************************************************

#define MSG_BUF_SIZE  4096



//*****************************************************************************
// global variables
//*****************************************************************************

extern spinlock_t pmsg_lock;



//*****************************************************************************
// interface functions (within messages subsystem)
//*****************************************************************************

void hpcrun_write_msg_to_log(bool echo_stderr, bool add_thread_id,
                             const char *tag, const char *fmt, va_list_box* box);


#endif
