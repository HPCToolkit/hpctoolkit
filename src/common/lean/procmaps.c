// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

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
