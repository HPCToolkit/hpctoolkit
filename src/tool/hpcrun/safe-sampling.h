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

//***************************************************************************
//
// File:
// $HeadURL$
//
// Purpose:
// This file implements the "safe sampling" rules.
//
// Safe sampling extends the async blocks by tracking all entry and
// exit points in and out of the hpcrun code.  Every thread keeps an
// 'inside_hpcrun' bit in the thread data struct.  Set this bit via
// hpcrun_safe_enter() when entering hpcrun code and unset it via
// hpcrun_safe_exit() when returning to the application.  Thus, at
// every entry point, we know if we just came from inside our own
// code.
//
// A synchronous override that was called from inside our code should
// call the real function and return (see IO overrides).  An async
// interrupt that interrupted our code should restart the next signal
// (if needed) and return (see PAPI handler).  In both cases, if
// coming from inside our code, then don't take a sample and don't
// call MSG.
//
// Notes:
// 1. Put the safe enter and exit functions in the first, top-level
// function that enters our code.  Don't use them deep in the middle
// of our code.
//
// 2. Don't call MSG before hpcrun_safe_enter().
//
// 3. Check the return value of hpcrun_safe_enter() and don't call
// hpcrun_safe_exit() if safe enter returned false (unsafe).
//
// 4. In a sync override, it's ok to call safe exit and enter
// surrounding the call to the real function.  For example,
//
//    hpcrun_safe_exit();
//    ret = real_function(...);
//    hpcrun_safe_enter();
//
// 5. Be sure to #include <safe-sampling.h> in any file using these
// functions or else you'll get 'undefined reference' at runtime.
//
// 6. Technically, we don't need atomic test and set here.  There is
// no race condition between threads.  And if an interrupt happens
// inside hpcrun_safe_enter(), then the signal handler will restore
// inside_hpcrun to whatever it was when the interrupt happened.
//
// "Welcome to the Joe Hackett Flight School where our motto is,
// 'safety first, fun second.'" -- Joe Hackett in "Wings"
//
//***************************************************************************

#ifndef _HPCRUN_SAFE_SAMPLING_H_
#define _HPCRUN_SAFE_SAMPLING_H_

#include <hpcrun/main.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trampoline/common/trampoline.h>


// Use hpcrun_safe_enter() at entry into our code in a synchronous
// override.  If unsafe (false), then call the real function and
// return.
//
// A sync override can occur anywhere, including before hpcrun init.
// So, if uninit or if thread data is not set up, then assume we're in
// our init code and return unsafe.
//
// Returns: true if safe, ie, not already inside our code.
//
static inline int
hpcrun_safe_enter(void)
{
  thread_data_t *td;
  int prev;

  if (! hpcrun_is_initialized() || ! hpcrun_td_avail()) {
    return 0;
  }
  td = hpcrun_get_thread_data();
  prev = td->inside_hpcrun;
  td->inside_hpcrun = 1;

  return (prev == 0);
}


// Use hpcrun_safe_enter_async() at entry into our code in an async
// interrupt.  If unsafe (false), then restart the next interrupt and
// return.
//
// Use the program counter address (pc) to test if the interrupt came
// from inside the trampoline assembler code.
//
// Returns: true if safe, ie, not already inside our code.
//
static inline int
hpcrun_safe_enter_async(void *pc)
{
  thread_data_t *td;
  int prev;

  if (hpcrun_trampoline_interior(pc) || hpcrun_trampoline_at_entry(pc)
      || ! hpcrun_td_avail()) {
    return 0;
  }

  td = hpcrun_get_thread_data();
  prev = td->inside_hpcrun;
  td->inside_hpcrun = 1;

  return (prev == 0);
}


// Use hpcrun_safe_exit() at return from our code back to the
// application.  But call this only if the previous safe enter
// returned true.
//
static inline void
hpcrun_safe_exit(void)
{
  thread_data_t *td = hpcrun_get_thread_data();

  td->inside_hpcrun = 0;
}


#endif  // _HPCRUN_SAFE_SAMPLING_H_
