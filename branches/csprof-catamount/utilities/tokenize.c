// Simple utility for iterating the tokens out of a given string
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *tmp;
static char *tk;

char *
start_tok(char *lst)
{
  tmp = strdup(lst);
  tk  = strtok_r(tmp," ;");
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
  tk = strtok_r(NULL," ;");
  return tk;
}
