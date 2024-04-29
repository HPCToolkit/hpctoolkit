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
// Copyright ((c)) 2002-2024, Rice University
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
