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
