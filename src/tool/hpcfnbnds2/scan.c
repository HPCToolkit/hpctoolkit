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
// This file contains the routines that scan instructions to identify functions

#include <fnbounds.h>

uint64_t
skipSectionScan(Elf *e, GElf_Shdr secHead, int secFlag)
{
size_t secHeadStringIndex;
char *secName;
Elf_Scn *section;

	elf_getshdrstrndx(e, &secHeadStringIndex);
	secName = elf_strptr(e, secHeadStringIndex, secHead.sh_name);

	if (secFlag == SC_SKIP) {
	    if(verbose) {
		fprintf (stderr, "Skipping scanning %s instructions\n",secName);
	    }
	    return SC_SKIP;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from scanning %s instructions\n",secName);
	    }
	}

	return SC_DONE;
}
// scan the .plt section
//
uint64_t
pltscan(Elf *e, GElf_Shdr secHead)
{
uint64_t ii;
uint64_t startAddr, endAddr, pltEntrySize;
char nameBuff[TB_SIZE];
char *vegamite;

	if (skipSectionScan(e, secHead, pltscan_f) == SC_SKIP) {
	    return SC_SKIP;
	}
	
    	startAddr = secHead.sh_addr;
	endAddr = startAddr + secHead.sh_size;
	pltEntrySize = secHead.sh_entsize;

	for (ii = startAddr + pltEntrySize; ii < endAddr; ii += pltEntrySize) {
	    sprintf(nameBuff,"stripped_0x%lx",ii);
	    vegamite = strdup(nameBuff);
	    add_function(ii, vegamite, "p",FR_YES);
	}

	return SC_DONE;
}

// scan the .init section
uint64_t
initscan(Elf *e, GElf_Shdr secHead)
{
	if (skipSectionScan(e, secHead, initscan_f) == SC_SKIP) {
	    return SC_SKIP;
	}
	
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .init instructions not yet implemented\n");
	}
	// use "i" as source string in add_function() calls
}

// scan the .text section
uint64_t
textscan(Elf *e, GElf_Shdr secHead)
{
	if (skipSectionScan(e, secHead, textscan_f) == SC_SKIP) {
	    return SC_SKIP;
	}
	
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .text instructions not yet implemented\n");
	}
	// use "t" as source string in add_function() calls
}

// scan the .fini section
uint64_t
finiscan(Elf *e, GElf_Shdr secHead)
{
	if (skipSectionScan(e, secHead, finiscan_f) == SC_SKIP) {
	    return SC_SKIP;
	}
	
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .fini instructions not yet implemented\n");
	}
	// use "f" as source string in add_function() calls
}

// scan the .fini section
uint64_t
altinstr_replacementscan(Elf *e, GElf_Shdr secHead)
{
	if (skipSectionScan(e, secHead, altinstr_replacementscan_f) == SC_SKIP) {
	    return SC_SKIP;
	}
	
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .altinstr_replacement instructions not yet implemented\n");
	}
	// use "a" as source string in add_function() calls
}
uint64_t
ehframescan(Elf *e, GElf_Shdr secHead)
{
	if (skipSectionScan(e, secHead, ehframeread_f) == SC_SKIP) {
	    return SC_SKIP;
	}
	
	// NOT YET IMPLEMENTED
	if (verbose) {
	    fprintf(stderr, "\tprocessing .eh_frame not yet implemented\n");
	}
	// use "e" as source string in add_function() calls
}

