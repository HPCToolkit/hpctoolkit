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
// Copyright ((c)) 2002-2020, Rice University
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
//   procmaps.c
//
// Purpose:
//   decode a line in Linux /proc/self/maps
//
//***************************************************************************


//******************************************************************************
// global includes
//******************************************************************************

#include <stdio.h>
#include <string.h>
#include <sys/param.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "procmaps.h"



//***************************************************************************
// type declarations
//***************************************************************************

typedef struct {
  const void *val;
  lm_seg_t *s;
} lm_seg_finder_t;



//***************************************************************************
// thread local data
//***************************************************************************

static __thread lm_seg_t lm_seg; 



//******************************************************************************
// private operations
//******************************************************************************

static int
lm_segment_permissions_decode
(
 char *perms
)
{
  int result = 0;
  while(*perms) {
    switch(*perms) {
    case 'r': result |= lm_perm_r; break;
    case 'w': result |= lm_perm_w; break;
    case 'x': result |= lm_perm_x; break;
    case 'p': result |= lm_perm_p; break;
    default: break;
    }
    perms++;
  }
  return result;
}


static int
lm_segment_find_by_addr_callback
(
 lm_seg_t *s,
 void *arg
)
{
  lm_seg_finder_t *sinfo = (lm_seg_finder_t *) arg;
  if (lm_segment_contains(s, sinfo->val)) {
    sinfo->s = s;
    return 1;
  }
  return 0;
}


static int
lm_segment_find_by_name_callback
(
 lm_seg_t *s,
 void *arg
)
{
  lm_seg_finder_t *sinfo = (lm_seg_finder_t *) arg;
  if (strcmp(s->path, (const char *) sinfo->val) == 0) {
    sinfo->s = s;
    return 1;
  }
  return 0;
}



//******************************************************************************
// interface operations
//******************************************************************************

int
lm_segment_contains
(
 lm_seg_t *s,
 const void *addr
)
{
  return (s->start_address <= addr) && (addr < s->end_address);
}


size_t
lm_segment_length
(
 lm_seg_t *s
)
{
  return ((long) s->end_address) - ((long) s->start_address);
}


void
lm_segment_parse
(
 lm_seg_t *s,
 const char *line
)
{
  char item[MAXPATHLEN];

  s->path[0] ='\0';

  sscanf(line, "%p-%p %s %lx %s %d %s", &s->start_address, &s->end_address,
	 item, &s->offset, s->device, &s->inode, s->path);

  s->permissions = lm_segment_permissions_decode(item);
}


void
lm_segment_iterate
(
 lm_callback_t lm_callback,
 void *arg
)
{
  char *line = NULL;
  size_t len = 0;

  FILE* loadmap = fopen("/proc/self/maps", "r");

  if (loadmap) {
    for(; getline(&line, &len, loadmap) != -1;) {
      lm_segment_parse(&lm_seg, line);
      if (lm_callback(&lm_seg, arg)) break;
    }
    fclose(loadmap);
  }
}


lm_seg_t *
lm_segment_find_by_addr
(
 void *addr
)
{
  lm_seg_finder_t info = { addr, 0 };
  lm_segment_iterate(lm_segment_find_by_addr_callback, &info);
  return info.s;
}


lm_seg_t *
lm_segment_find_by_name
(
 const char *name
)
{
  lm_seg_finder_t info = { name, 0 };
  lm_segment_iterate(lm_segment_find_by_name_callback, &info);
  return info.s;
}
