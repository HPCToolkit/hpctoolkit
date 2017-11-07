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
// Copyright ((c)) 2002-2017, Rice University
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



/******************************************************************************
 * segv signal handler
 *****************************************************************************/
// this segv handler is used to monitor first touches
static void
segv_handler (int signal_number, siginfo_t *si, void *context)
{
  int pagesize = getpagesize();
  if (TD_GET(inside_hpcrun) && si && si->si_addr) {
    void *p = (void *)(((uint64_t)(uintptr_t) si->si_addr) & ~(pagesize-1));
    mprotect (p, pagesize, PROT_READ | PROT_WRITE);
    return;
  }
  hpcrun_safe_enter();
  if (!si || !si->si_addr) {
    hpcrun_safe_exit();
    return;
  }
  void *start, *end;
  cct_node_t *data_node = splay_lookup((void *)si->si_addr, &start, &end);
  if (data_node) {
    void *p = (void *)(((uint64_t)(uintptr_t) start + pagesize-1) & ~(pagesize-1));
    mprotect (p, (uint64_t)(uintptr_t) end - (uint64_t)(uintptr_t) p, PROT_READ|PROT_WRITE);

    sampling_info_t info;
    struct datacentric_info_s data_aux = {.data_node = data_node, .first_touch = true};

    info.sample_clock = 0;
    info.sample_custom_cct.update_before_fn = hpcrun_cct_record_datacentric;
    info.sample_custom_cct.update_after_fn  = 0;
    info.sample_custom_cct.data_aux         = &data_aux;

    TMSG(DATACENTRIC, "found node %p, p: %p, context: %p", data_node, p, context);

    hpcrun_sample_callpath(context, alloc_metric_id,  (hpcrun_metricVal_t) {.i=1},
                            0/*skipInner*/, 0/*isSync*/, &info);
  }
  else {
    TMSG(DATACENTRIC, "NOT found node %p", context);
    void *p = (void *)(((uint64_t)(uintptr_t) si->si_addr) & ~(pagesize-1));
    mprotect (p, pagesize, PROT_READ | PROT_WRITE);
  }
  hpcrun_safe_exit();
  return;
}

static inline void
set_segv_handler()
{
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = segv_handler;
  sigaction(SIGSEGV, &sa, NULL);
}
