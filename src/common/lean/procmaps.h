// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File:
//   procmaps.h
//
// Purpose:
//   interface for decoding a line in Linux /proc/self/maps
//
//***************************************************************************

#ifndef __PROCMAPS_H__
#define __PROCMAPS_H__

#ifdef __cplusplus
extern "C" {
#endif

//******************************************************************************
// global includes
//******************************************************************************

#include <sys/param.h>



//******************************************************************************
// types
//******************************************************************************

typedef enum {
  lm_perm_r = 1,
  lm_perm_w = 2,
  lm_perm_x = 4,
  lm_perm_p = 8
} lm_perm_t;


typedef struct lm_seg_s {
  void* start_address;      // segment start address
  void* end_address;        // segment end address
  int permissions;
  unsigned long offset;
  char device[32];          // major:minor
  int inode;                // inode of the file that backs the area
  char path[MAXPATHLEN];    // the path to the file associated with the segment
} lm_seg_t;


// callback returns 1 if iteration should be terminated
typedef int (lm_callback_t)(lm_seg_t *seg, void *arg);



//******************************************************************************
// interface operations
//******************************************************************************

int
lm_segment_contains
(
 lm_seg_t *s,
 const void *addr
);


size_t
lm_segment_length
(
 lm_seg_t *s
);


void
lm_segment_parse
(
 lm_seg_t *s,
 const char *line
);


void
lm_segment_iterate
(
 lm_callback_t lm_callback,
 void *arg                   // pointer to callback shared state
);


lm_seg_t *
lm_segment_find_by_addr
(
  void *addr
);


lm_seg_t *
lm_segment_find_by_name
(
  const char *name
);


#ifdef __cplusplus
};
#endif

#endif
