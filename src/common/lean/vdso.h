// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File:
//   vdso.c
//
// Purpose:
//   interface for information about VDSO segment in linux
//
// Description:
//   identify VDSO segment and its properties
//
//***************************************************************************

#ifndef __VDSO_H__
#define __VDSO_H__

#ifdef __cplusplus
extern "C" {
#endif

//***************************************************************************
// macros
//***************************************************************************

#define VDSO_SEGMENT_NAME_SHORT "[vdso]"
#define VDSO_SEGMENT_NAME_LONG  "linux-vdso.so"
#define VDSO_SUFFIX ".vdso"


//***************************************************************************
// interface declarations
//***************************************************************************

// returns non-zero value if segname is used for a VDSO segment
int
vdso_segment_p
(
 const char *filename
);


// returns address of VDSO segment
void *
vdso_segment_addr
(
);


// returns length of VDSO segment
size_t
vdso_segment_len
(
);

const char*
get_saved_vdso_path();

int
set_saved_vdso_path(const char*);


#ifdef __cplusplus
};
#endif

#endif
