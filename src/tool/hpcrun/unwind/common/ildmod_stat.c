/*
 * ildmod_stat.c
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "ildmod_stat.h"



//******************************************************************************
// String output
//******************************************************************************

void
load_module_tostr(void* lm, char str[])
{
  load_module_t* ldmod = (load_module_t*)lm;
  if (ldmod) {
	snprintf(str, LDMOD_NAME_LEN, "%s%s%d", ldmod->name, " ", ldmod->id);
  }
  else {
	snprintf(str, LDMOD_NAME_LEN, "%s", "nil");
  }
}

void
load_module_print(void* lm)
{
  char str[LDMOD_NAME_LEN];
  load_module_tostr(lm, str);
  printf("%s", str);
}

static char
ILdMod_Stat_MaxSpaces[] = "                                                                              ";

void
ildmod_stat_tostr(void* ilms, char str[])
{
  ildmod_stat_t* ildmod_stat = (ildmod_stat_t*)ilms;
  char intervalstr[MAX_INTERVAL_STR];
  interval_t_tostr(ildmod_stat->interval, intervalstr);
  char ldmodstr[LDMOD_NAME_LEN];
  load_module_tostr(ildmod_stat->loadmod, ldmodstr);
  char statstr[MAX_STAT_STR];
  treestat_tostr(atomic_load_explicit(&ildmod_stat->stat, memory_order_relaxed), statstr);
  sprintf(str, "(%s %s %s)", intervalstr, ldmodstr, statstr);
}

void
ildmod_stat_print(void* ilms)
{
  char str[MAX_ILDMODSTAT_STR];
  ildmod_stat_tostr(ilms, str);
  printf("%s", str);
}

void
treestat_tostr(tree_stat_t stat, char str[])
{
  switch (stat) {
  case NEVER: strcpy(str, "      NEVER"); break;
  case DEFERRED: strcpy(str, "   DEFERRED"); break;
  case FORTHCOMING: strcpy(str, "FORTHCOMING"); break;
  case READY: strcpy(str, "      READY"); break;
  default: strcpy(str, "STAT_ERROR");
  }
}

void
treestat_print(tree_stat_t ts)
{
  char statstr[MAX_STAT_STR];
  treestat_tostr(ts, statstr);
  printf("%s", statstr);
}

/*
 * the max spaces occupied by "([start_address ... end_address), load module xx, status)
 */
char*
ildmod_stat_maxspaces()
{
  return ILdMod_Stat_MaxSpaces;
}
