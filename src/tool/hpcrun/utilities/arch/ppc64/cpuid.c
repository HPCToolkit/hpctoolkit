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
// Copyright ((c)) 2002-2018, Rice University
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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define UNIT_TEST 1

#if UNIT_TEST
#include "../cpuid.h"
#else
#include <utilities/arch/cpuid.h>
#endif

#define MAX_CHAR_BUFFER 100

#define mfspr(rn)       ({unsigned long rval; \
			 asm volatile("mfspr %0,0x11F" : "=r" (rval)); rval; })

#define PVR_VER(pvr)    (((pvr) >>  16) & 0xFFFF) /* Version field */
#define PVR_REV(pvr)    (((pvr) >>   0) & 0xFFFF) /* Revison field */



cpu_type_t
get_cpuid()
{
  unsigned long pvr;
  unsigned int  ver;

  cpu_type_t  cpu_type = CPU_UNSUP;

  pvr = mfspr(SPRN_PVR);
  ver = PVR_VER(pvr);

  if (ver == 0) return CPU_UNSUP;

  switch(ver) {
    case 0x4e: 
         cpu_type = POWER9;
         break;
    case 0x4b: 
         cpu_type = POWER8;
         break;
    default: 
         cpu_type = POWER7;  /* FIXME : it can be error reading */
  }
  return cpu_type;
}


char *
get_cpuid_str()
{
	char *bufp;

	if (asprintf(&bufp, "%.8lx", mfspr(SPRN_PVR)) < 0)
		bufp = NULL;

	return bufp;
}

#if UNIT_TEST

int main()
{
  cpu_type_t type  = get_cpuid();
  char *cpu_string = get_cpuid_str();

  printf("cpu: %s,  type: %x\n", cpu_string, type);
}

#endif

