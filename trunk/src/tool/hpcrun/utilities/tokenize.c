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
extract_ev_thresh(const char *in, int evlen, char *ev, long *th)
{
  unsigned int len;

  char *dlm = strrchr(in,'@');
  if (dlm) {
    if (isdigit(dlm[1])){ // assume this is the threshold
      len = MIN(dlm-in,evlen);
      strncpy(ev,in,len);
      ev[len] = '\0';
    }
    else {
      dlm = NULL;
    }
  }
  if (!dlm) {
    len = strlen(in);
    strncpy(ev,in,len);
    ev[len] = '\0';
  }
  *th = dlm ? strtol(dlm+1,(char **)NULL,10) : DEFAULT_THRESHOLD;
}
