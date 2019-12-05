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

// scan the .plt section
void
pltscan()
{
	if (pltscan_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping scanning .plt instructions\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from scanning .plt instructions\n");
	    }
	}
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .plt instructions not yet implemented\n");
	}
	// use "p" as source string in add_function() calls
}

// scan the .init section
void
initscan()
{
	if (initscan_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping scanning .init instructions\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from scanning .init instructions\n");
	    }
	}
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .init instructions not yet implemented\n");
	}
	// use "i" as source string in add_function() calls
}

// scan the .text section
void
textscan()
{
	if (textscan_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping scanning .text instructions\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from scanning .text instructions\n");
	    }
	}
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .text instructions not yet implemented\n");
	}
	// use "t" as source string in add_function() calls
}

// scan the .fini section
void
finiscan()
{
	if (finiscan_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping scanning .fini instructions\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from scanning .fini instructions\n");
	    }
	}
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .fini instructions not yet implemented\n");
	}
	// use "f" as source string in add_function() calls
}

// scan the .fini section
void
altinstr_replacementscan()
{
	if (altinstr_replacementscan_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping scanning .altinstr_replacement instructions\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from scanning .altinstr_replacement instructions\n");
	    }
	}
	// NOT YET IMPLEMENTED
	if(verbose) {
	    fprintf (stderr, "\tscanning .altinstr_replacement instructions not yet implemented\n");
	}
	// use "a" as source string in add_function() calls
}
