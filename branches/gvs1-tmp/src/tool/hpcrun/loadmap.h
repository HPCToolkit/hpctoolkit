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

/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
#ifndef LOADMAP_H
#define LOADMAP_H

#include <stdio.h>

/* an "loadmap" is an interval of time during which no two dynamic 
   libraries are mapped to the same region of the address space. 
   an loadmap can span across dlopen and dlclose operations. an loadmap
   ends when a dlopen maps a new load module on top of a region of 
   the address space that has previously been occupied by another
   module earlier during the loadmap.
*/

// Local includes

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

// FIXME:tallent: I moved the declaration of loadmap_src_t from
// hpcrun-fmt.h.  Most likely, the guts of loadmap_src_t should simply
// be replaced with loadmap_entry_t.

typedef struct loadmap_src_t {
  uint id;
  char* name;
  void* vaddr;
  void* mapaddr;
  size_t size;

  struct loadmap_src_t* next;

} loadmap_src_t;


typedef struct hpcrun_loadmap_t
{
  struct hpcrun_loadmap_t *next;  /* the next loadmap */
  unsigned int id;            /* an identifier for disk writeouts */
  unsigned int num_modules;   /* how many modules are loaded? */
  loadmap_src_t *loaded_modules;
} hpcrun_loadmap_t;

hpcrun_loadmap_t* hpcrun_static_loadmap(void);
hpcrun_loadmap_t* hpcrun_loadmap_new(void);
void            hpcrun_loadmap_init(hpcrun_loadmap_t* e);
hpcrun_loadmap_t* hpcrun_get_loadmap(void);

void hpcrun_loadmap_add_module(const char* module_name, void* vaddr, void* mapaddr, size_t size);

/* avoid weird dynamic loading conflicts */

void hpcrun_loadmap_lock();
void hpcrun_loadmap_unlock();
int  hpcrun_loadmap_is_locked();

void hpcrun_finalize_current_loadmap(void);

void hpcrun_write_all_loadmaps(FILE *);
void hpcrun_write_current_loadmap(FILE *);

#endif // LOADMAP_H
