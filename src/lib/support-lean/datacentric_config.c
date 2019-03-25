// -*-Mode: C++;-*- // technically C99

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


//*********************************************************************
// system includes
//*********************************************************************

#include <stdlib.h>


//*********************************************************************
// local includes
//*********************************************************************

#include "datacentric_config.h"

//*********************************************************************
// macros
//*********************************************************************

#define MIN_BYTES_DEFAULT     1024
#define MIN_BYTES_INIT_VALUE  1

//*********************************************************************
// variables
//*********************************************************************


static const char* HPCRUN_DATACENTRIC_MIN_BYTES = "DATACENTRIC_MIN_BYTES";

//*********************************************************************
// Interfaces
//*********************************************************************


int env_get_datacentric_min_bytes()
{
  static int min_bytes = MIN_BYTES_INIT_VALUE;

  if (min_bytes > MIN_BYTES_INIT_VALUE)
    return min_bytes;

  char *option = getenv(HPCRUN_DATACENTRIC_MIN_BYTES);
  if (option != NULL) {
    min_bytes = atoi(option);
    if (min_bytes > MIN_BYTES_INIT_VALUE) {
      return min_bytes;
    }
  }
  min_bytes = MIN_BYTES_DEFAULT;
  return min_bytes;
}

