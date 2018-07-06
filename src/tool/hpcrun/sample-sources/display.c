// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


//******************************************************************************
// local header
//******************************************************************************

#include "line_wrapping.h"


//******************************************************************************
// Constants
//******************************************************************************

const int MAXBUF             = 1024;
const int MAX_DESC_PER_LINE  = 65;
const int MAX_EVENT_NAME     = 15;

static const char *line_double = "===========================================================================\n";
static const char *line_single = "---------------------------------------------------------------------------\n";

static const char *newline = "\n";

//******************************************************************************
// Local methods
//******************************************************************************

static void
printw(FILE *output, const char *name, const char *desc)
{
  char **line;
  int *len;
  char sdesc[MAX_DESC_PER_LINE];

  int lines = strwrap((char *)desc, MAX_DESC_PER_LINE, &line, &len);
  for (int i=0; i<lines; i++) {
    strncpy(sdesc, line[i], len[i]);
    sdesc[len[i]] = '\0';
    const char *name_ptr = " ";

    if (i == 0) {
      int len = strlen(name);
      if (len > MAX_EVENT_NAME) {
        fprintf(output, "%s\n", name);
      } else {
	name_ptr = name;
      }
    }
    fprintf(output, "%-*s %s\n", MAX_EVENT_NAME, name_ptr, sdesc);
  }
  free (line);
  free (len);
}


//******************************************************************************
// Interfaces
//******************************************************************************

void
display_line_single(FILE *output)
{
  fprintf(output, line_single);
}

void
display_line_double(FILE *output)
{
  fprintf(output, line_double);
}

void
display_header(FILE *output, const char *title)
{
  display_line_double(output);
  fprintf(output, "%s\n", title);
  display_line_double(output);
}

void
display_header_event(FILE *output)
{
  fprintf(output, "Name\t\tDescription\n");
  display_line_single(output);
}

void
display_event_info(FILE *output, const char *event, const char *desc)
{
  printw(output, event, desc);
  fprintf(output, newline);
}

