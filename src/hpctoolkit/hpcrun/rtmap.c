/* $Id$ */
/* -*-C-*- */

/****************************************************************************
//
// File: 
//    map.c
//
// Purpose:
//    Finds a list of loaded modules (e.g. DSOs) for the current process.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

/************************** System Include Files ****************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h> /* for 'PATH_MAX' */
#include <sys/types.h>

/**************************** User Include Files ****************************/

#include "map.h"

/**************************** Forward Declarations **************************/

#define TESTING 0

static loadmodules_t loadmodules;

static void finalizelines(void);
static int iscodeline(char *line);
static char* get_line_slot(void);

static char* name(char *line);
static char* offset(char *line);
static char* length(char *line);
static long htoll(char *s);

static void dumploadmodules(void);
static void dumplines(void);

/****************************************************************************/

/* number of bytes to print out for a long in hex with a 0x prefix */ 
#define bhl (sizeof(long)*2+2)

#define MAXLINELEN (PATH_MAX + 80)

/****************************************************************************/

/*
 *  Read lines from the load maps in /proc to find runtime libraries
 *  and load addresses.
 */
loadmodules_t* 
papirun_code_lines_from_loadmap(int dumpmap)
{
  char filename[PATH_MAX];
  FILE *pf;
  char *line = get_line_slot(); 
  pid_t pid = getpid();

  sprintf(filename,"/proc/%d/maps", pid);
  pf = fopen(filename,"r");

  /* read lines from the maps file in /proc for this
   * process. discard the line if it does not correspond
   * to an executable segment
   */
  do {
    if (iscodeline(line)) {
    	line = get_line_slot(); 
    }
  } while (fgets(line, MAXLINELEN, pf) != NULL);
  finalizelines();

  if (dumpmap) { 
    dumploadmodules(); 
    /* dumplines(); */
  }

  return &loadmodules;
}

/****************************************************************************/

static char **mappings = 0;
static int slots_avail = 0;
static int slots_in_use = 0;

/* allocate a slot for a new line in an 
 * extensible array of load map lines.
 */
static char*
get_line_slot(void) 
{
  if (slots_in_use == slots_avail) {
    if (mappings == 0) {
      slots_avail = 512;
      mappings = malloc(slots_avail * sizeof(char*));
    } else
      slots_avail += 256;
      mappings = realloc(mappings, slots_avail * sizeof(char));
  }
  mappings[slots_in_use] = (char *) malloc(MAXLINELEN);
  *mappings[slots_in_use] = (char) NULL;
  return mappings[slots_in_use++];
}

/* check if a line in a loadmap entry represents 
 * code by testing to see if execute permission
 * is set and file name has a path.
 */
static int 
iscodeline(char *line)
{
  char *blank = index(line, (int) ' ');
  char *slash = index(line, (int) '/');
  return (blank && *(blank + 3) == 'x' && slash);
}

/* discard the last line in the array of 
 * load map lines if it does not represent
 * executable code.
 */
static void 
finalizelines(void)
{
  int i;
  if (slots_in_use > 0) {
    int last = slots_in_use - 1;
    if (!iscodeline(mappings[last])) {
      free(mappings[last]);
      slots_in_use = last;
    }
  }
  
  loadmodules.module = 
    (loadmodule_t *) malloc(sizeof(loadmodule_t) * slots_in_use);
  
  for(i = 0; i < slots_in_use; i++) {
    loadmodules.module[i].name = strdup(name(mappings[i]));
    loadmodules.module[i].offset = htoll(offset(mappings[i]));
    loadmodules.module[i].length = htoll(length(mappings[i])) -
      loadmodules.module[i].offset;
  }
  loadmodules.count = slots_in_use;
#if TESTING
  dumploadmodules();
#endif
  
  mappings = realloc(mappings, (slots_in_use +1) * sizeof(char*));
  slots_avail = slots_in_use + 1;
}

static char*
name(char *line)
{
  char *name = index(line, (int) '/');
  char *newline = index(line, (int) '\n');
  if (newline) *newline = 0;
  return name;
}

static char*
offset(char *line)
{
  return line;
}

static char*
length(char *line)
{
  char *minus = index(line, (int) '-');
  return minus + 1;
}

static long 
htoll(char *s)
{
  unsigned long res = 0;
  sscanf(s,"%lx",&res);
  return res;
}

/*  Dump a processed form of the load map
 */
static void 
dumploadmodules(void)
{
  int i;
  fprintf(stderr, "Dumping currently mapped load modules:\n");
  for (i = 0; i < loadmodules.count; i++) {
    fprintf(stderr,"  offset=%#0*lx ", bhl,
	    loadmodules.module[i].offset);
    fprintf(stderr,"length=%#0*lx ", bhl,
	    loadmodules.module[i].length);
    fprintf(stderr,"name=%s\n", loadmodules.module[i].name);
  }
}

/*  Simply regurgitate the code lines of the load map
 */
static void 
dumplines(void)
{
  int i = 0;
  for (; i < slots_in_use; i++) {
    printf("%s\n",mappings[i]);
  }
}


#if TESTING
int 
main(void)
{
  papirun_code_lines_from_loadmap(1);
#if 0
  dumplines();
#endif
  return 0;
}
#endif

