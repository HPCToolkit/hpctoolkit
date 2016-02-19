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

#if 0
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#endif

#include "monitor.h"

#include "general.h"
#include "interface.h"

#include "epoch.h"
#include "mem.h"

extern char *executable_name;

/* these symbols will be defined when the program is fully linked */ 
extern void _start(void);
extern int __stop___libc_freeres_ptrs;

void *static_epoch_offset = (void *)&_start;
void *static_epoch_end    = (void *)&__stop___libc_freeres_ptrs;
char *static_executable_name = NULL; 

void hpcrun_init_process(struct monitor_start_main_args *m)
{
  static_executable_name =  strdup((m->argv)[0]);
  hpcrun_init_internal();
}

void hpcrun_fini_process()
{
  hpcrun_fini_internal();
}

void hpcrun_epoch_get_loaded_modules(hpcrun_epoch_t *epoch,
				     hpcrun_epoch_t *previous_epoch)
{
  hpcrun_epoch_module_t *newmod;

  if (previous_epoch == NULL) {
    long static_epoch_size = 
      ((long) static_epoch_end) - ((long)static_epoch_offset);

    MSG(1,"synthesize initial epoch");
    newmod = hpcrun_malloc(sizeof(hpcrun_epoch_module_t));
    newmod->module_name = static_executable_name;
    newmod->mapaddr = static_epoch_offset;
    newmod->vaddr = NULL;
    newmod->size = static_epoch_size;

    newmod->next = epoch->loaded_modules;
    epoch->loaded_modules = newmod;
    epoch->num_modules++;
  } else epoch = previous_epoch;


  MSG(1,"epoch information: name = %s\n"
      "         mapaddr = %p\n"
      "         size    = %lx", 
      epoch->loaded_modules->module_name,
      epoch->loaded_modules->mapaddr,
      epoch->loaded_modules->size);

}
