// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2019, Rice University
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
//   src/tool/hpctracedump/main.cpp
//
// Purpose:
//   a program that dumps a trace file recorded by hpcrun
//
// Description:
//   driver program that uses library functions to read a trace file
//
//***************************************************************************

//***************************************************************************
// local include files
//***************************************************************************

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>



//***************************************************************************
// interface functions
//***************************************************************************

int
main(int argc, char **argv)
{
  int ret;
  if (argc < 2) {
    fprintf(stderr, "usage: %s <filename>\n", argv[0]);
    exit(-1);
  }
  char *fileName = argv[1];
  char* infsBuf = new char[HPCIO_RWBufferSz];

  FILE* infs = hpcio_fopen_r(fileName);

  if (!infs) {
    fprintf(stderr, "%s: error opening trace file %s\n", argv[0], fileName);
  }

  ret = setvbuf(infs, infsBuf, _IOFBF, HPCIO_RWBufferSz);

  if (ret != 0) {
    fprintf(stderr, "%s: setvbuf failed\n", argv[0]);
    exit(-1);
  }

  hpctrace_fmt_hdr_t hdr;

  ret = hpctrace_fmt_hdr_fread(&hdr, infs);

  if (ret != HPCFMT_OK) {
    fprintf(stderr, "%s: unable to read header for %s\n", argv[0], fileName);
    exit(-1);
  }

  // read and dump trace records until EOF 
  while ( !feof(infs) ) {
    hpctrace_fmt_datum_t datum;

    ret = hpctrace_fmt_datum_fread(&datum, hdr.flags, infs);

    if (ret == HPCFMT_EOF) {
      break;
    }
    else if (ret == HPCFMT_ERR) {
      fprintf(stderr, "%s: error reading trace file %s\n", argv[0], fileName);
      exit(-1);
    }

    printf("%d\n", datum.cpId);
  }

  hpcio_fclose(infs);

  delete[] infsBuf;

  return 0;
}
