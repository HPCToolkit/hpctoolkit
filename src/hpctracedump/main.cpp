// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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

#include "../common/lean/hpcfmt.h"
#include "../common/lean/hpcrun-fmt.h"



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
