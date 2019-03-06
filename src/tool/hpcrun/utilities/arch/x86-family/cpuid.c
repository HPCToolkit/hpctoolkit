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

// Note:
// Some of the code is adapted from Linux Perf source code:
// https://github.com/torvalds/linux/blob/master/tools/perf/arch/x86/util/header.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <utilities/arch/cpuid.h>


static inline void
asm_cpuid(unsigned int op, unsigned int *a, unsigned int *b, unsigned int *c,
      unsigned int *d)
{
  // this assembly code is taken from Linux kernel
  // the code is gpl, so I assume it's okay to put it here
	__asm__ __volatile__ (".byte 0x53\n\tcpuid\n\t"
			      "movl %%ebx, %%esi\n\t.byte 0x5b"
			: "=a" (*a),
			"=S" (*b),
			"=c" (*c),
			"=d" (*d)
			: "a" (op));
}

/* return cpu type based on the cpu model        */
/* see https://en.wikichip.org/wiki/intel/cpuid  */
static cpu_type_t
get_intel_cpu_type(struct cpuid_type_s *cpuid)
{
  cpu_type_t type = CPU_UNSUP;

  if (cpuid->family == 6) {
    switch (cpuid->model) {

    case 26:
            type = INTEL_NHM_EP;
            break;
    case 42:
            type = INTEL_SNB;
            break;
    case 44:
            type = INTEL_WSM_EP;
            break;
    case 45:
            type = INTEL_SNB_EP;
            break;
    case 46:
            type = INTEL_NHM_EX;
            break;
    case 47:
            type = INTEL_WSM_EX;
            break;
    case 62:
            type = INTEL_IVB_EX;
            break;
    case 63:
            type = INTEL_HSX;
            break;
    case 79:
            type = INTEL_BDX;
            break;
    case 85:
            type = INTEL_SKX;
            break;
    case 87:
            type = INTEL_KNL;
            break;
    case 0x7e:
            type = INTEL_ICL;
            break;

    }
  }
  return type;
}

static cpu_type_t
get_amd_cpu_type(struct cpuid_type_s *cpuid)
{
  cpu_type_t type = CPU_UNSUP;

  if (cpuid->family == 16) {
    switch (cpuid->model) {
    case 9:
      type = AMD_MGN_CRS;
      break;
    }
  }
  return type;
}

static cpu_type_t
__get_cpuid(struct cpuid_type_s *cpuid)
{
	unsigned int a, b, c, d, lvl;

	asm_cpuid(0, &lvl, &b, &c, &d);

	strncpy(&cpuid->vendor[0], (char *)(&b), 4);
	strncpy(&cpuid->vendor[4], (char *)(&d), 4);
	strncpy(&cpuid->vendor[8], (char *)(&c), 4);

	cpuid->vendor[12] = '\0';

	if (lvl >= 1) {
	  asm_cpuid(1, &a, &b, &c, &d);

		cpuid->family = (a >> 8) & 0xf;  /* bits 11 - 8 */
		cpuid->model  = (a >> 4) & 0xf;  /* Bits  7 - 4 */
		cpuid->step   = a & 0xf;

		/* extended family */
		if (cpuid->family == 0xf)
		  cpuid->family += (a >> 20) & 0xff;

		/* extended model */
		if (cpuid->family >= 0x6)
		  cpuid->model += ((a >> 16) & 0xf) << 4;
	}

	/* look for end marker to ensure the entire data fit */
	if (strncmp(cpuid->vendor, VENDOR_INTEL, 12) == 0) {
	  // intel
		return get_intel_cpu_type(cpuid);
	} else if (strncmp(cpuid->vendor, VENDOR_AMD, 12) == 0) {
	  // amd
	  return get_amd_cpu_type(cpuid);
	}
	return CPU_UNSUP;
}



cpu_type_t
get_cpuid()
{
  struct cpuid_type_s cpuid;
  memset(&cpuid, 0, sizeof(cpuid));

	cpu_type_t type = __get_cpuid(&cpuid);
#if UNIT_TEST_CPUID
	printf("vendor:\t%s\nfamily:\t%d\nmodel:\t%d\nstep:\t%d\n", cpuid.vendor, cpuid.family, cpuid.model, cpuid.step);
#endif
	return type;
}


#if UNIT_TEST_CPUID

int main(void)
{
    cpu_type_t type = get_cpuid();
    printf("\ncpu type: %d\n", type);
}
#endif
