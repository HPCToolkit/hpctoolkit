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
