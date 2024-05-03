// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * system includes
 *****************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


//******************************************************************************
// local header
//******************************************************************************

#include "../utilities/line_wrapping.h"


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

static
char *
sanitize
(
  char *out,
  const char *in
)
{
        char *result = out;
        while (*in != 0) {
          // filter out unprintable characters
          if (isprint(*in)) *out++ = *in++;
          else in++;
        }
        *out = 0;
        return result;
}

static void
printw(FILE *output, const char *name, const char *desc_unsanitized)
{
  char **line;
  int *len;
  char sdesc[MAX_DESC_PER_LINE];

  int dlen;
  char *desc_buffer;

  if (desc_unsanitized) {
    dlen = strlen(desc_unsanitized);
    desc_buffer = (char *) malloc(dlen+1);
    char *desc = sanitize(desc_buffer, desc_unsanitized);

    int lines = strwrap((char *)desc, MAX_DESC_PER_LINE, &line, &len);
    if (lines == 0) {
      fprintf(output, "%s\n", name);
    } else {
      int i;
      for (i = 0; i < lines; i++) {
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
      free(line);
      free(len);
    }
    free(desc);
  } else {
    fprintf(output, "%s\n", name);
  }
}


//******************************************************************************
// Interfaces
//******************************************************************************

void
display_line_single(FILE *output)
{
  fprintf(output, "%s", line_single);
}

void
display_line_double(FILE *output)
{
  fprintf(output, "%s", line_double);
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
  fprintf(output, "%s", newline);
}
