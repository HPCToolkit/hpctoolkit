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
// Copyright ((c)) 2002-2019, Rice University
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

#ifndef PTHREAD_BLAME_H
#define PTHREAD_BLAME_H
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

#define PTHREAD_EVENT_NAME "PTHREAD_WAIT"

#define PTHREAD_BLAME_METRIC "PTHREAD_BLAME"
#define PTHREAD_BLOCKWAIT_METRIC "PTHREAD_BLOCK_WAIT"
#define PTHREAD_SPINWAIT_METRIC "PTHREAD_SPIN_WAIT"

// pthread blame shifting enabled
extern bool pthread_blame_lockwait_enabled(void);

//
// handling pthread blame
//
extern void pthread_directed_blame_shift_blocked_start(void* obj);
extern void pthread_directed_blame_shift_spin_start(void* obj);
extern void pthread_directed_blame_shift_end(void);
extern void pthread_directed_blame_accept(void* obj);

#endif // PTHREAD_BLAME_H
