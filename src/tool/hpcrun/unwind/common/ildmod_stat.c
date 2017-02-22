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
// Constructors
//******************************************************************************

ildmod_stat_t*
ildmod_stat_new(interval_ldmod_pair_t *key,  tree_stat_t treestat,
	mem_alloc m_alloc)
{
  ildmod_stat_t* result = (ildmod_stat_t*)m_alloc(sizeof(ildmod_stat_t));
  result->ildmod = key;
  atomic_store_explicit(&result->stat, treestat, memory_order_relaxed);
  return result;
}

ildmod_stat_t*
ildmod_stat_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	tree_stat_t treestat, mem_alloc m_alloc)
{
  interval_ldmod_pair_t *ilm = interval_ldmod_pair_build(start, end,
	  ldmod, m_alloc);
  return ildmod_stat_new(ilm, treestat, m_alloc);
}

//******************************************************************************
// String output
//******************************************************************************

static char
ILdMod_Stat_MaxSpaces[] = "                                                                              ";

void
ildmod_stat_tostr(void* ilms, char str[])
{
  ildmod_stat_t* ildmod_stat = (ildmod_stat_t*)ilms;
  interval_ldmod_pair_tostr(ildmod_stat->ildmod, str);
  char statstr[MAX_STAT_STR];
  treestat_tostr(atomic_load_explicit(&ildmod_stat->stat, memory_order_relaxed), statstr);
  strcat(str, " ");
  strcat(str, statstr);
  strcat(str, ")");
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
