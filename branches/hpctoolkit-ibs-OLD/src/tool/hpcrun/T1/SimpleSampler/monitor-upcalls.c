// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include "monitor.h"

#include "sample_source.h"
#include "sample_handler.h"
#include "thread_data.h"

/*
 *  Callback functions for the client to override.
 */

void *
monitor_init_process(int *argc, char **argv, void *data)
{
  void *rv = alloc_thread_data(0);
  init_sample_source();
  start_sample_source();
  return rv;
}

void
monitor_fini_process(int how, void *data)
{
  stop_sample_source();
  shutdown_sample_source();
  sample_results();
  free_thread_data();
}

void 
monitor_init_thread_support(void)
{
  ;
}

void *
monitor_init_thread(int tid, void *data)
{
  void *rv = alloc_thread_data(tid);
  start_sample_source();
  return rv;
}

void 
monitor_fini_thread(void *data)
{
  stop_sample_source();
  sample_results();
  free_thread_data();
}
