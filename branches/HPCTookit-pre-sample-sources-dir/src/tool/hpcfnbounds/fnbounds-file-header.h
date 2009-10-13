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
// Copyright ((c)) 2002-2009, Rice University 
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

//
//

#ifndef _FNBOUNDS_FILE_HEADER_
#define _FNBOUNDS_FILE_HEADER_

//
// Printf format strings for fnbounds file names and extensions.
// The %s conversion args are directory and basename.
//
#define FNBOUNDS_BINARY_FORMAT  "%s/%s.fnbounds.bin"
#define FNBOUNDS_C_FORMAT       "%s/%s.fnbounds.c"
#define FNBOUNDS_TEXT_FORMAT    "%s/%s.fnbounds.txt"

//
// The extra info in the binary file of function addresses, written by
// hpcfnbounds-bin and read in the main process.  We call it "header",
// even though it's actually at the end of the file.
//
#define FNBOUNDS_MAGIC  0xf9f9f9f9

struct fnbounds_file_header {
    long zero_pad;
    long magic;
    long num_entries;
    int  relocatable;
};

#endif
