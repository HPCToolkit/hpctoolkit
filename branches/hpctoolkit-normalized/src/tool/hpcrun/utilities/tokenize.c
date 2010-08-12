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
// Copyright ((c)) 2002-2010, Rice University 
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

// Simple utility for iterating the tokens out of a given string
// FIXME: don't use strtok(), don't use single static buffer,
// factor out delimiters, etc.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokenize.h"

#define DEFAULT_THRESHOLD  1000000L
#define MIN(a,b)  (((a)<=(b))?(a):(b))

static char *tmp;
static char *tk;
static char *last;

static char *sep1 = " ,;";

char *
start_tok(char *lst)
{
  tmp = strdup(lst);
  tk  = strtok_r(tmp,sep1,&last);
  return tk;
}

int
more_tok(void)
{
  if (! tk){
    free(tmp);
  }
  return (tk != NULL);
}

char *
next_tok(void)
{
  tk = strtok_r(NULL,sep1,&last);
  return tk;
}

void
extract_ev_thresh_w_default(const char *in, int evlen, char *ev, long *th, long def)
{
  unsigned int len;

  char *dlm = strrchr(in, '@');
  if (!dlm) {
    dlm = strrchr(in, ':');
  }
  if (dlm) {
    if (isdigit(dlm[1])) { // assume this is the threshold
      len = MIN(dlm - in, evlen);
      strncpy(ev, in, len);
      ev[len] = '\0';
    }
    else {
      dlm = NULL;
    }
  }

  if (!dlm) {
    len = strlen(in);
    strncpy(ev, in, len);
    ev[len] = '\0';
  }
  
  *th = dlm ? strtol(dlm+1,(char **)NULL,10) : def;
}

void
extract_ev_thresh(const char *in, int evlen, char *ev, long *th)
{
  extract_ev_thresh_w_default(in, evlen, ev, th, DEFAULT_THRESHOLD);
}
